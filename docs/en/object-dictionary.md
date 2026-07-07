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

## 8. Validation checklist

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
