# CiA 402 Multi-axis Product Reference

This directory is the staging contract for a product-style CiA 402 device description. It is deliberately separate from `examples/demo_device`, which contains package bring-up and test-only manufacturer objects.

## Release artifact set

`project.xdd` is the source of truth. For a release candidate, generate all of the following from the same XDD revision with the selected CANopenEditor version:

```text
project.xdd
OD.c
OD.h
project.eds
project.md
manifest.json
```

Do not hand-edit one generated output to fix a mismatch. A BSP or application integrates the generated `OD.c`/`OD.h`; the package must not enable `PKG_CANOPENNODE_USING_DEMO_OD` for a product-reference build.

The product reference must not contain the demo/test-only `0x2300..0x23FF` objects. Device recognition and normal CiA 402 control must use standard identity/profile information, not a private magic index.

## Version and identity policy

Keep these concepts separate:

- XDD `fileVersion` and EDS `FileVersion`: device-description version;
- OD `0x1018:03`: product/device revision;
- OD `0x100A`: firmware software version.

`manifest.json` records the description version, generator, source revision, selected normative baseline, 0x1018 identity, and SHA-256 of every generated output. Copy `manifest.example.json` to `manifest.json`, replace every placeholder, then calculate hashes only after the generated files are frozen.

## Static release check

From the repository root:

```sh
python3 .github/ci/canopennode-rtt/verify-cia402-artifacts.py examples/cia402_multi_axis_device
```

The checker verifies hashes, description version, 0x1018 identity, the three logical-device object matrix, Error-code object attributes, and the absence of `0x23xx` test objects. This is static artifact evidence only; it does not prove CANopenEditor regeneration, target firmware identity, CAN timing, or master interoperability.

This repository snapshot intentionally does not include fabricated product generated files. Populate this directory only after product identity, fault taxonomy, selected CiA 301/402 baseline, and the actual A7 source revision are frozen.
