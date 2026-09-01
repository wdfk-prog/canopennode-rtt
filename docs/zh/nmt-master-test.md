[English](../en/nmt-master-test.md)

# NMT Master 自动测试

## 1. 目标

该功能验证 RT-Thread MCU 上 CANopenNode 的简单 NMT Master 命令发送与远端状态控制能力。MCU 保持自身 Node-ID 的 NMT slave、Heartbeat、SDO、PDO 等功能，同时使用 `CO_NMT_sendCommand()` 控制远端测试节点。`PKG_CANOPENNODE_NMT_MASTER` 不与这些 slave 功能互斥。

推荐拓扑：

```text
STM32F407 / CANopenNode Node 1
        | NMT CAN-ID 0x000
        | Heartbeat consumer 0x702
        v
TQ8MP / Lely BasicSlave Node 2
```

## 2. 配置

```text
PKG_CANOPENNODE_APP_AUTO_INIT=y
PKG_CANOPENNODE_AUTO_INIT_NODE_ID=1
PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST=y
PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST_TARGET_NODE_ID=2
PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST_HB_TIMEOUT_MS=1500
PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST_STATE_TIMEOUT_MS=3000
```

测试选项自动选择（包括 `GLOBAL_OD_DYNAMIC`，用于让 0x1016 写入同步更新 Heartbeat Consumer runtime）：

```text
PKG_CANOPENNODE_NMT_MASTER
PKG_CANOPENNODE_USING_HB_CONS
PKG_CANOPENNODE_HB_CONS_QUERY_FUNCT
PKG_CANOPENNODE_GLOBAL_OD_DYNAMIC
```

目标 Node-ID 范围固定为 1..127，不允许 broadcast Node-ID 0；运行时如果目标与本机 active Node-ID 相同，测试直接失败且不发送命令。

`CO_demo_nmt_master_bind()` 会在 CAN normal mode 之前为目标节点配置 demo OD `0x1016`。如果已存在该目标的 consumer entry 则复用，否则使用第一个未配置 entry。默认 Node 2 / 1500 ms 对应值为 `0x000205DC`。测试会保存被覆盖 entry 的原值和实际写入值。运行期间每轮都重新确认该 index 仍属于目标 Node-ID 且 0x1016 当前值仍等于 demo 写入值；若外部 SDO 重配置该 entry，测试立即失败。cleanup 仅在 demo 仍拥有该值时恢复原值，所有权已丢失时保留外部新配置，避免覆盖其他测试或主站写入。

## 3. 上线判定

测试不再依赖固定 startup delay。状态机先等待：

```text
local Node 1 == OPERATIONAL
        ↓
Heartbeat consumer(Node 2) == ACTIVE
        ↓
Node 2 NMT state valid
```

首次等待 Node 2 周期 Heartbeat 属于测试夹具 ready 条件，不使用协议步骤 timeout；MCU 可以先启动并持续等待 Host。Host peer 必须启用 Producer Heartbeat，推荐 500 ms，默认 consumer timeout 为 1500 ms。Node 2 一旦可见，如果当前状态不是 PRE-OP，MCU 先额外发送一次 `ENTER_PREOP` 做 fixture preparation，再使用 `PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST_STATE_TIMEOUT_MS` 等待新的 PRE-OP Heartbeat。

## 4. 自动序列

MCU 非阻塞执行：

```text
WAIT Node 2 online
    ↓
必要时：PREOP fixture preparation -> wait PRE-OPERATIONAL
    ↓
START       -> wait OPERATIONAL
    ↓
STOP        -> wait STOPPED
    ↓
PREOP       -> wait PRE-OPERATIONAL
    ↓
RESET_COMM  -> wait Boot-up/UNKNOWN -> 必要时归一化到 PRE-OP
    ↓
RESET_NODE  -> wait Boot-up/UNKNOWN -> 必要时归一化到 PRE-OP
    ↓
START       -> wait OPERATIONAL
    ↓
PASS
```

默认 Node 2 的六条正式 NMT CAN payload 保持：

```text
000#0102
000#0202
000#8002
000#8202
000#8102
000#0102
```

测试准备阶段以及 reset 后如果 software peer 按未修改的 MCU EDS 自动进入 Operational，总线上还可能额外出现 `000#8002`；这些是 fixture PRE-OP 归一化命令，不计入正式六步序列。

普通命令只有在 Heartbeat consumer 返回预期远端 NMT state 后才进入下一步。Reset 命令必须先看到 Heartbeat consumer 从 ACTIVE 进入 `UNKNOWN`，证明收到了远端 Boot-up。随后如果首个有效 Heartbeat 不是 PRE-OP，MCU 会发送一次 preparation PREOP，再等待新的 PRE-OP Heartbeat后才进入下一正式步骤。这样既不会接受 Reset 前的旧状态，也不依赖 Host 修改 EDS startup。

`PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST_STATE_TIMEOUT_MS` 只用于 fixture normalization 发出后的 PRE-OP 确认，以及每条正式命令的状态迁移确认；首次等待 peer online 本身不超时。`CO_NMT_sendCommand()` 失败或 normalization/state transition 超时都会立即标记测试失败，不做隐藏重试。

## 5. Demo 模块边界

自动测试实现位于 `port/rtthread/demo/CO_demo_nmt_master.c`，配置项位于 `PKG_CANOPENNODE_USING_DEMO_OD` 配置域内。`CO_app_RTT.c` 只通过统一 dispatcher 调用，不包含测试状态机。

SConscript 仅在 `PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST` 开启时加入 `CO_demo_nmt_master.c`；测试关闭时不会编译该实现。

本机 communication reset 时 demo 状态会清零；新 `CO_t` 完成初始化后，`CO_demo_bind()` 会重新建立目标 Heartbeat consumer 配置。

## 6. Host peer 要求

对应的 Linux Host/主站测试代码位于 [canopen-slave-tester](https://github.com/wdfk-prog/canopen-slave-tester)。验证 MCU NMT Master 时，该工程切换为 Lely `BasicSlave` Node 2，作为本测试的远端 peer。

Linux `BasicSlave` Node 2 必须持续产生 Heartbeat。Host 测试程序先完成 `BasicSlave::Reset()` 并注册 `OnCommand()` observer，再把本地 `0x1017` 配成 500 ms；Heartbeat ACTIVE 因此也作为 Host validation-ready 门控。由于 NMT RESET_COMM 会恢复通信参数，Host 只在 reset completion 的 `ENTER_PREOP` callback 到达、Lely 已完成通信参数恢复后重新写入 `0x1017=500 ms`。formal final START callback 后 Host 确认该 Producer Heartbeat 仍启用，并保持两个完整 heartbeat 周期后才报告 Host 侧 PASS。

这样 MCU 即使晚于 Linux 启动，也能通过后续周期 Heartbeat 判断 Node 2 已上线，不依赖捕获最开始的一次 Boot-up。

Linux 仍使用 `BasicSlave::OnCommand()` 验证命令 callback 序列；MCU Heartbeat consumer 则验证远端实际状态变化。两侧证据互补：

```text
MCU:   online + NMT state + reset boot-up
Linux: received command + Lely reset callback sequence
```

该测试验证简单 NMT Master command producer 和远端节点控制行为，不等价于完整 NMT network manager。
