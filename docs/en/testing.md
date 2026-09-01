[中文](../zh/testing.md)

# Testing and validation

This repository provides the MCU/RT-Thread CANopenNode integration, demo/test Object Dictionary, protocol fixtures, and target-side observability. The corresponding Linux Host/master-side automated protocol tests are maintained in [canopen-slave-tester](https://github.com/wdfk-prog/canopen-slave-tester).

`canopen-slave-tester` is based on Lely CANopen. Its normal role is the test master that drives and checks the MCU node over a real CAN bus. When validating this repository's NMT Master capability, the Host can switch to a Lely `BasicSlave` Node 2 and become the remote node controlled by the MCU. The Host repository defines which automatic flows are currently enabled; this repository does not duplicate that configuration.

## 1. Repository responsibilities

| Repository | Primary responsibility |
|---|---|
| `canopennode-rtt` | RT-Thread CAN target driver, CANopenNode lifecycle/thread wrapper, Kconfig/SCons integration, storage backends, demo OD, MCU-side fixtures, and diagnostic records. |
| `canopen-slave-tester` | Linux/Lely Host or test master, automated protocol flows, Host-side assertions, and evidence capture; it can act as the remote Lely Slave for NMT Master validation. |

The repositories interact through standard CANopen objects, CAN frames, and the test-only OD records from this package. They do not share an in-process API or a common source build.

## 2. Recommended topology

Normal slave-capability validation:

```text
Linux / Lely canopen-slave-tester
           |
           | SocketCAN / CAN
           v
RT-Thread MCU / canopennode-rtt
```

MCU NMT Master validation:

```text
RT-Thread MCU / CANopenNode Node 1
           |
           | NMT + Heartbeat
           v
Linux / Lely BasicSlave Node 2
(canopen-slave-tester Slave role)
```

See [NMT Master automatic test](nmt-master-test.md) for the target configuration and command sequence.

## 3. MCU-side fixtures

The following objects are for automated protocol validation and target-side observability. They are not product application OD entries:

| Capability | Kconfig / object | MCU-side evidence |
|---|---|---|
| TIME consumer | `PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC`, `0x2300` | Valid TIME receive count and the applied ms/day values. |
| EMCY consumer | `PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC`, `0x2301` | Remote EMCY receive count and a coherent latest-message snapshot. |
| GFC | `PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC`, `0x1300` + `0x2302` | Consumer receive evidence and producer request/result sequence. |
| MCU SDO Client | `PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST`, `0x2303` | Request/active/completion sequence, direction, length, and native SDO result. |
| SDO Server Block | `PKG_CANOPENNODE_DEMO_SDO_BLOCK_TEST`, `0x2304` | Bounded `DOMAIN` payload for block-transfer and CRC-path validation. |
| EEPROM Storage | `PKG_CANOPENNODE_DEMO_STORAGE_DIAGNOSTIC`, `0x2305` | Storage startup result, raw-region backup/restore, and controlled corruption entry points. |
| SRDO | `PKG_CANOPENNODE_DEMO_SRDO_DIAGNOSTIC`, `0x2306` | Deterministic SRDO channel state and TX-request observability. |
| NMT Master | `PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST`, `0x1016` Heartbeat Consumer | Remote online state, NMT transitions, reset boot-up, and command result. |
| LSS persistence | Standard LSS services + storage backend | Node-ID/bitrate configure, Store, Reset Node, power-cycle, and runtime bitrate-switch results. |

See the [Object Dictionary guide](object-dictionary.md) for field definitions and limitations, and the [Configuration guide](configuration.md) for dependencies.

## 4. Integration flow

1. Enable only the CANopen features and demo/test fixtures required by the current MCU validation scope; do not enable unrelated capabilities just to create an all-on image.
2. Build and flash the product BSP, perform basic CAN bring-up, and confirm that Node-ID and bitrate match the Host configuration.
3. Prepare the Linux Host according to the current `canopen-slave-tester` README and configuration. Its build, DCF/EDS, and deployment commands remain authoritative in that repository and are not duplicated here.
4. Run the corresponding protocol validation over the real CAN bus while retaining Host logs, MCU logs, and `candump` evidence where useful.
5. Tests that temporarily modify OD or communication parameters must restore the original values. Persistence tests must also include Reset Node and a real power cycle.
6. Disable demo/test Kconfig options that must not enter product firmware, then validate the product Object Dictionary and actual product configuration again.

## 5. Evidence boundaries

- A successful Host build or syntax check only proves that the Host tooling can be built; it does not prove target protocol behavior.
- A package static check or CI compile proves only the covered configuration/build graph. It does not prove real CAN timing, hardware filters, EEPROM behavior, bitrate switching, or power-loss persistence.
- A passed automated protocol flow should retain both Host assertions and MCU/bus-side evidence. Reset, power-cycle, storage-media, and bitrate-switch scenarios require the corresponding target-board action.
- GFC/SRDO fixtures validate CANopenNode protocol behavior and observability only. They do not establish functional-safety certification, redundant hardware coverage, WCET proof, or machine-level safety timing.

## 6. Related documents

- [Quick start](quick-start.md)
- [Configuration guide](configuration.md)
- [Object Dictionary guide](object-dictionary.md)
- [NMT Master automatic test](nmt-master-test.md)
- [LSS Node-ID and bitrate persistence](lss-persistence.md)
- [Troubleshooting](troubleshooting.md)
