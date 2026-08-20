[中文](../zh/object-dictionary.md)

# Object Dictionary guide

The Object Dictionary is the application data model exposed through CANopen. This package includes a generated demo OD for bring-up, but production firmware should normally provide its own OD.

## 1. Demo OD layout

The included demo files are under:

```text
examples/demo_device/
├── OD.c
├── OD.h
├── project.eds
├── project.md
└── project.xdd
```

When `PKG_CANOPENNODE_USING_DEMO_OD` is enabled, `SConscript` compiles `examples/demo_device/OD.c` and adds `examples/demo_device/` to the include path.

## 2. When to use the demo OD

Use the demo OD when:

- bringing up the RT-Thread CAN driver and this package for the first time;
- validating basic NMT, heartbeat, SDO, SYNC, or PDO runtime paths;
- checking whether the target sends the expected CANopen boot-up frame;
- testing thread priorities, CAN receive dispatch, and logging without product-specific OD complexity.

Do not treat the demo OD as the final product data model.

## 3. Replacing the demo OD

For a product OD:

1. Generate `OD.c` and `OD.h` from the product CANopen object model.
2. Place the generated files in the application or BSP source tree.
3. Disable `PKG_CANOPENNODE_USING_DEMO_OD`.
4. Add the product `OD.c` to the application/BSP SConscript.
5. Add the directory containing product `OD.h` to the include path before building this package.
6. Verify that the selected CANopenNode feature groups match objects present in the generated OD.

Example application-side SCons pattern:

```python
src += [os.path.join(cwd, 'canopen_od', 'OD.c')]
CPPPATH += [os.path.join(cwd, 'canopen_od')]
```

The exact SCons code depends on the application repository layout.

## 4. OD and storage groups

When storage is enabled, the wrapper can create storage entries for selected generated OD persistence groups:

| Kconfig option | Required generated symbol | OD 0x1010/0x1011 sub-index |
|---|---|---:|
| `PKG_CANOPENNODE_STORAGE_PERSIST_COMM` | `OD_PERSIST_COMM` | `2` |
| `PKG_CANOPENNODE_STORAGE_PERSIST_APP` | `OD_PERSIST_APP` | `3` |
| `PKG_CANOPENNODE_STORAGE_PERSIST_MANU` | `OD_PERSIST_MANU` | `4` |

Enable only the groups that are present in the generated `OD.h`. If storage is enabled but no selected persistence group exists, the build should be considered incorrectly configured.

## 5. OD and PDO mapping

PDO behavior depends on the generated OD entries and the selected Kconfig options:

- `PKG_CANOPENNODE_USING_PDO` must be enabled for PDO objects.
- `PKG_CANOPENNODE_RPDO` controls receive PDO support.
- `PKG_CANOPENNODE_TPDO` controls transmit PDO support.
- `PKG_CANOPENNODE_PDO_SYNC` is required for synchronous PDO behavior.
- `PKG_CANOPENNODE_PDO_OD_IO_ACCESS` makes PDO mapping use OD accessors instead of direct memory mapping.

When replacing the OD, verify:

1. RPDO communication parameters and mapping entries are valid.
2. TPDO communication parameters, event timers, inhibit times, and mapping entries match the product data path.
3. The NMT state is operational when expecting PDO traffic.
4. Application code protects shared OD variables when they are accessed from multiple RT-Thread threads.

## 6. OD and SDO

SDO access requires `PKG_CANOPENNODE_USING_SDO_SERVER` and compatible OD access attributes. For product OD entries, verify:

- read/write permissions match the intended diagnostic or configuration behavior;
- data lengths match the tester/master expectations;
- segmented or block transfers are enabled when larger data objects are required;
- write callbacks or OD extensions are safe under RT-Thread thread context.

## 7. OD and identity objects

Before production use, set identity and descriptive objects intentionally:

| Object | Purpose |
|---|---|
| `0x1000` | Device type. |
| `0x1008` | Manufacturer device name. |
| `0x1009` | Manufacturer hardware version. |
| `0x100A` | Manufacturer software version. |
| `0x1018` | Identity object: vendor ID, product code, revision, serial number. |

These values are often used by CANopen masters, commissioning tools, and field diagnostics.

## 8. Demo TIME consumer diagnostics

The generated demo OD contains manufacturer-specific record `0x2300` for automated TIME consumer validation:

| Sub-index | Type | Access | Meaning |
|---:|---|---|---|
| `0x01` | `UNSIGNED32` | read-only | Count of syntactically valid DLC=6 TIME frames observed by CANopenNode callback-pre. |
| `0x02` | `UNSIGNED32` | read-only | Applied `CO_TIME_t::ms` milliseconds after midnight. |
| `0x03` | `UNSIGNED16` | read-only | Applied `CO_TIME_t::days` day count since 1984-01-01. |

The record is RAM-only and not PDO-mappable. The values are updated only when `PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC` is enabled. The receive count is maintained in the RT-Thread wrapper instance and survives CANopen communication reset; the applied millisecond/day fields always come from the current `CO_TIME_t` after `CO_process()`.

This is a demo/test observability contract, not a standard CiA 301 TIME object. Product firmware with a custom OD should expose equivalent application evidence only when its validation strategy requires it.

## 9. Demo EMCY consumer diagnostics

The generated demo OD contains manufacturer-specific record `0x2301` for automated EMCY Consumer validation:

| Sub-index | Type | Access | Meaning |
|---:|---|---|---|
| `0x01` | `UNSIGNED32` | read-only | Number of remote EMCY callbacks received. |
| `0x02` | `UNSIGNED8` | read-only | Source Node-ID derived from the latest remote EMCY CAN-ID. |
| `0x03` | `UNSIGNED16` | read-only | CAN-ID of the latest remote EMCY. |
| `0x04` | `UNSIGNED16` | read-only | Latest EMCY error code. |
| `0x05` | `UNSIGNED8` | read-only | Latest EMCY error register. |
| `0x06` | `UNSIGNED8` | read-only | CANopenNode callback `errorBit` value (EMCY byte 3 / first manufacturer-specific byte). |
| `0x07` | `UNSIGNED32` | read-only | Latest manufacturer-specific info code. |

The receive callback updates RT-Thread atomic fields and an odd/even sequence counter. `CO_demo_process()` publishes each OD update only after reading one stable receive-side snapshot. Because the fields are separate SDO sub-indices, a Host that combines several reads should read `remote_rx_count` before and after the other fields and retry if the two counts differ. The receive count and latest remote EMCY snapshot intentionally survive communication reset while the callback is rebound to the recreated CANopenNode stack. Local EMCY callbacks identified by CANopenNode with `ident == 0` are ignored; duplicate remote EMCY messages and recovery messages with error code zero are each counted. The record is RAM-only and not PDO-mappable.

This is a demo/test observability contract, not a product-level remote fault manager.

## 10. GFC parameter and demo protocol diagnostics

The generated demo OD includes the standard GFC parameter `0x1300:00` as an `UNSIGNED8` read/write value with default `1`. CANopenNode interprets `0` as GFC disabled and `1` as GFC enabled; values greater than `1` are rejected by the GFC OD extension.

For automated J03/B09G protocol validation, manufacturer-specific record `0x2302` exposes only test control and evidence:

| Sub-index | Type | Access | Meaning |
|---:|---|---|---|
| `0x01` | `UNSIGNED32` | read-only | Count of accepted valid GFC consumer callbacks. |
| `0x02` | `UNSIGNED8` | read-only | Sticky protocol-test flag set after an accepted GFC callback. |
| `0x03` | `UNSIGNED32` | read/write | Producer request sequence written by the Host. |
| `0x04` | `UNSIGNED32` | read-only | Last producer request sequence consumed by the MCU mainline. |
| `0x05` | `INTEGER32` | read-only | Result of the latest mainline `CO_GFCsend()` call. |

The CAN receive callback only updates RT-Thread atomic receive evidence. `CO_GFCsend()` is invoked from the CANopenNode mainline when a new request sequence is observed. Receive evidence survives communication reset, while producer request/completion state is synchronized on rebind so an old request is not replayed on the recreated stack.

This record validates GFC protocol behavior only. It does not drive an actuator, implement a product safe state, or establish SIL/PL/EN 50325-5 compliance.

## 11. MCU SDO Client test control/status

For J04/B03 validation, manufacturer-specific record `0x2303` exposes a test-only request and result contract. It does not replace CANopenNode's SDO Client protocol state machine.

| Sub-index | Type | Access | Meaning |
|---:|---|---|---|
| `0x01` | `UNSIGNED32` | read/write | Request sequence. The Host writes this field last to commit a complete request. |
| `0x02` | `UNSIGNED8` | read/write | Command: `1=UPLOAD`, `2=DOWNLOAD`. |
| `0x03` | `UNSIGNED8` | read/write | Target SDO server Node-ID. |
| `0x04` | `UNSIGNED16` | read/write | Target Object Dictionary index. |
| `0x05` | `UNSIGNED8` | read/write | Target Object Dictionary sub-index. |
| `0x06` | `UNSIGNED32` | read/write | DOWNLOAD payload size in bytes; UPLOAD uses zero. |
| `0x07` | `UNSIGNED32` | read/write | U32 DOWNLOAD value or deterministic segmented-payload seed. |
| `0x08` | `UNSIGNED8` | read/write | Request flags. J04 requires zero; J06/B02-12 may set bit 0 to request block transfer when `PKG_CANOPENNODE_SDO_CLI_BLOCK` is compiled. |
| `0x09` | `UNSIGNED32` | read-only | Sequence accepted by the MCU mainline. |
| `0x0A` | `UNSIGNED32` | read-only | Sequence with a published terminal result. |
| `0x0B` | `INTEGER32` | read-only | Normalized result: `0=NONE`, `1=SUCCESS`, `2=ABORT`, `3=TIMEOUT`, `4=RESET_CANCELLED`, `5=SETUP_ERROR`, `6=UNSUPPORTED`, `7=INTERNAL_ERROR`. |
| `0x0C` | `UNSIGNED32` | read-only | Native CANopen SDO abort code; reset cancellation leaves this zero. |
| `0x0D` | `UNSIGNED32` | read-only | Number of payload bytes transferred. |
| `0x0E` | `UNSIGNED32` | read-only | Uploaded U32 value, or the successful U32 DOWNLOAD probe value. |
| `0x0F` | `UNSIGNED32` | read-only | FNV-1a checksum over payload bytes handled by the test wrapper. |

The Host writes `0x2303:02..08` first and commits the transaction by writing a new nonzero `request_seq`. The CANopen mainline latches the request and drives `CO_SDOclient_setup()`, initiate, and process calls without blocking. Local transfers use CANopenNode's `CO_CONFIG_SDO_CLI_LOCAL` path and therefore do not produce SDO frames on the CAN bus. Remote tests use the CiA 301 predefined SDO connection for the selected Node-ID.

J04 keeps the SDO Client FIFO at 32 bytes and uses a 48-byte segmented payload to exercise FIFO drain/refill. OD `0x1280` is not modified for these transactions. A communication reset cancels an active request as `RESET_CANCELLED`, consumes its sequence, and prevents replay after the CANopenNode stack is recreated. J04 always uses `flags=0`; J06/B02-12 uses bit 0 only when `PKG_CANOPENNODE_SDO_CLI_BLOCK` is additionally enabled, so the original segmented path is unchanged.

## 12. SDO Server Block Transfer test object

J06/B02 uses manufacturer-specific `0x2304:00` as an SDO read/write, non-PDO-mappable `DOMAIN`. The demo dispatcher binds its test-only OD extension only when `PKG_CANOPENNODE_DEMO_SDO_BLOCK_TEST` is enabled.

The backend uses a fixed 2048-byte RAM buffer for variable-length payloads. It performs no dynamic allocation or persistence and does not implement the SDO Block/CRC state machine; block sequencing, CRC handling and retransmission remain owned by the CANopenNode SDO Server. The generated OD keeps this DOMAIN at `dataOrig=NULL` and `dataLength=0`; the extension publishes the active length while reading or writing.

The single-buffer fixture does not promise atomic rollback of the old payload after an aborted download. If a large transfer already wrote partial data, the fixture remains dirty and reads return no-data; the next complete download starting at offset zero re-establishes a valid payload. A communication reset while dirty normalizes the fixture to a deterministic one-byte baseline, while a complete payload survives communication reset.

The target J06 profile must also enable `PKG_CANOPENNODE_SDO_SRV_BLOCK`. The SDO Server block buffer value belongs to the BSP/test profile; 1024 bytes is the first-version recommendation and must be evaluated against linker-map/heap evidence instead of changing the package-wide default.

## 13. Validation checklist

Before replacing the demo OD in a product build, verify:

- `PKG_CANOPENNODE_USING_DEMO_OD` is disabled.
- The product `OD.c` is compiled exactly once.
- The product `OD.h` is reachable from the include path.
- Enabled CANopenNode Kconfig features have matching OD objects.
- SDO access to product entries behaves as expected.
- PDO mappings are valid and do not exceed classic CAN frame length.
- Storage groups match generated `OD_PERSIST_*` symbols.
- Identity objects and Node-ID strategy are product-specific.
- Application OD access is protected when shared across threads.
