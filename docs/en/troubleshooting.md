[中文](../zh/troubleshooting.md)

# Troubleshooting

This page lists common integration failures and the fastest checks for each case.

## 1. `PKG_USING_CANOPENNODE` is not visible

Likely causes:

- Required RT-Thread features are disabled.
- This package's `Kconfig` is not included by the parent project.

Check that these dependencies are enabled:

```text
RT_USING_HEAP
RT_USING_DEVICE
RT_USING_CAN
RT_USING_MUTEX
RT_USING_SEMAPHORE
```

Then confirm the parent package menu includes this package's `Kconfig`.

## 2. Build reports missing `CANopenNode` or `CANopenNode/301`

Likely cause: the git submodule has not been initialized.

Fix from the repository root:

```sh
git submodule update --init --recursive
```

Then verify:

```sh
test -d CANopenNode/301 && echo ok
```

## 3. Build reports missing `examples/demo_device/OD.h`

Likely causes:

- `PKG_CANOPENNODE_USING_DEMO_OD` is enabled but demo OD files are absent.
- The package root layout was changed.

Fix options:

- Restore `examples/demo_device/OD.c` and `OD.h`.
- Or disable `PKG_CANOPENNODE_USING_DEMO_OD` and compile the product `OD.c` from the application/BSP.

## 4. Initialization fails before CANopen starts

Likely causes:

| Cause | Check |
|---|---|
| Wrong CAN device name | Run RT-Thread device list command if available, or inspect BSP CAN registration. |
| CAN driver not enabled | Confirm `RT_USING_CAN` and BSP CAN peripheral support. |
| Invalid Node-ID | Use `1..127`, unless LSS unconfigured mode is intentionally used. |
| Invalid bitrate | Use a non-zero bitrate supported by the BSP driver. |
| Duplicate auto/manual init | Disable `PKG_CANOPENNODE_APP_AUTO_INIT` when using explicit `canopen_app_rtt_init()`. |

## 5. No CANopen boot-up frame

Check in this order:

1. Physical CAN wiring, transceiver enable pin, and bus termination.
2. CAN bitrate on every node.
3. RT-Thread CAN device name and open/configuration status.
4. Whether the CAN controller is in normal mode, not silent or loopback mode.
5. Whether a CAN analyzer can see any frame from the target.
6. Whether initialization logs report an error when `PKG_CANOPENNODE_USING_DEBUG` is enabled.

A node with Node-ID `1` normally sends boot-up on COB-ID `0x701` with payload `00`.

## 6. SDO timeout

Likely causes:

- `PKG_CANOPENNODE_USING_SDO_SERVER` is disabled.
- Master uses the wrong Node-ID.
- Bus bitrate or termination is wrong.
- OD 0x1200 server parameters do not match the expected default COB-IDs.
- The node has not completed initialization.

Checks:

```text
Node-ID 1 default SDO request COB-ID: 0x601
Node-ID 1 default SDO response COB-ID: 0x581
```

Also confirm that the tester/master is using classic CAN frames and the correct bitrate.

## 7. PDO does not update

Check:

1. `PKG_CANOPENNODE_USING_PDO` is enabled.
2. `PKG_CANOPENNODE_RPDO` or `PKG_CANOPENNODE_TPDO` is enabled for the required direction.
3. The node is in NMT operational state.
4. OD PDO communication and mapping parameters are valid.
5. For event-driven TPDO, event timer or event trigger logic is configured.
6. For synchronous PDO, SYNC is present and `PKG_CANOPENNODE_PDO_SYNC` is enabled.
7. Shared OD variables are protected when accessed from application threads.

## 8. CAN RX works only with software dispatch

This can be normal. RT-Thread CAN HDR filtering is optional and depends on BSP support and available filter banks. If `PKG_CANOPENNODE_USING_RTT_CAN_FILTER` is enabled but the driver cannot assign all RX buffers to HDR banks, the port falls back to software dispatch.

Software dispatch is valid for bring-up and many products. Revisit hardware filtering only after measuring CPU load and bus traffic.

## 9. Storage does not persist values

Check:

| Backend | Checks |
|---|---|
| DFS | `RT_USING_DFS` enabled, mount point exists, configured directory exists, path length fits `PKG_CANOPENNODE_STORAGE_DFS_MAX_PATH`. |
| EEPROM | `PKG_USING_AT24CXX` enabled, I2C bus name correct, AT24CXX address input correct, storage offset does not overlap other data. |
| User | Application provides a strong `co_storage_rtt_backend_get_ops()` symbol and permanent ops table. |

Also check that the generated OD provides the selected `OD_PERSIST_*` groups and that OD 0x1010/0x1011 access is tested through SDO.

## 10. Trace cannot be enabled

This is intentional. `PKG_CANOPENNODE_USING_TRACE` depends on an unavailable internal option because the current trace module is not ported to the SDO server and Object Dictionary APIs used by this package.

Do not force-enable trace by editing generated configuration. Use `PKG_CANOPENNODE_USING_DEBUG` or `PKG_CANOPENNODE_USING_FRAME_TRACE` for bring-up diagnostics instead.

## 11. Multi-instance issues

Check:

- Increase `PKG_CANOPENNODE_CAN_BINDING_COUNT` if more than one RT-Thread CAN device is bound.
- Use separate `CANopenNodeRTT` instances.
- Use unique CAN device names or well-defined binding ownership.
- Use unique Node-IDs on the same CANopen network.
- Do not use the built-in AT24CXX EEPROM backend for multiple instances; it is intentionally limited to one binding.

## 12. Debug strategy

For difficult bring-up:

1. Enable `PKG_CANOPENNODE_USING_DEBUG` if `RT_USING_ULOG` is available.
2. Enable `PKG_CANOPENNODE_USING_FRAME_TRACE` only temporarily to inspect RX/TX frames.
3. Start from the demo OD and default SDO server.
4. Verify boot-up, then SDO, then PDO.
5. Add storage only after basic communication is stable.
6. Replace the demo OD after runtime and bus-level behavior are confirmed.
