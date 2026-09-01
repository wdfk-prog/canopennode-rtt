[中文](../zh/index.md)

# CANopenNode RT-Thread Documentation

This documentation is organized for RT-Thread BSP developers, firmware integrators, and CANopen device developers.

## Start here

1. Read [Quick start](quick-start.md) to build and run the package in an RT-Thread BSP.
2. Read [RT-Thread integration](rt-thread-integration.md) to understand runtime ownership, threads, locks, and CAN device interaction.
3. Read [Configuration guide](configuration.md) before changing Kconfig feature groups.
4. Read [High-Resolution Time integration](high-resolution-time.md) before enabling the hardware microsecond time source.
5. Read [Persistent LSS Node-ID and bitrate](lss-persistence.md) when using Storage-backed LSS persistence.
6. Read [Object Dictionary guide](object-dictionary.md) before replacing the demo OD.
7. Read [Testing and validation](testing.md) before running protocol validation with the Linux Host.
8. Read [Submodule update guide](submodule-update.md) before updating CANopenNode.
9. Use [Troubleshooting](troubleshooting.md) when build or runtime behavior is unexpected.

## Document map

| Document | Reader question |
|---|---|
| [Quick start](quick-start.md) | How do I add, configure, build, and verify the package quickly? |
| [RT-Thread integration](rt-thread-integration.md) | How does the RT-Thread runtime wrapper interact with CANopenNode and the CAN driver? |
| [Configuration guide](configuration.md) | Which Kconfig options matter for runtime, protocol objects, storage, logging, and debug? |
| [High-Resolution Time integration](high-resolution-time.md) | What are the 1 MHz, 32-bit, single-instance timer requirements and the current API width-detection limitation? |
| [Persistent LSS Node-ID and bitrate](lss-persistence.md) | How is LSS configuration loaded from the selected Storage backend before the first CAN initialization? |
| [Testing and validation](testing.md) | How do the MCU-side fixtures work with the `canopen-slave-tester` master code, and which evidence belongs to Host, target, or HIL validation? |
| [NMT Master automatic test](nmt-master-test.md) | How does the MCU automatically drive a Linux Lely slave to validate the NMT Master command sequence? |
| [Object Dictionary guide](object-dictionary.md) | How do I use or replace the demo OD? |
| [Submodule update guide](submodule-update.md) | How do I clone, initialize, update, or pin the CANopenNode submodule? |
| [Troubleshooting](troubleshooting.md) | How do I diagnose common build, CAN, SDO, PDO, storage, or trace issues? |

## Repository entry pages

- [English README](../../README.md)
- [中文 README](../../README.zh-CN.md)
- [Companion Linux Host/master test repository](https://github.com/wdfk-prog/canopen-slave-tester)
