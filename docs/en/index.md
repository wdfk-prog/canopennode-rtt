[中文](../zh/index.md)

# CANopenNode RT-Thread Documentation

This documentation is organized as a project manual for RT-Thread BSP developers, firmware integrators, and CANopen device developers.

## Recommended reading

1. [Quick start](quick-start.md) — add the package to an RT-Thread BSP and start the first node.
2. [RT-Thread integration](rt-thread-integration.md) — runtime ownership, threads, locks, reset lifecycle, and CAN device interaction.
3. [Configuration guide](configuration.md) — Kconfig feature groups and dependency rules.
4. [Object Dictionary](object-dictionary.md) — demo OD usage and product OD replacement.
5. [High-Resolution Time](high-resolution-time.md) — optional hardware microsecond time source.
6. [LSS persistence](lss-persistence.md) — persistent Node-ID/bitrate and runtime bitrate switching.
7. [CiA 402 Device core](cia402-device-core.md) — Pure-C multi-axis PDS and OD binding.
8. [CiA 402 RT-Thread integration](cia402-device-rtt.md) — lifecycle, thread, and Communication Reset integration.
9. [CiA 402 diagnostics and product integration](cia402.md) — per-axis diagnostics and product OD guidance.
10. [CiA 402 Controller API](cia402-controller.md) — remote-drive PDS Controlword sequencing.
11. [Submodule update](submodule-update.md) — initialize, update, and pin CANopenNode.
12. [Troubleshooting](troubleshooting.md) — common build, CAN, OD, storage, and runtime issues.

## Document map

| Document | Scope |
|---|---|
| [Quick start](quick-start.md) | Package integration, initial configuration, build entry, and node startup. |
| [RT-Thread integration](rt-thread-integration.md) | Runtime structure, scheduling, ownership, lifecycle, and reset behavior. |
| [Configuration guide](configuration.md) | Kconfig groups, dependencies, defaults, and integration choices. |
| [Object Dictionary](object-dictionary.md) | Generated demo OD, custom OD integration, and demo-only objects. |
| [High-Resolution Time](high-resolution-time.md) | Timer source contract and time-width limitations. |
| [LSS persistence](lss-persistence.md) | Persistent LSS record format, startup loading, bitrate switching, and recovery. |
| [CiA 402 Device core](cia402-device-core.md) | Multi-axis Device runtime, PDS supervision, OD binding, and DriveIF contract. |
| [CiA 402 RT-Thread integration](cia402-device-rtt.md) | Device lifecycle attachment, worker thread, lock order, and reset/rebind behavior. |
| [CiA 402 diagnostics and product integration](cia402.md) | Error-code/EMCY bridge and product-generated OD guidance. |
| [CiA 402 Controller API](cia402-controller.md) | Transport-independent remote PDS target sequencing. |
| [Submodule update](submodule-update.md) | CANopenNode submodule maintenance. |
| [Troubleshooting](troubleshooting.md) | Diagnostic paths for common integration failures. |

## Repository entry pages

- [English README](../../README.md)
- [中文 README](../../README.zh-CN.md)
