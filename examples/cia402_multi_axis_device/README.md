# CiA 402 Multi-axis Product OD Reference

This directory describes the expected layout for a product-oriented multi-axis CiA 402 device description. It is intentionally separate from `examples/demo_device/`, which is the package bring-up Object Dictionary.

## Generated artifact set

`project.xdd` is the semantic source of truth. Product-generated outputs should come from the same XDD revision and generator version:

```text
project.xdd
OD.c
OD.h
project.eds
project.md
manifest.json
```

Do not hand-edit one generated output to repair a mismatch. A BSP or application integrates the generated `OD.c`/`OD.h`; product builds should keep `PKG_CANOPENNODE_USING_DEMO_OD` disabled.

The product reference should not depend on package demo-only manufacturer objects in `0x2300..0x23FF`. Device identity and normal CiA 402 operation should use the product's standard identity/profile objects and explicitly defined application objects.

## Version and identity policy

Keep these concepts separate:

- XDD `fileVersion` and EDS `FileVersion`: device-description version;
- OD `0x1018:03`: product/device revision;
- OD `0x100A`: firmware software version.

`manifest.json` may record the description version, generator, source revision, selected normative baseline, `0x1018` identity, and SHA-256 values for generated files. Copy `manifest.example.json` to `manifest.json` only when the product description and generated outputs are ready to be maintained together.

This repository snapshot intentionally does not fabricate product-generated files. Populate the directory from the actual product device description and identity data.
