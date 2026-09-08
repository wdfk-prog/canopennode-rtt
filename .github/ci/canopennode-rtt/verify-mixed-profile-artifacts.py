#!/usr/bin/env python3
"""Validate the checked-in mixed logical-device runtime/artifact layout."""

from __future__ import annotations

import argparse
import configparser
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

REQUIRED_ARTIFACTS = ("project.xdd", "project.eds", "OD.c", "OD.h")
PROFILE_OD_FIRST = 0x6000
PROFILE_OD_LAST = 0x9FFF
PROFILE_OD_STRIDE = 0x0800
PROFILE_DEVICE_TYPE_CANONICAL = 0x67FF
PDO_PER_LOGICAL_DEVICE = 64


def fail(message: str) -> None:
    raise ValueError(message)


def read_eds(path: Path) -> configparser.ConfigParser:
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    parser.optionxform = str
    try:
        with path.open("r", encoding="utf-8-sig") as stream:
            parser.read_file(stream)
    except (OSError, configparser.Error) as exc:
        raise ValueError(f"EDS: cannot parse {path}: {exc}") from exc
    return parser


def parse_int(value: str, label: str, base: int = 0) -> int:
    try:
        return int(value.strip(), base)
    except (AttributeError, ValueError) as exc:
        raise ValueError(f"{label}: invalid integer {value!r}") from exc


def macro_int(header: str, name: str) -> int:
    match = re.search(rf"^#define\s+{re.escape(name)}\s+([0-9]+)U?\s*$", header, re.MULTILINE)
    if not match:
        fail(f"runtime layout: missing integer macro {name}")
    return int(match.group(1), 10)


def runtime_descriptors(header_path: Path, source_path: Path) -> list[tuple[int, int]]:
    header = header_path.read_text(encoding="utf-8")
    source = source_path.read_text(encoding="utf-8")
    count = macro_int(header, "CO_PROFILE_MIXED_DEMO_DESCRIPTOR_COUNT")
    profile_macros = {
        name: macro_int(header, name)
        for name in (
            "CO_PROFILE_MIXED_DEMO_CIA402_PROFILE_NUMBER",
            "CO_PROFILE_MIXED_DEMO_CIA401_PROFILE_NUMBER",
        )
    }
    slot_macros = {
        name: macro_int(header, name)
        for name in (
            "CO_PROFILE_MIXED_DEMO_CIA402_AXIS0_LOGICAL_DEVICE",
            "CO_PROFILE_MIXED_DEMO_CIA402_AXIS1_LOGICAL_DEVICE",
            "CO_PROFILE_MIXED_DEMO_CIA402_AXIS2_LOGICAL_DEVICE",
            "CO_PROFILE_MIXED_DEMO_CIA401_LOGICAL_DEVICE",
        )
    }
    table = re.search(
        r"CO_profileMixedDemoDescriptors\s*\[[^\]]+\]\s*=\s*\{(?P<body>.*?)\n\};",
        source,
        re.DOTALL,
    )
    if not table:
        fail("runtime layout: descriptor table not found")
    entries = re.findall(r"\{\s*([A-Z0-9_]+)\s*,\s*([A-Z0-9_]+)\s*\}", table.group("body"))
    if len(entries) != count:
        fail(f"runtime layout: descriptor count is {len(entries)}, expected {count}")

    descriptors: list[tuple[int, int]] = []
    for slot_name, profile_name in entries:
        if slot_name not in slot_macros or profile_name not in profile_macros:
            fail(f"runtime layout: unresolved descriptor macros {slot_name}, {profile_name}")
        descriptors.append((slot_macros[slot_name], profile_macros[profile_name]))

    slots = [slot for slot, _ in descriptors]
    if any(slot < 0 or slot >= 8 for slot in slots):
        fail(f"runtime layout: logical-device slot out of range: {slots}")
    if len(set(slots)) != len(slots):
        fail(f"runtime layout: duplicate logical-device slot: {slots}")
    if any(profile <= 0 or profile > 0xFFFF for _, profile in descriptors):
        fail(f"runtime layout: invalid profile number: {descriptors}")
    return descriptors


def xdd_local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def xdd_indexes(root: ET.Element) -> set[int]:
    indexes: set[int] = set()
    for elem in root.iter():
        if xdd_local_name(elem.tag) == "CANopenObject" and elem.attrib.get("index"):
            indexes.add(parse_int(elem.attrib["index"], "XDD CANopenObject index", 16))
    return indexes


def xdd_default(root: ET.Element, unique_id: str) -> int:
    for elem in root.iter():
        if xdd_local_name(elem.tag) != "parameter" or elem.attrib.get("uniqueID") != unique_id:
            continue
        for child in elem.iter():
            if xdd_local_name(child.tag) == "defaultValue" and "value" in child.attrib:
                return parse_int(child.attrib["value"], f"XDD {unique_id} defaultValue")
    fail(f"XDD: missing default value for {unique_id}")


def od_c_index_sequence(text: str) -> list[int]:
    return [
        int(value, 16)
        for value in re.findall(r"\{\s*0x([0-9A-Fa-f]{4})\s*,\s*0x[0-9A-Fa-f]{2}\s*,\s*ODT_", text)
    ]


def od_c_indexes(text: str) -> set[int]:
    return set(od_c_index_sequence(text))


def od_h_indexes(text: str) -> set[int]:
    return {int(value, 16) for value in re.findall(r"OD_ENTRY_H([0-9A-Fa-f]{4})(?:_|\s)", text)}


def check_od_shortcuts(od_c: str, od_h: str) -> None:
    sequence = od_c_index_sequence(od_c)
    if not sequence:
        fail("OD.c: generated ODList is empty")
    if len(sequence) != len(set(sequence)):
        fail("OD.c: generated ODList contains duplicate indexes")
    if sequence != sorted(sequence):
        for position, (current, expected) in enumerate(zip(sequence, sorted(sequence))):
            if current != expected:
                fail(
                    f"OD.c: ODList is not ordered at list[{position}]: "
                    f"0x{current:04X} precedes expected 0x{expected:04X}"
                )
        fail("OD.c: generated ODList is not ordered")

    positions = {index: position for position, index in enumerate(sequence)}
    shortcut_re = re.compile(
        r"^#define\s+OD_ENTRY_H([0-9A-Fa-f]{4})(?:_[A-Za-z0-9_]+)?\s+&OD->list\[(\d+)\]$",
        re.MULTILINE,
    )
    shortcuts = shortcut_re.findall(od_h)
    if not shortcuts:
        fail("OD.h: generated OD_ENTRY shortcuts are missing")
    for index_text, position_text in shortcuts:
        index = int(index_text, 16)
        position = int(position_text, 10)
        if index not in positions:
            fail(f"OD.h: shortcut OD_ENTRY_H{index:04X} has no matching OD.c entry")
        if positions[index] != position:
            fail(
                f"OD.h: shortcut OD_ENTRY_H{index:04X} points to list[{position}], "
                f"expected list[{positions[index]}]"
            )


def eds_indexes(eds: configparser.ConfigParser) -> set[int]:
    return {int(section, 16) for section in eds.sections() if re.fullmatch(r"[0-9A-Fa-f]{4}", section)}


def generated_default(value: str, label: str) -> int:
    text = value.strip()
    if text.upper().startswith("$NODEID+"):
        text = text.split("+", 1)[1]
    return parse_int(text, label)


def eds_sub_defaults(eds: configparser.ConfigParser, index: int) -> dict[int, int]:
    prefix = f"{index:04X}sub"
    defaults: dict[int, int] = {}
    for section in eds.sections():
        if not section.lower().startswith(prefix.lower()):
            continue
        suffix = section[len(prefix):]
        if not re.fullmatch(r"[0-9A-Fa-f]+", suffix) or "DefaultValue" not in eds[section]:
            continue
        subindex = int(suffix, 16)
        defaults[subindex] = generated_default(
            eds[section]["DefaultValue"], f"EDS {section}/DefaultValue"
        )
    return defaults


def xdd_parameter_defaults(root: ET.Element) -> dict[str, str]:
    defaults: dict[str, str] = {}
    for elem in root.iter():
        if xdd_local_name(elem.tag) != "parameter" or "uniqueID" not in elem.attrib:
            continue
        for child in elem.iter():
            if xdd_local_name(child.tag) == "defaultValue" and "value" in child.attrib:
                defaults[elem.attrib["uniqueID"]] = child.attrib["value"]
                break
    return defaults


def xdd_sub_defaults(root: ET.Element, index: int, parameter_defaults: dict[str, str]) -> dict[int, int]:
    for elem in root.iter():
        if xdd_local_name(elem.tag) != "CANopenObject" or not elem.attrib.get("index"):
            continue
        if parse_int(elem.attrib["index"], "XDD CANopenObject index", 16) != index:
            continue
        defaults: dict[int, int] = {}
        for child in elem:
            if xdd_local_name(child.tag) != "CANopenSubObject":
                continue
            unique_id = child.attrib.get("uniqueIDRef")
            if not unique_id or unique_id not in parameter_defaults:
                fail(f"XDD 0x{index:04X}: missing default for sub-object {child.attrib.get('subIndex', '?')}")
            subindex = parse_int(child.attrib.get("subIndex", ""), f"XDD 0x{index:04X} subIndex", 16)
            defaults[subindex] = generated_default(
                parameter_defaults[unique_id], f"XDD {unique_id}/defaultValue"
            )
        return defaults
    fail(f"XDD: missing CANopenObject 0x{index:04X}")


def od_c_record_defaults(text: str, index: int) -> dict[int, int]:
    match = re.search(
        rf"\.x{index:04X}_[A-Za-z0-9_]+\s*=\s*\{{(?P<body>.*?)\n\s*\}},",
        text,
        re.DOTALL,
    )
    if not match:
        fail(f"OD.c: missing record initializer for 0x{index:04X}")

    fields = {
        name: parse_int(value, f"OD.c 0x{index:04X}/{name}")
        for name, value in re.findall(
            r"\.([A-Za-z0-9_]+)\s*=\s*(0x[0-9A-Fa-f]+|[0-9]+)", match.group("body")
        )
    }
    defaults: dict[int, int] = {}
    fixed = {
        "highestSub_indexSupported": 0,
        "COB_IDUsedByRPDO": 1,
        "COB_IDUsedByTPDO": 1,
        "transmissionType": 2,
        "inhibitTime": 3,
        "eventTimer": 5,
        "SYNCStartValue": 6,
        "numberOfMappedApplicationObjectsInPDO": 0,
    }
    for name, value in fields.items():
        if name in fixed:
            defaults[fixed[name]] = value
            continue
        application = re.fullmatch(r"applicationObject([1-8])", name)
        if application:
            defaults[int(application.group(1), 10)] = value
    if not defaults:
        fail(f"OD.c: no PDO defaults parsed for 0x{index:04X}")
    return defaults


def check_pdo_defaults(
    eds: configparser.ConfigParser, xdd_root: ET.Element, od_c: str, indexes: set[int]
) -> None:
    parameter_defaults = xdd_parameter_defaults(xdd_root)
    for index in sorted(indexes):
        eds_values = eds_sub_defaults(eds, index)
        xdd_values = xdd_sub_defaults(xdd_root, index, parameter_defaults)
        od_values = od_c_record_defaults(od_c, index)
        if eds_values != xdd_values:
            fail(
                f"PDO 0x{index:04X}: EDS/XDD defaults differ: "
                f"EDS={eds_values}, XDD={xdd_values}"
            )
        if eds_values != od_values:
            fail(
                f"PDO 0x{index:04X}: EDS/OD.c defaults differ: "
                f"EDS={eds_values}, OD.c={od_values}"
            )


def profile_indexes(indexes: set[int]) -> set[int]:
    return {index for index in indexes if PROFILE_OD_FIRST <= index <= PROFILE_OD_LAST}


def pdo_indexes(indexes: set[int]) -> set[int]:
    return {index for index in indexes if 0x1400 <= index <= 0x1BFF}


def require_equal(label: str, sets: dict[str, set[int]]) -> set[int]:
    names = list(sets)
    reference = sets[names[0]]
    for name in names[1:]:
        if sets[name] != reference:
            missing = sorted(reference - sets[name])
            extra = sorted(sets[name] - reference)
            fail(
                f"{label}: {name} differs from {names[0]}; "
                f"missing={[f'0x{v:04X}' for v in missing]}, extra={[f'0x{v:04X}' for v in extra]}"
            )
    return reference


def device_type_index(logical_device: int) -> int:
    return PROFILE_DEVICE_TYPE_CANONICAL + logical_device * PROFILE_OD_STRIDE


def eds_default(eds: configparser.ConfigParser, index: int) -> int:
    section = f"{index:04X}"
    if not eds.has_section(section) or "DefaultValue" not in eds[section]:
        fail(f"EDS: missing [{section}]/DefaultValue")
    return parse_int(eds[section]["DefaultValue"], f"EDS {section}/DefaultValue")


def od_c_default(text: str, index: int) -> int:
    match = re.search(rf"\.x{index:04X}_[A-Za-z0-9_]+\s*=\s*(0x[0-9A-Fa-f]+|[0-9]+)", text)
    if not match:
        fail(f"OD.c: missing initializer for 0x{index:04X}")
    return parse_int(match.group(1), f"OD.c 0x{index:04X} initializer")


def check_profile_layout(
    descriptors: list[tuple[int, int]],
    indexes: set[int],
    eds: configparser.ConfigParser,
    xdd_root: ET.Element,
    od_c: str,
) -> None:
    declared = {slot: profile for slot, profile in descriptors}
    for index in sorted(indexes):
        logical_device = (index - PROFILE_OD_FIRST) // PROFILE_OD_STRIDE
        if logical_device not in declared:
            fail(f"profile blocks: object 0x{index:04X} belongs to undeclared logical-device slot {logical_device}")

    for slot, profile in descriptors:
        block_first = PROFILE_OD_FIRST + slot * PROFILE_OD_STRIDE
        block_last = block_first + PROFILE_OD_STRIDE - 1
        if not any(block_first <= index <= block_last for index in indexes):
            fail(f"profile blocks: logical-device slot {slot} has no generated objects")

        type_index = device_type_index(slot)
        if type_index not in indexes:
            fail(f"Device Type: slot {slot} is missing 0x{type_index:04X}")
        values = {
            "EDS": eds_default(eds, type_index),
            "XDD": xdd_default(xdd_root, f"UID_OBJ_{type_index:04X}"),
            "OD.c": od_c_default(od_c, type_index),
        }
        if len(set(values.values())) != 1:
            fail(f"Device Type 0x{type_index:04X}: generated values differ: {values}")
        value = next(iter(values.values()))
        if value & 0xFFFF != profile:
            fail(
                f"Device Type 0x{type_index:04X}: low 16-bit profile is {value & 0xFFFF}, "
                f"runtime descriptor declares {profile}"
            )


def mapping_values(eds: configparser.ConfigParser, mapping_index: int) -> list[int]:
    prefix = f"{mapping_index:04X}sub"
    values: list[int] = []
    for section in eds.sections():
        if not section.lower().startswith(prefix.lower()) or section.lower() == f"{prefix}0".lower():
            continue
        if "DefaultValue" not in eds[section]:
            continue
        value = parse_int(eds[section]["DefaultValue"], f"EDS {section}/DefaultValue")
        if value != 0:
            values.append(value)
    return values


def check_pdo_ownership(eds: configparser.ConfigParser, all_indexes: set[int], declared_slots: set[int]) -> int:
    checked = 0
    for mapping_index in sorted(index for index in all_indexes if 0x1600 <= index <= 0x17FF or 0x1A00 <= index <= 0x1BFF):
        mapped_indexes = [(value >> 16) & 0xFFFF for value in mapping_values(eds, mapping_index)]
        profile_mapped = [index for index in mapped_indexes if PROFILE_OD_FIRST <= index <= PROFILE_OD_LAST]
        if not profile_mapped:
            continue

        mapped_slots = {(index - PROFILE_OD_FIRST) // PROFILE_OD_STRIDE for index in profile_mapped}
        if len(mapped_slots) != 1:
            fail(
                f"PDO mapping 0x{mapping_index:04X}: maps multiple logical-device slots "
                f"{sorted(mapped_slots)}"
            )
        mapped_slot = next(iter(mapped_slots))
        if mapped_slot not in declared_slots:
            fail(f"PDO mapping 0x{mapping_index:04X}: maps undeclared logical-device slot {mapped_slot}")

        if 0x1600 <= mapping_index <= 0x17FF:
            pdo_number = mapping_index - 0x1600 + 1
            communication_index = 0x1400 + pdo_number - 1
        else:
            pdo_number = mapping_index - 0x1A00 + 1
            communication_index = 0x1800 + pdo_number - 1
        pdo_slot = (pdo_number - 1) // PDO_PER_LOGICAL_DEVICE
        if pdo_slot != mapped_slot:
            fail(
                f"PDO mapping 0x{mapping_index:04X}: PDO{pdo_number} belongs to logical-device slot "
                f"{pdo_slot}, but mapped profile objects belong to slot {mapped_slot}"
            )
        if communication_index not in all_indexes:
            fail(
                f"PDO mapping 0x{mapping_index:04X}: missing matching communication object "
                f"0x{communication_index:04X}"
            )
        checked += 1
    return checked


def verify(artifact_dir: Path, runtime_header: Path, runtime_source: Path) -> tuple[int, int, int]:
    for name in REQUIRED_ARTIFACTS:
        if not (artifact_dir / name).is_file():
            fail(f"artifact: missing required file {name}")
    if not runtime_header.is_file() or not runtime_source.is_file():
        fail("runtime layout: canonical mixed descriptor header/source is missing")

    descriptors = runtime_descriptors(runtime_header, runtime_source)
    eds = read_eds(artifact_dir / "project.eds")
    try:
        xdd_root = ET.parse(artifact_dir / "project.xdd").getroot()
    except (OSError, ET.ParseError) as exc:
        raise ValueError(f"XDD: cannot parse project.xdd: {exc}") from exc
    od_c = (artifact_dir / "OD.c").read_text(encoding="utf-8")
    od_h = (artifact_dir / "OD.h").read_text(encoding="utf-8")

    check_od_shortcuts(od_c, od_h)
    all_sets = {
        "EDS": eds_indexes(eds),
        "XDD": xdd_indexes(xdd_root),
        "OD.c": od_c_indexes(od_c),
        "OD.h": od_h_indexes(od_h),
    }
    profile_set = require_equal("profile object set", {name: profile_indexes(values) for name, values in all_sets.items()})
    pdo_set = require_equal("PDO object set", {name: pdo_indexes(values) for name, values in all_sets.items()})
    check_profile_layout(descriptors, profile_set, eds, xdd_root, od_c)
    check_pdo_defaults(eds, xdd_root, od_c, pdo_set)
    checked_pdos = check_pdo_ownership(eds, pdo_set, {slot for slot, _ in descriptors})
    if checked_pdos == 0:
        fail("PDO ownership: no profile-mapped PDOs were found")
    return len(descriptors), len(profile_set), checked_pdos


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("artifact_dir", type=Path, help="directory containing project.xdd/project.eds/OD.c/OD.h")
    parser.add_argument("runtime_header", type=Path, help="canonical CO_profile_mixed_demo.h")
    parser.add_argument("runtime_source", type=Path, help="canonical CO_profile_mixed_demo.c")
    args = parser.parse_args()
    try:
        descriptor_count, profile_count, pdo_count = verify(
            args.artifact_dir, args.runtime_header, args.runtime_source
        )
    except ValueError as exc:
        print(f"MIXED_PROFILE_ARTIFACT_FAIL: {exc}", file=sys.stderr)
        return 1
    print(
        "MIXED_PROFILE_ARTIFACT_PASS: "
        f"descriptors={descriptor_count} profile_objects={profile_count} profile_pdos={pdo_count}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
