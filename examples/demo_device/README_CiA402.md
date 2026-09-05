# CiA 402 Demo-device Object Dictionary

`project.xdd` is the source of truth for the package demo device. It contains the existing demo objects plus three CiA 402 logical-device application blocks. `OD.c`, `OD.h`, `project.eds`, and `project.md` are generated outputs and should be regenerated from `project.xdd` with the selected CANopenEditor version when the dictionary changes.

## Reusable insertion profile

`CiA402_profile.xpd` is an authoring helper, not a second device description. It contains the Axis0 CiA 402 application objects in the `0x6000..0x67FF` logical-device range, including Controlword, Statusword, mode objects, position/velocity/torque values, profile parameters, homing parameters, and Supported drive modes.

CANopenEditor can load the file through `Insert Profile -> Open Profile File...`. For a clean project, insert the profile with index offsets `0 2048 4096` to create the first three logical-device blocks at `0x6000`, `0x6800`, and `0x7000`.

Do not insert the profile again into this repository's `project.xdd`; those objects are already present and duplicate insertion causes index collisions.

## PDO ownership

The reusable profile intentionally excludes `0x1400/0x1600/0x1800/0x1A00` communication and mapping objects. The `0x0800` logical-device offset applies to the device-profile application range, while PDO communication and mapping objects follow CANopen communication-profile rules and must be defined by the final device dictionary.

The checked-in `project.xdd` includes an engineering PDO example for Axis0/1/2. Mapping entries are populated in `0x1601..0x1603` and `0x1A01..0x1A03`, while the corresponding `0x1401..0x1403` and `0x1801..0x1803` communication objects remain disabled by default. This preserves the original demo PDO0 bus behavior until the application explicitly enables the additional CiA 402 PDOs.

A product should review the final PDO numbering, communication parameters, and mapping against its selected CiA 301/CiA 402 baseline before adopting the demo layout.

## Regenerating the dictionary

When the XDD changes:

1. Open `project.xdd` with the project's selected CANopenEditor version.
2. Resolve device-description warnings and review RPDO/TPDO mappings.
3. Export CANopenNode V4 `OD.c` and `OD.h`.
4. Export EDS and Markdown from the same `project.xdd` revision.
5. Review the generated diff and replace the checked-in generated artifacts together.

Do not hand-edit generated `OD.c` or `OD.h` to add or remove CiA 402 objects.

## Cyclic synchronous demo logging

When CSP/CSV/CST is enabled, the package demo provides a software `SyncIF` for all configured logical devices. `PKG_CANOPENNODE_CIA402_DEMO_SYNC_LOG` prints command/feedback snapshots from the lower-priority `co_402` worker for bring-up diagnostics.

If `co_402` lags `co_rt`, intermediate SYNC generations may be coalesced. The log is therefore a software snapshot stream rather than a one-record-per-SYNC trace. The synchronous `co_rt` callback itself performs only the bounded handoff and does not print.

The checked-in RPDO1/2/3 example maps Controlword, Modes of operation, and Target position and remains disabled by default. Applications using CSV or CST should map the selected axis PDO to Target velocity (`0x60FF`) or Target torque (`0x6071`) as required by their product dictionary.
