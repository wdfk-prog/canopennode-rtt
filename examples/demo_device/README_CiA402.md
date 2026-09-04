# CiA 402 demo-device Object Dictionary

`project.xdd` is the single source of truth for the demo device. It contains the
existing demo objects plus three CiA 402 logical-device application blocks.
`OD.c`, `OD.h`, `project.eds`, and `project.md` are derived artifacts and must be
regenerated from `project.xdd` with CANopenEditor v4.2.3 before release.

## Reusable insertion profile

`CiA402_profile.xpd` is an authoring helper, not a second device description. It
contains only the Axis0 CiA 402 application objects in the `0x6000..0x67FF`
logical-device range:

- `0x603F` Error code
- `0x6040` Controlword
- `0x6041` Statusword
- `0x6060` Modes of operation
- `0x6061` Modes of operation display
- `0x6064` Position actual value
- `0x606C` Velocity actual value
- `0x6071` Target torque
- `0x6077` Torque actual value
- `0x607A` Target position
- `0x607C` Home offset
- `0x6081` Profile velocity
- `0x6083` Profile acceleration
- `0x6084` Profile deceleration
- `0x6085` Quick stop deceleration
- `0x6086` Motion profile type
- `0x6098` Homing method
- `0x6099` Homing speeds
- `0x609A` Homing acceleration
- `0x60FF` Target velocity
- `0x6502` Supported drive modes

CANopenEditor v4.2.3 can load the file with `Insert Profile -> Open Profile
File...`. For a clean project, insert the profile with index offsets `0 2048
4096` to create the first three logical-device blocks at `0x6000`, `0x6800`,
and `0x7000`.

Do not insert the profile again into this repository's `project.xdd`; those
objects are already present and the editor will report index collisions.

## PDO ownership

The reusable profile intentionally does not contain `0x1400/0x1600/0x1800/
0x1A00` communication and mapping objects. The `0x0800` logical-device offset
applies to the device-profile application range, while PDO numbering for later
logical devices follows CANopen communication-profile rules and must be designed
in the final project dictionary. Keep PDO communication/mapping configuration in
`project.xdd` and validate it against the selected CiA 301/CiA 402 normative
matrix.

The current PDO mapping in `project.xdd` is an engineering example. Mapping
entries for Axis0/1/2 are pre-filled in `0x1601..0x1603` and
`0x1A01..0x1A03`, but their matching `0x1401..0x1403` and
`0x1801..0x1803` communication objects remain disabled by default. This keeps
the original demo's PDO0 bus behavior unchanged until a later integration stage
explicitly enables the CiA 402 PDOs.

These sequential PDO slots are an engineering example, not CiA 402 conformance
evidence. Validate the final PDO numbering and mapping against the
project-selected CiA 301/CiA 402-3 normative matrix before enabling them.

## Regeneration gate

Before release:

1. Open `project.xdd` with CANopenEditor v4.2.3.
2. Resolve all editor warnings and verify RPDO/TPDO mappings.
3. Export CANopenNode V4 `OD.c` and `OD.h` to a staging directory.
4. Export EDS and Markdown from the same `project.xdd` revision.
5. Review the generated diff, then replace the checked-in generated artifacts.

Do not hand-edit generated `OD.c` or `OD.h` to add or remove CiA 402 objects.

## Cyclic synchronous demo observability

When CSP/CSV/CST is enabled, the package demo provides a software `SyncIF` for
all three logical devices. Enable `PKG_CANOPENNODE_CIA402_DEMO_SYNC_LOG` only
for protocol bring-up to print command/feedback snapshots from the lower-priority
`co_402` worker. If that worker lags `co_rt`, intermediate SYNC generations may be
coalesced; the log is observability, not a one-record-per-SYNC trace. The synchronous
`co_rt` callback itself does not log; it only performs the bounded handoff.

A successful sample has the form `axis=0 SYNC seq=42 mode=8 ... pub=1 ...
fresh=1 follow=1`. `pub=0` shows command rejection by the endpoint, while `fresh=0`
shows missing or stale feedback for that generation. `follow=1` is the product
SyncIF verdict used for cyclic Statusword bit 12; generation freshness alone does
not assert that the physical drive follows the command.

The checked-in RPDO1/2/3 engineering example still maps Controlword, Modes of
operation, and Target position and remains disabled by default. For RPDO-driven
CSV or CST tests, remap the selected axis PDO to Target velocity (`0x60FF`) or
Target torque (`0x6071`) before enabling that PDO; do not change the checked-in
mapping merely to run one mode-specific test.
