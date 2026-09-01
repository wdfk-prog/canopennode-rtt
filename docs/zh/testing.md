[English](../en/testing.md)

# 测试与验证

本仓库提供 MCU/RT-Thread 侧的 CANopenNode 集成、demo/test Object Dictionary、协议测试夹具和目标侧可观察性。对应的 Linux Host/主站自动协议测试代码维护在 [canopen-slave-tester](https://github.com/wdfk-prog/canopen-slave-tester)。

`canopen-slave-tester` 基于 Lely CANopen。其默认角色是测试主站，用于通过真实 CAN 总线驱动和检查 MCU 节点；验证本仓库的 NMT Master 能力时，Host 可切换为 Lely `BasicSlave` Node 2，作为 MCU 控制的远端节点。Host 当前实际启用的自动流程由该仓库自身配置决定，不在本仓库复制维护。

## 1. 两个仓库的职责

| 仓库 | 主要职责 |
|---|---|
| `canopennode-rtt` | RT-Thread CAN target driver、CANopenNode 生命周期与线程封装、Kconfig/SCons 集成、storage backend、demo OD、MCU 侧测试夹具与诊断记录。 |
| `canopen-slave-tester` | Linux/Lely Host 或测试主站、自动协议流程、主站侧断言、抓取测试证据；NMT Master 验证时可作为远端 Lely Slave。 |

两边通过 CANopen 标准对象、CAN 帧和本仓库的 test-only OD 记录交互，不共享进程内 API，也不要求合并源码构建。

## 2. 推荐联调拓扑

常规从机能力验证：

```text
Linux / Lely canopen-slave-tester
           |
           | SocketCAN / CAN
           v
RT-Thread MCU / canopennode-rtt
```

MCU NMT Master 能力验证：

```text
RT-Thread MCU / CANopenNode Node 1
           |
           | NMT + Heartbeat
           v
Linux / Lely BasicSlave Node 2
(canopen-slave-tester Slave role)
```

NMT Master 的目标配置和命令序列见 [NMT Master 自动测试](nmt-master-test.md)。

## 3. MCU 侧测试夹具

以下对象用于协议自动验证和目标侧可观察性，不应当被当作产品应用 OD：

| 能力 | Kconfig / 对象 | MCU 侧证据 |
|---|---|---|
| TIME consumer | `PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC`, `0x2300` | 合法 TIME 接收计数以及已应用的 ms/day。 |
| EMCY consumer | `PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC`, `0x2301` | 远端 EMCY 接收计数与一致的最近消息快照。 |
| GFC | `PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC`, `0x1300` + `0x2302` | consumer 接收证据以及 producer request/result sequence。 |
| MCU SDO Client | `PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST`, `0x2303` | request/active/completion sequence、传输方向、长度和原生 SDO 结果。 |
| SDO Server Block | `PKG_CANOPENNODE_DEMO_SDO_BLOCK_TEST`, `0x2304` | 有界 `DOMAIN` payload，用于 block-transfer 与 CRC 路径验证。 |
| EEPROM Storage | `PKG_CANOPENNODE_DEMO_STORAGE_DIAGNOSTIC`, `0x2305` | storage 启动结果、原始区域备份/恢复与受控破坏入口。 |
| SRDO | `PKG_CANOPENNODE_DEMO_SRDO_DIAGNOSTIC`, `0x2306` | 确定性 SRDO channel 状态和 TX request 可观察性。 |
| NMT Master | `PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST`, `0x1016` Heartbeat Consumer | 远端上线、NMT 状态迁移、reset boot-up 与命令结果。 |
| LSS 持久化 | 标准 LSS 服务 + storage backend | Node-ID/bitrate configure、Store、Reset Node、power-cycle 与运行时 bitrate 切换结果。 |

对象的字段定义和限制见 [Object Dictionary 指南](object-dictionary.md)，配置依赖见 [配置指南](configuration.md)。

## 4. 联调流程

1. 在 MCU 工程中启用本次验证实际需要的 CANopen 功能和 demo/test 夹具，不要为了“全开”而引入无关功能。
2. 使用产品 BSP 完成固件构建、烧录和基础 CAN bring-up；确认 Node-ID、bitrate 与 Host 配置一致。
3. 按 `canopen-slave-tester` 当前 README/配置准备 Linux Host。Host 仓库的构建、DCF/EDS 和部署方式以其自身文档为准，本仓库不复制这些命令。
4. 在真实 CAN 总线上运行对应协议验证，同时保留 Host 日志、MCU 日志和必要的 `candump` 证据。
5. 涉及临时 OD/通信参数修改的测试必须恢复原值；涉及持久化的测试还要执行 Reset Node 和真实 power-cycle 复核。
6. 测试结束后关闭不应进入产品固件的 demo/test Kconfig 选项，并使用产品 OD 重新验证实际配置。

## 5. 证据边界

- Host 代码能编译或语法检查通过，只说明 Host 工具本身可构建，不等于目标板协议行为通过。
- MCU 软件包能通过静态检查或 CI 编译，只说明对应配置可以进入构建图，不等于真实 CAN 时序、硬件过滤器、EEPROM、bitrate 切换或掉电持久化已经验证。
- 自动协议测试通过时，应同时保留 Host 断言与 MCU/总线侧证据；涉及 reset、power-cycle、存储介质或 bitrate 切换的场景必须有对应目标板动作。
- GFC/SRDO 测试夹具只验证 CANopenNode 协议行为和可观察性，不代表功能安全认证、冗余硬件覆盖、WCET 或机器级安全时序已经满足。

## 6. 相关文档

- [快速接入](quick-start.md)
- [配置指南](configuration.md)
- [Object Dictionary 指南](object-dictionary.md)
- [NMT Master 自动测试](nmt-master-test.md)
- [LSS Node-ID 与 bitrate 持久化](lss-persistence.md)
- [故障排查](troubleshooting.md)
