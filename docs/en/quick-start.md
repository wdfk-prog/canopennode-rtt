[中文](../zh/quick-start.md)

# Quick start

This guide brings up one CANopenNode RT-Thread instance on an RT-Thread BSP with a working CAN device.

## 1. Prerequisites

Confirm these items before enabling the package:

| Item | Requirement |
|---|---|
| RT-Thread CAN driver | The BSP must expose a CAN device, for example `can0` or `can1`. |
| CAN transceiver | The board must have a physical CAN transceiver and correct termination. |
| Bitrate | All nodes on the bus must use the same bitrate. |
| CANopen Node-ID | The selected Node-ID must be unique on the CANopen network. |
| Submodule | `CANopenNode/301` must exist after submodule initialization. |

## 2. Clone the repository

For a new checkout:

```sh
git clone --recursive <repo-url> canopennode-rtt
cd canopennode-rtt
```

For an existing checkout:

```sh
git submodule update --init --recursive
```

The package build expects the `CANopenNode` submodule under the package root. See [Submodule update guide](submodule-update.md) for update and recovery workflows.

## 3. Add the package to the RT-Thread project

Keep the repository layout unchanged and make the package visible to the RT-Thread project. The parent project must include this package's `Kconfig` in its menu tree and this package's `SConscript` in the SCons build path.

Typical local layout:

```text
rt-thread-project/
├── bsp/<board>/
├── packages/
│   └── canopennode-rtt/
│       ├── CANopenNode/
│       ├── Kconfig
│       └── SConscript
└── rt-thread/
```

The exact parent Kconfig/SCons include point depends on the project's package management style. The package itself assumes only that its root-relative paths are preserved.

## 4. Enable required RT-Thread features

In the BSP configuration, enable the core RT-Thread features required by `PKG_USING_CANOPENNODE`:

```text
RT_USING_HEAP
RT_USING_DEVICE
RT_USING_CAN
RT_USING_MUTEX
RT_USING_SEMAPHORE
```

Enable optional features only when needed:

```text
RT_CAN_USING_HDR     # Optional hardware CAN filter path
RT_USING_ULOG        # Optional debug logs
RT_USING_PIN         # Optional CANopen LED GPIO output
RT_USING_DFS         # Optional DFS storage backend
PKG_USING_AT24CXX    # Optional EEPROM storage backend
```

## 5. Configure the package

Run `menuconfig`, then enable the package:

```text
PKG_USING_CANOPENNODE=y
```

For a first demo, keep most defaults and verify these items:

| Option | Recommended first value |
|---|---|
| `PKG_CANOPENNODE_CAN_DEV_NAME` | BSP CAN device name, for example `can1`. |
| `PKG_CANOPENNODE_APP_AUTO_INIT` | `y` for the first demo. |
| `PKG_CANOPENNODE_AUTO_INIT_CAN_DEV_NAME` | Same as the real RT-Thread CAN device name. |
| `PKG_CANOPENNODE_AUTO_INIT_NODE_ID` | A unique value from `1` to `127`. |
| `PKG_CANOPENNODE_AUTO_INIT_BITRATE` | The bus bitrate, for example `1000`. |
| `PKG_CANOPENNODE_USING_DEMO_OD` | `y` for first bring-up. |
| `PKG_CANOPENNODE_USING_SDO_SERVER` | `y` if SDO access is required. |
| `PKG_CANOPENNODE_USING_PDO` | `y` if PDO demo behavior is required. |

## 6. Build

From the RT-Thread BSP or application build directory, build with SCons:

```sh
scons
```

If the build reports a missing CANopenNode source path, run:

```sh
git submodule update --init --recursive
```

If the build reports a missing `examples/demo_device/OD.h`, keep `PKG_CANOPENNODE_USING_DEMO_OD=y` only when the demo OD files are present. Disable it when the application supplies its own OD.

## 7. Run and verify

After flashing the target, verify the following behavior:

1. The RT-Thread CAN device is opened successfully.
2. The configured bitrate is accepted by the BSP CAN driver.
3. The CANopen node sends a boot-up frame on startup.
4. If SDO server is enabled, a CANopen master or tester can access OD entries through SDO.
5. If PDO is enabled and the demo OD is used, TPDO/RPDO behavior matches the demo mapping.

A common boot-up frame for Node-ID `1` uses COB-ID `0x701` and one data byte `0x00`.

## 8. Manual initialization path

Automatic initialization is convenient for a single demo instance. Disable `PKG_CANOPENNODE_APP_AUTO_INIT` when the application wants explicit control.

```c
#include "CO_app_RTT.h"

static CANopenNodeRTT canopen_app;

static int app_canopen_init(void)
{
    return (int)canopen_app_rtt_init(&canopen_app, "can1", 1, 1000);
}
INIT_APP_EXPORT(app_canopen_init);
```

Rules for manual initialization:

- `CANopenNodeRTT` must be zero-initialized before first use.
- `canName` must match an RT-Thread CAN device name returned by `rt_device_find()`.
- `nodeID` must be `1..127`, unless LSS slave is enabled and the unconfigured assignment value is intentionally used.
- `bitrate` is expressed in kbit/s.
- Do not create a manual instance while auto initialization is still enabled for the same CAN device and Node-ID.

## 9. Next steps

- Read [RT-Thread integration](rt-thread-integration.md) before changing thread priorities or calling CANopenNode APIs from application code.
- Read [Configuration guide](configuration.md) before enabling optional protocol groups.
- Read [Object Dictionary guide](object-dictionary.md) before replacing the demo OD.
