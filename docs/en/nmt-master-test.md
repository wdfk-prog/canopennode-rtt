[中文](../zh/nmt-master-test.md)

# NMT Master automatic test

## 1. Goal

This feature validates simple CANopenNode NMT Master command transmission and remote-state control on the RT-Thread MCU. The MCU keeps its normal local NMT slave, heartbeat, SDO, PDO and related behavior while additionally using `CO_NMT_sendCommand()` to control a remote test node. `PKG_CANOPENNODE_NMT_MASTER` is not mutually exclusive with those slave-side capabilities.

Recommended topology:

```text
STM32F407 / CANopenNode Node 1
        | NMT CAN-ID 0x000
        | Heartbeat consumer 0x702
        v
TQ8MP / Lely BasicSlave Node 2
```

## 2. Configuration

```text
PKG_CANOPENNODE_APP_AUTO_INIT=y
PKG_CANOPENNODE_AUTO_INIT_NODE_ID=1
PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST=y
PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST_TARGET_NODE_ID=2
PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST_HB_TIMEOUT_MS=1500
PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST_STATE_TIMEOUT_MS=3000
```

The test automatically selects (including `GLOBAL_OD_DYNAMIC`, which makes 0x1016 writes update the Heartbeat Consumer runtime):

```text
PKG_CANOPENNODE_NMT_MASTER
PKG_CANOPENNODE_USING_HB_CONS
PKG_CANOPENNODE_HB_CONS_QUERY_FUNCT
PKG_CANOPENNODE_GLOBAL_OD_DYNAMIC
```

The target Node-ID is limited to 1..127, so broadcast Node-ID 0 cannot be configured. Runtime validation rejects a target equal to the active local Node-ID and transmits no test command.

`CO_demo_nmt_master_bind()` configures one demo OD `0x1016` entry for the target before CAN normal mode. An existing consumer for the same node is reused; otherwise the first unconfigured entry is used. The default Node 2 / 1500 ms value is `0x000205DC`. The previous entry value and the value applied by the demo are saved. Each process iteration verifies that the cached index still belongs to the target Node-ID and that OD 0x1016 still contains the demo-owned value. If an external SDO write reconfigures the entry, the test fails immediately. Cleanup restores the previous value only while ownership is intact; otherwise the external value is preserved.

## 3. Online detection

The test no longer relies on a fixed startup delay. The state machine waits for:

```text
local Node 1 == OPERATIONAL
        ↓
Heartbeat consumer(Node 2) == ACTIVE
        ↓
Node 2 NMT state valid
```

The test waits indefinitely for the first valid periodic heartbeat from Node 2; peer discovery is a fixture-readiness condition, not a protocol-step timeout. The Host peer must therefore enable Producer Heartbeat; 500 ms is recommended with the default 1500 ms consumer timeout. Once Node 2 is visible, if its state is not PRE-OP the MCU sends an extra `ENTER_PREOP` fixture-preparation command and then applies `PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST_STATE_TIMEOUT_MS` while waiting for the PRE-OP heartbeat.

## 4. Automatic sequence

The MCU executes the following non-blocking sequence:

```text
WAIT Node 2 online
    ↓
if needed: PREOP fixture preparation -> wait PRE-OPERATIONAL
    ↓
START       -> wait OPERATIONAL
    ↓
STOP        -> wait STOPPED
    ↓
PREOP       -> wait PRE-OPERATIONAL
    ↓
RESET_COMM  -> wait Boot-up/UNKNOWN -> normalize to PRE-OP if needed
    ↓
RESET_NODE  -> wait Boot-up/UNKNOWN -> normalize to PRE-OP if needed
    ↓
START       -> wait OPERATIONAL
    ↓
PASS
```

The six formal NMT payloads for the default Node 2 target remain:

```text
000#0102
000#0202
000#8002
000#8202
000#8102
000#0102
```

Additional `000#8002` frames are fixture-normalization commands and may appear before the first formal START and after reset completion when the Host software peer automatically starts Operational according to the unchanged MCU-provided EDS.

Normal commands advance only after the Heartbeat Consumer reports the expected remote NMT state. Reset commands must first make the consumer leave ACTIVE and enter `UNKNOWN`, proving that a remote Boot-up was received. If the next active heartbeat reports a state other than PRE-OP, the MCU sends one preparation PREOP and waits for a PRE-OP heartbeat before advancing. This prevents stale pre-reset state from being accepted and keeps the formal sequence independent from Host fixture startup behavior.

`PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST_STATE_TIMEOUT_MS` applies after a fixture-normalization command is sent and to each formal post-command state confirmation. Initial peer discovery itself has no timeout. A `CO_NMT_sendCommand()` error or a normalization/state-transition timeout fails the test immediately; no hidden retry is performed.

## 5. Demo module boundary

The automatic test is implemented in `port/rtthread/demo/CO_demo_nmt_master.c` and configured under `PKG_CANOPENNODE_USING_DEMO_OD`. `CO_app_RTT.c` only invokes the common dispatcher and contains no test state machine.

SConscript adds `CO_demo_nmt_master.c` only when `PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST` is enabled, so the implementation is not compiled when the test is disabled.

Local communication reset clears the demo state. After the new `CO_t` is initialized, `CO_demo_bind()` configures the target Heartbeat Consumer again.

## 6. Host peer requirement

The corresponding Linux Host/master test code lives in [canopen-slave-tester](https://github.com/wdfk-prog/canopen-slave-tester). When validating the MCU NMT Master, that project switches to a Lely `BasicSlave` Node 2 and acts as the remote peer for this test.

The Linux `BasicSlave` Node 2 must produce periodic heartbeat messages. The Host tester first completes `BasicSlave::Reset()` and registers the `OnCommand()` observer, then sets local object `0x1017` to 500 ms; Heartbeat ACTIVE therefore also acts as the Host validation-ready gate. Because NMT RESET_COMM restores communication parameters, the Host rewrites `0x1017=500 ms` only after the reset-completion `ENTER_PREOP` callback, when Lely has finished restoring communication defaults. After the formal final START callback, the Host confirms Producer Heartbeat remains enabled and keeps Node 2 alive for two full heartbeat periods before reporting Host-side success.

This allows the MCU to discover Node 2 even when the MCU starts after Linux; the test no longer depends on seeing the one-time Boot-up emitted at initial Host startup.

Linux continues to use `BasicSlave::OnCommand()` for command callback validation, while the MCU Heartbeat Consumer verifies actual remote state transitions. The evidence is complementary:

```text
MCU:   online + NMT state + reset boot-up
Linux: received command + Lely reset callback sequence
```

This test validates a simple NMT Master command producer and remote-node control behavior; it does not claim a complete NMT network manager.
