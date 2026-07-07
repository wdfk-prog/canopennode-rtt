[中文](../zh/configuration.md)

# Configuration guide

This package is configured mainly through `Kconfig`. The generated configuration controls RT-Thread runtime parameters, selected CANopenNode source files, and CANopenNode `CO_CONFIG_*` feature flags.

This page summarizes the options that usually matter during integration. The `Kconfig` file remains the authoritative source for defaults and dependency rules.

## 1. Core enable option

| Option | Default | Notes |
|---|---:|---|
| `PKG_USING_CANOPENNODE` | `n` | Enables the package. Depends on heap, device, CAN, mutex, and semaphore support. |

When this option is disabled, the package contributes no source files to the build group.

## 2. CAN device binding

| Option | Default | Notes |
|---|---:|---|
| `PKG_CANOPENNODE_CAN_DEV_NAME` | `"can1"` | General RT-Thread CAN device name used by package configuration. |
| `PKG_CANOPENNODE_CAN_BINDING_COUNT` | `1` | Maximum number of RT-Thread CAN device bindings. Increase only for multi-instance designs. |
| `PKG_CANOPENNODE_USING_RTT_CAN_FILTER` | `n` | Uses RT-Thread CAN HDR filters when available and when all RX buffers can be assigned filters. |

If the configured CAN device name does not match a BSP device returned by `rt_device_find()`, initialization fails before CANopen communication starts.

## 3. Runtime thread options

| Option | Default | Purpose |
|---|---:|---|
| `PKG_CANOPENNODE_RX_THREAD_STACK_SIZE` | `2048` | Stack for the CAN RX helper. |
| `PKG_CANOPENNODE_RX_THREAD_PRIORITY` | `2` | RX helper priority. Lower RT-Thread numeric value means higher priority. |
| `PKG_CANOPENNODE_RX_THREAD_TICK` | `10` | RX helper time slice. |
| `PKG_CANOPENNODE_RX_BATCH_SIZE` | `8` | Number of CAN frames read per RX loop. |
| `PKG_CANOPENNODE_MAIN_THREAD_STACK_SIZE` | `2048` | Stack for the mainline thread. |
| `PKG_CANOPENNODE_MAIN_THREAD_PRIORITY` | `10` | Mainline processing priority. |
| `PKG_CANOPENNODE_MAIN_THREAD_TICK` | `10` | Mainline time slice. |
| `PKG_CANOPENNODE_RT_THREAD_STACK_SIZE` | `2048` | Stack for realtime processing. |
| `PKG_CANOPENNODE_RT_THREAD_PRIORITY` | `3` | Realtime processing priority. |
| `PKG_CANOPENNODE_RT_THREAD_TICK` | `10` | Realtime time slice. |
| `PKG_CANOPENNODE_TIMER_PERIOD_US` | `1000` | Requested realtime processing period in microseconds. |

Recommended priority relationship for typical CANopen devices:

```text
RX helper priority <= realtime priority < mainline priority
```

Because RT-Thread uses lower numeric values as higher priorities, the default RX helper priority `2`, realtime priority `3`, and mainline priority `10` follow this model.

## 4. Application auto initialization

| Option | Default | Notes |
|---|---:|---|
| `PKG_CANOPENNODE_APP_FIRST_HB_TIME_MS` | `500` | First heartbeat producer delay. |
| `PKG_CANOPENNODE_APP_SDO_SRV_TIMEOUT_MS` | `1000` | SDO server timeout. |
| `PKG_CANOPENNODE_APP_SDO_CLI_TIMEOUT_MS` | `500` | Default SDO client timeout. |
| `PKG_CANOPENNODE_APP_SDO_CLI_BLOCK` | `n` | Enables SDO client block transfer by default when supported. |
| `PKG_CANOPENNODE_APP_AUTO_INIT` | `y` | Creates one default instance during RT-Thread application initialization. |
| `PKG_CANOPENNODE_AUTO_INIT_CAN_DEV_NAME` | `"can1"` | CAN device used by auto init. |
| `PKG_CANOPENNODE_AUTO_INIT_NODE_ID` | `1` | Node-ID used by auto init. |
| `PKG_CANOPENNODE_AUTO_INIT_BITRATE` | `1000` | Bitrate used by auto init. |

Disable `PKG_CANOPENNODE_APP_AUTO_INIT` when the product creates `CANopenNodeRTT` instances explicitly.

## 5. Common CANopenNode flags

| Option | Default | Notes |
|---|---:|---|
| `PKG_CANOPENNODE_GLOBAL_CALLBACK_PRE` | `n` | Enables supported mainline callback-pre hooks. |
| `PKG_CANOPENNODE_GLOBAL_RT_CALLBACK_PRE` | `n` | Enables supported realtime callback-pre hooks. |
| `PKG_CANOPENNODE_GLOBAL_TIMERNEXT` | `n` | Enables `timerNext_us` calculation where supported. |
| `PKG_CANOPENNODE_GLOBAL_OD_DYNAMIC` | `y` | Lets supported objects re-read OD communication parameters after OD writes. |

Keep dynamic OD enabled for configurable devices unless the product intentionally forbids runtime communication-parameter changes.

## 6. CiA 301 services

### NMT, heartbeat, and node guarding

| Option | Default | Notes |
|---|---:|---|
| `PKG_CANOPENNODE_NMT_CALLBACK_CHANGE` | `n` | Application callback on local NMT state change. |
| `PKG_CANOPENNODE_NMT_MASTER` | `n` | Allows this node to transmit simple NMT master commands. |
| `PKG_CANOPENNODE_USING_HB_CONS` | `y` | Heartbeat consumer support. |
| `PKG_CANOPENNODE_USING_NODE_GUARDING` | `n` | Legacy node guarding support. Prefer heartbeat for new designs. |

### Emergency

| Option | Default | Notes |
|---|---:|---|
| `PKG_CANOPENNODE_EM_PRODUCER` | `y` | Local node can transmit EMCY messages. |
| `PKG_CANOPENNODE_EM_HISTORY` | `y` | Stores recent emergency history in OD 0x1003. |
| `PKG_CANOPENNODE_EM_CONSUMER` | `n` | Receive EMCY messages from remote nodes. |
| `PKG_CANOPENNODE_EM_ERR_STATUS_BITS_COUNT` | `80` | Error status bit count, valid range 48..256 and multiple of 8. |

### SDO

| Option | Default | Notes |
|---|---:|---|
| `PKG_CANOPENNODE_USING_SDO_SERVER` | `y` | Required for normal OD access by a master/tool. |
| `PKG_CANOPENNODE_SDO_SRV_SEGMENTED` | `y` | Allows server transfers larger than expedited frames. |
| `PKG_CANOPENNODE_SDO_SRV_BLOCK` | `n` | Improves large transfers, requires larger buffer and CRC16. |
| `PKG_CANOPENNODE_SDO_SRV_BUFFER_SIZE` | `32` or `1000` | Larger when server block transfer is enabled. |
| `PKG_CANOPENNODE_USING_SDO_CLIENT` | `n` | Enable for masters, gateways, or local tools that access remote nodes. |
| `PKG_CANOPENNODE_SDO_CLI_BLOCK` | `n` | Enables SDO client block transfer when segmented transfer is enabled. |
| `PKG_CANOPENNODE_SDO_CLI_BUFFER_SIZE` | `32` | Internal SDO client circular buffer size. |

### TIME, SYNC, and PDO

| Option | Default | Notes |
|---|---:|---|
| `PKG_CANOPENNODE_USING_TIME` | `y` | TIME consumer support by default. |
| `PKG_CANOPENNODE_TIME_PRODUCER` | `n` | Enable only for the network time producer. |
| `PKG_CANOPENNODE_USING_SYNC` | `y` | SYNC object support. |
| `PKG_CANOPENNODE_SYNC_PRODUCER` | `y` | Local node can produce SYNC. Validate this against network topology. |
| `PKG_CANOPENNODE_USING_PDO` | `y` | PDO object support. |
| `PKG_CANOPENNODE_RPDO` | `y` | Receive PDO support. |
| `PKG_CANOPENNODE_TPDO` | `y` | Transmit PDO support. |
| `PKG_CANOPENNODE_PDO_SYNC` | `y` | PDO synchronous transmission support. |
| `PKG_CANOPENNODE_PDO_OD_IO_ACCESS` | `y` | PDO mapping uses OD IO accessors. |

## 7. Storage

| Option | Default | Notes |
|---|---:|---|
| `PKG_CANOPENNODE_USING_STORAGE` | `n` | Enables CANopenNode storage and OD 0x1010/0x1011 behavior. |
| `PKG_CANOPENNODE_STORAGE_MAX_ENTRIES_COUNT` | `1` | Maximum supported storage entry count. |
| `PKG_CANOPENNODE_STORAGE_PERSIST_COMM` | `y` | Persist communication parameters. Requires generated `OD_PERSIST_COMM`. |
| `PKG_CANOPENNODE_STORAGE_PERSIST_APP` | `n` | Persist application parameters. Requires generated `OD_PERSIST_APP`. |
| `PKG_CANOPENNODE_STORAGE_PERSIST_MANU` | `n` | Persist manufacturer parameters. Requires generated `OD_PERSIST_MANU`. |

Storage backend choice:

| Backend option | Requirement | Notes |
|---|---|---|
| `PKG_CANOPENNODE_USING_STORAGE_DFS` | `RT_USING_DFS` | File-based backend. Directory must already exist. Not power-loss safe by default. |
| `PKG_CANOPENNODE_USING_STORAGE_EEPROM` | `PKG_USING_AT24CXX` and one CAN binding | Uses AT24CXX adapter and CANopenNode EEPROM storage. |
| `PKG_CANOPENNODE_USING_STORAGE_USER` | Application backend | Application must provide `co_storage_rtt_backend_get_ops()`. |

Exactly one storage backend must be selected when storage is enabled.

## 8. LED, safety, LSS, and gateway options

| Group | Main option | Default | Notes |
|---|---|---:|---|
| LED | `PKG_CANOPENNODE_USING_LEDS` | `y` | Enables CiA 303-3 run/error LED state calculation. |
| RT-Thread PIN LED | `PKG_CANOPENNODE_LEDS_USING_RTT_PIN` | `n` | Drives configured RT-Thread PIN outputs. |
| GFC | `PKG_CANOPENNODE_USING_GFC` | `n` | Safety-related object; enable only with a validated safety design. |
| SRDO | `PKG_CANOPENNODE_USING_SRDO` | `n` | Safety-related PDO; requires CRC16. |
| LSS slave | `PKG_CANOPENNODE_USING_LSS_SLAVE` | `y` | Allows node ID and bitrate configuration by an LSS master. |
| LSS master | `PKG_CANOPENNODE_USING_LSS_MASTER` | `n` | Enable for commissioning tools or gateways. |
| ASCII gateway | `PKG_CANOPENNODE_USING_GATEWAY_ASCII` | `n` | Compiles gateway helpers; application must provide the command transport. |

## 9. Helper and debug options

| Option | Default | Notes |
|---|---:|---|
| `PKG_CANOPENNODE_USING_CRC16` | `n` | Selected automatically by features that require CRC16. |
| `PKG_CANOPENNODE_CRC16_EXTERNAL` | `n` | Enable only when the application provides CRC16. |
| `PKG_CANOPENNODE_USING_FIFO` | `n` | Selected by SDO client or ASCII gateway features. |
| `PKG_CANOPENNODE_USING_TRACE` | unavailable | Intentionally disabled until trace is ported to the current APIs. |
| `PKG_CANOPENNODE_USING_DEBUG` | `n` | Enables RT-Thread ulog diagnostics for this port. |
| `PKG_CANOPENNODE_USING_FRAME_TRACE` | `n` | Logs CAN RX/TX frames. Use only for bring-up diagnostics. |

## 10. Demo Object Dictionary

| Option | Default | Notes |
|---|---:|---|
| `PKG_CANOPENNODE_USING_DEMO_OD` | `y` | Compiles `examples/demo_device/OD.c` and adds the demo OD include path. |

Disable this option when the BSP or application provides a custom generated `OD.c` and `OD.h` through its own SConscript and include path.
