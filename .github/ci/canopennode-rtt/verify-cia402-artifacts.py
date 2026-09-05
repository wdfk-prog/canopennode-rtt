#!/usr/bin/env python3
"""Validate a released CiA 402 product-description artifact set.

The checker is intentionally generator-independent. It verifies files produced
from one XDD revision; it does not claim that CANopenEditor generation itself is
reproducible in CI.
"""

from __future__ import annotations

import argparse
import configparser
import hashlib
import json
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

REQUIRED_FILES = ("project.xdd", "project.eds", "OD.c", "OD.h", "project.md")
BASE_OBJECTS = (0x603F, 0x6040, 0x6041, 0x6060, 0x6061, 0x6064, 0x606C, 0x6071, 0x6077, 0x607A, 0x60FF, 0x6502)
AXIS_OFFSETS = (0x0000, 0x0800, 0x1000)
TEST_ONLY_MIN = 0x2300
TEST_ONLY_MAX = 0x23FF


def fail(message: str) -> None:
    raise ValueError(message)


def parse_int(value: object, field: str) -> int:
    if isinstance(value, int):
        return value
    if not isinstance(value, str) or not value.strip():
        fail(f"{field}: expected integer or non-empty integer string")
    try:
        return int(value.strip(), 0)
    except ValueError as exc:
        raise ValueError(f"{field}: invalid integer {value!r}") from exc


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_manifest(path: Path) -> dict:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError(f"manifest: cannot read {path}: {exc}") from exc
    if not isinstance(data, dict) or data.get("schema") != 1:
        fail("manifest: schema must be integer 1")
    for key in ("description_version", "generator", "source_revision"):
        value = data.get(key)
        if not isinstance(value, str) or not value.strip() or "<" in value:
            fail(f"manifest: {key} must be a resolved non-placeholder string")
    if not re.fullmatch(r"[0-9a-fA-F]{7,64}", data["source_revision"].strip()):
        fail("manifest: source_revision must be an abbreviated or full hexadecimal Git commit ID")
    baseline = data.get("normative_baseline")
    if not isinstance(baseline, list) or not baseline or not all(isinstance(v, str) and v.strip() and "<" not in v for v in baseline):
        fail("manifest: normative_baseline must contain resolved specification identifiers")
    identity = data.get("identity")
    if not isinstance(identity, dict):
        fail("manifest: identity must be an object")
    for key in ("vendor_id", "product_code", "revision_number"):
        parse_int(identity.get(key), f"manifest.identity.{key}")
    files = data.get("files")
    if not isinstance(files, dict):
        fail("manifest: files must be an object")
    for name in REQUIRED_FILES:
        value = files.get(name)
        if not isinstance(value, str) or not re.fullmatch(r"sha256:[0-9a-fA-F]{64}", value):
            fail(f"manifest: files.{name} must be sha256:<64 hex>")
    return data


def read_eds(path: Path) -> configparser.ConfigParser:
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    parser.optionxform = str
    try:
        with path.open("r", encoding="utf-8-sig") as stream:
            parser.read_file(stream)
    except (OSError, configparser.Error) as exc:
        raise ValueError(f"EDS: cannot parse {path}: {exc}") from exc
    return parser


def eds_get(parser: configparser.ConfigParser, section: str, key: str) -> str:
    if not parser.has_section(section) or key not in parser[section]:
        fail(f"EDS: missing {section}/{key}")
    return parser[section][key].strip()


def xdd_local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def xdd_default(root: ET.Element, unique_id: str) -> int:
    for elem in root.iter():
        if xdd_local_name(elem.tag) != "parameter" or elem.attrib.get("uniqueID") != unique_id:
            continue
        for child in elem.iter():
            if xdd_local_name(child.tag) == "defaultValue" and "value" in child.attrib:
                return parse_int(child.attrib["value"], f"XDD {unique_id} defaultValue")
    fail(f"XDD: missing default value for {unique_id}")


def xdd_version(root: ET.Element) -> str:
    versions = {
        elem.attrib["fileVersion"].strip()
        for elem in root.iter()
        if xdd_local_name(elem.tag) == "ProfileBody" and elem.attrib.get("fileVersion")
    }
    if len(versions) != 1:
        fail(f"XDD: expected one fileVersion, got {sorted(versions)}")
    return versions.pop()


def xdd_indexes(root: ET.Element) -> set[int]:
    result = set()
    for elem in root.iter():
        if xdd_local_name(elem.tag) != "CANopenObject":
            continue
        raw = elem.attrib.get("index")
        if raw:
            try:
                result.add(int(raw, 16))
            except ValueError as exc:
                raise ValueError(f"XDD: invalid CANopenObject index {raw!r}") from exc
    return result


def od_c_indexes(text: str) -> set[int]:
    # Parse only generated OD list entries; arbitrary 16-bit default values are not object indexes.
    return {
        int(value, 16)
        for value in re.findall(r"\{\s*0x([0-9A-Fa-f]{4})\s*,\s*0x[0-9A-Fa-f]{2}\s*,\s*ODT_", text)
    }


def od_h_indexes(text: str) -> set[int]:
    return {int(value, 16) for value in re.findall(r"OD_ENTRY_H([0-9A-Fa-f]{4})(?:_|\s)", text)}


def require_release_matrix(indexes: set[int], source: str) -> None:
    required = {base + offset for offset in AXIS_OFFSETS for base in BASE_OBJECTS}
    missing = sorted(required - indexes)
    if missing:
        fail(f"{source}: missing CiA 402 release objects: " + ", ".join(f"0x{idx:04X}" for idx in missing))
    private = sorted(idx for idx in indexes if TEST_ONLY_MIN <= idx <= TEST_ONLY_MAX)
    if private:
        fail(f"{source}: product artifact contains test-only 0x23xx objects: " + ", ".join(f"0x{idx:04X}" for idx in private))


def check_error_objects(eds: configparser.ConfigParser) -> None:
    for index in (0x603F, 0x683F, 0x703F):
        section = f"{index:04X}"
        if eds_get(eds, section, "DataType").lower() != "0x0006":
            fail(f"EDS: [{section}] Error code must be UNSIGNED16 (0x0006)")
        if eds_get(eds, section, "AccessType").lower() != "ro":
            fail(f"EDS: [{section}] Error code must be read-only")
        if parse_int(eds_get(eds, section, "PDOMapping"), f"EDS {section}/PDOMapping") != 0:
            fail(f"EDS: [{section}] Error code must not be PDO-mappable in the release reference")


def check_generated_identity(od_c: str, expected: tuple[int, int, int]) -> None:
    patterns = (
        r"\.vendor_ID\s*=\s*(0x[0-9A-Fa-f]+|\d+)",
        r"\.productCode\s*=\s*(0x[0-9A-Fa-f]+|\d+)",
        r"\.revisionNumber\s*=\s*(0x[0-9A-Fa-f]+|\d+)",
    )
    actual = []
    for pattern in patterns:
        match = re.search(pattern, od_c)
        if not match:
            fail(f"OD.c: identity initializer not found for pattern {pattern}")
        actual.append(int(match.group(1), 0))
    if tuple(actual) != expected:
        fail(f"OD.c: identity {tuple(hex(v) for v in actual)} does not match manifest/EDS {tuple(hex(v) for v in expected)}")


def verify(directory: Path) -> None:
    manifest_path = directory / "manifest.json"
    manifest = load_manifest(manifest_path)

    for name in REQUIRED_FILES:
        path = directory / name
        if not path.is_file():
            fail(f"artifact: missing required file {name}")
        expected = manifest["files"][name].split(":", 1)[1].lower()
        actual = sha256(path)
        if actual != expected:
            fail(f"artifact: SHA-256 mismatch for {name}: expected {expected}, got {actual}")

    eds = read_eds(directory / "project.eds")
    try:
        xdd_root = ET.parse(directory / "project.xdd").getroot()
    except (OSError, ET.ParseError) as exc:
        raise ValueError(f"XDD: cannot parse project.xdd: {exc}") from exc

    description_version = manifest["description_version"].strip()
    if eds_get(eds, "FileInfo", "FileVersion") != description_version:
        fail("version: manifest description_version does not match EDS FileVersion")
    if xdd_version(xdd_root) != description_version:
        fail("version: manifest description_version does not match XDD fileVersion")

    identity = manifest["identity"]
    expected_identity = (
        parse_int(identity["vendor_id"], "manifest.identity.vendor_id"),
        parse_int(identity["product_code"], "manifest.identity.product_code"),
        parse_int(identity["revision_number"], "manifest.identity.revision_number"),
    )
    eds_identity = (
        parse_int(eds_get(eds, "1018sub1", "DefaultValue"), "EDS 1018sub1/DefaultValue"),
        parse_int(eds_get(eds, "1018sub2", "DefaultValue"), "EDS 1018sub2/DefaultValue"),
        parse_int(eds_get(eds, "1018sub3", "DefaultValue"), "EDS 1018sub3/DefaultValue"),
    )
    xdd_identity = (
        xdd_default(xdd_root, "UID_SUB_101801"),
        xdd_default(xdd_root, "UID_SUB_101802"),
        xdd_default(xdd_root, "UID_SUB_101803"),
    )
    if eds_identity != expected_identity or xdd_identity != expected_identity:
        fail("identity: manifest, EDS 0x1018, and XDD 0x1018 values differ")

    od_c = (directory / "OD.c").read_text(encoding="utf-8", errors="strict")
    od_h = (directory / "OD.h").read_text(encoding="utf-8", errors="strict")
    check_generated_identity(od_c, expected_identity)

    eds_indexes = {int(section, 16) for section in eds.sections() if re.fullmatch(r"[0-9A-Fa-f]{4}", section)}
    require_release_matrix(eds_indexes, "EDS")
    require_release_matrix(xdd_indexes(xdd_root), "XDD")
    require_release_matrix(od_c_indexes(od_c), "OD.c")
    require_release_matrix(od_h_indexes(od_h), "OD.h")
    check_error_objects(eds)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("artifact_dir", type=Path, help="directory containing manifest.json and generated product artifacts")
    args = parser.parse_args()
    try:
        verify(args.artifact_dir)
    except ValueError as exc:
        print(f"CIA402_ARTIFACT_FAIL: {exc}", file=sys.stderr)
        return 1
    print(f"CIA402_ARTIFACT_PASS: {args.artifact_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
