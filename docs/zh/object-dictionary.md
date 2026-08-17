[English](../en/object-dictionary.md)

# Object Dictionary 指南

Object Dictionary 是通过 CANopen 暴露的应用数据模型。本软件包自带 demo OD 便于 bring-up，但产品固件通常应提供自己的 OD。

## 1. Demo OD 目录

自带 demo 文件位于：

```text
examples/demo_device/
├── OD.c
├── OD.h
├── project.eds
├── project.md
└── project.xdd
```

当 `PKG_CANOPENNODE_USING_DEMO_OD` 启用时，`SConscript` 会编译 `examples/demo_device/OD.c`，并将 `examples/demo_device/` 加入 include path。

## 2. 什么时候使用 demo OD

以下场景适合使用 demo OD：

- 首次 bring-up RT-Thread CAN 驱动和本软件包；
- 验证基础 NMT、heartbeat、SDO、SYNC 或 PDO 运行路径；
- 检查目标是否发送预期 CANopen boot-up 帧；
- 在不引入产品 OD 复杂度的情况下测试线程优先级、CAN 接收分发和日志。

不要把 demo OD 当作最终产品数据模型。

## 3. 替换 demo OD

产品 OD 接入步骤：

1. 基于产品 CANopen 对象模型生成 `OD.c` 和 `OD.h`。
2. 将生成文件放入应用或 BSP 源码树。
3. 关闭 `PKG_CANOPENNODE_USING_DEMO_OD`。
4. 在应用/BSP SConscript 中加入产品 `OD.c`。
5. 将产品 `OD.h` 所在目录加入 include path，并确保本软件包构建时可以找到。
6. 确认启用的 CANopenNode 功能组与生成 OD 中存在的对象匹配。

应用侧 SCons 示例模式：

```python
src += [os.path.join(cwd, 'canopen_od', 'OD.c')]
CPPPATH += [os.path.join(cwd, 'canopen_od')]
```

实际 SCons 写法取决于应用仓库目录结构。

## 4. OD 与 storage group

启用 storage 时，封装层可以为选中的生成 OD persistence groups 创建 storage entries：

| Kconfig 选项 | 需要生成的符号 | OD 0x1010/0x1011 sub-index |
|---|---|---:|
| `PKG_CANOPENNODE_STORAGE_PERSIST_COMM` | `OD_PERSIST_COMM` | `2` |
| `PKG_CANOPENNODE_STORAGE_PERSIST_APP` | `OD_PERSIST_APP` | `3` |
| `PKG_CANOPENNODE_STORAGE_PERSIST_MANU` | `OD_PERSIST_MANU` | `4` |

只启用生成 `OD.h` 中实际存在的 group。如果开启 storage 但没有选择任何存在的 persistence group，应视为配置错误。

## 5. OD 与 PDO mapping

PDO 行为取决于生成 OD entries 和 Kconfig 选项：

- `PKG_CANOPENNODE_USING_PDO` 必须启用 PDO objects。
- `PKG_CANOPENNODE_RPDO` 控制 receive PDO 支持。
- `PKG_CANOPENNODE_TPDO` 控制 transmit PDO 支持。
- `PKG_CANOPENNODE_PDO_SYNC` 用于同步 PDO 行为。
- `PKG_CANOPENNODE_PDO_OD_IO_ACCESS` 使 PDO mapping 通过 OD accessor 访问，而不是直接访问内存映射。

替换 OD 时需要验证：

1. RPDO communication parameters 和 mapping entries 合法。
2. TPDO communication parameters、event timers、inhibit times 和 mapping entries 符合产品数据路径。
3. 期望 PDO 通信时，NMT 状态应处于 operational。
4. 应用代码在多个 RT-Thread 线程间共享 OD 变量时需要做好保护。

## 6. OD 与 SDO

SDO 访问要求 `PKG_CANOPENNODE_USING_SDO_SERVER` 开启，并且 OD access attributes 配置兼容。产品 OD entries 需要确认：

- 读写权限符合诊断或配置意图；
- 数据长度与 tester/master 预期一致；
- 大对象传输需要开启 segmented 或 block transfer；
- write callback 或 OD extension 在 RT-Thread 线程上下文中是安全的。

## 7. OD 与 identity objects

产品化前，应明确设置 identity 和描述对象：

| Object | 用途 |
|---|---|
| `0x1000` | Device type。 |
| `0x1008` | Manufacturer device name。 |
| `0x1009` | Manufacturer hardware version。 |
| `0x100A` | Manufacturer software version。 |
| `0x1018` | Identity object，包括 vendor ID、product code、revision、serial number。 |

这些值通常会被 CANopen master、commissioning tool 和现场诊断工具读取。

## 8. Demo TIME consumer 诊断

生成的 demo OD 提供 manufacturer-specific 记录 `0x2300`，用于 TIME consumer 自动验证：

| Sub-index | 类型 | 访问 | 含义 |
|---:|---|---|---|
| `0x01` | `UNSIGNED32` | 只读 | CANopenNode callback-pre 观察到的合法 DLC=6 TIME 帧计数。 |
| `0x02` | `UNSIGNED32` | 只读 | 实际 `CO_TIME_t::ms`，即午夜后的毫秒数。 |
| `0x03` | `UNSIGNED16` | 只读 | 实际 `CO_TIME_t::days`，即自 1984-01-01 起的 day count。 |

该记录位于 RAM，不可 PDO mapping。只有开启 `PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC` 时运行时才会更新这些值。接收计数保存在 RT-Thread wrapper 实例中，因此 CANopen communication reset 不会清零；应用毫秒/day 字段始终来自当前 `CO_TIME_t`，并在 `CO_process()` 之后发布。

该对象只是 demo/test 可观察性契约，不属于标准 CiA 301 TIME 对象。产品固件使用自定义 OD 时，应根据自身验证策略提供等价的应用结果接口。

## 9. Demo EMCY consumer 诊断

生成的 demo OD 提供 manufacturer-specific 记录 `0x2301`，用于 EMCY Consumer 自动验证：

| Sub-index | 类型 | 访问 | 含义 |
|---:|---|---|---|
| `0x01` | `UNSIGNED32` | 只读 | 收到的远端 EMCY callback 数量。 |
| `0x02` | `UNSIGNED8` | 只读 | 根据最近一次远端 EMCY CAN-ID 推导出的来源 Node-ID。 |
| `0x03` | `UNSIGNED16` | 只读 | 最近一次远端 EMCY 的 CAN-ID。 |
| `0x04` | `UNSIGNED16` | 只读 | 最近一次 EMCY error code。 |
| `0x05` | `UNSIGNED8` | 只读 | 最近一次 EMCY error register。 |
| `0x06` | `UNSIGNED8` | 只读 | 最近一次远端 EMCY 的 CANopenNode callback `errorBit`，即 EMCY byte 3 / manufacturer-specific field 首字节。 |
| `0x07` | `UNSIGNED32` | 只读 | 最近一次 manufacturer-specific info code。 |

接收 callback 使用 RT-Thread atomic 字段和奇偶 sequence counter 更新快照；`CO_demo_process()` 只有在前后 sequence 一致且为偶数时才把一次完整接收侧快照发布到 OD。由于各字段属于独立 SDO sub-index，Host 组合读取多个字段时应先读一次 `remote_rx_count`、再读取其余字段、最后再次读取 `remote_rx_count`；前后计数不同则重试。接收计数和最近一次远端 EMCY 快照会在 communication reset 时保留，同时 callback 会重新绑定到重建后的 CANopenNode stack。CANopenNode 以 `ident == 0` 报告的本节点 EMCY 会被过滤；重复远端 EMCY 以及 error code 为零的恢复消息都分别计数。该记录位于 RAM，不可 PDO mapping。

该对象只是 demo/test 可观察性契约，不承担产品级远端故障管理。

## 10. GFC 参数与 demo 协议诊断

生成的 demo OD 包含标准 GFC 参数 `0x1300:00`，类型为可读写 `UNSIGNED8`，默认值为 `1`。CANopenNode 将 `0` 解释为 GFC 无效、`1` 解释为 GFC 有效；大于 `1` 的值由 GFC OD extension 拒绝。

为支持 J03/B09G 自动协议验证，manufacturer-specific 记录 `0x2302` 只暴露测试控制和证据：

| Sub-index | 类型 | 访问 | 含义 |
|---:|---|---|---|
| `0x01` | `UNSIGNED32` | 只读 | 合法 GFC consumer callback 的累计次数。 |
| `0x02` | `UNSIGNED8` | 只读 | 收到过合法 GFC 后置位的粘性协议测试标志。 |
| `0x03` | `UNSIGNED32` | 读写 | Host 写入的 producer request sequence。 |
| `0x04` | `UNSIGNED32` | 只读 | MCU mainline 已消费的最近 producer request sequence。 |
| `0x05` | `INTEGER32` | 只读 | 最近一次 mainline `CO_GFCsend()` 调用结果。 |

CAN receive callback 只更新 RT-Thread atomic 接收证据；只有 mainline 观察到新的 request sequence 后才调用 `CO_GFCsend()`。communication reset 会保留 consumer 接收证据，同时在新 GFC 对象重新绑定时同步 producer request/completion，避免旧请求在新 stack 上重放。

该记录仅用于 GFC 协议功能验证，不控制真实执行器、不实现产品安全状态，也不代表 SIL、PL 或 EN 50325-5 系统级合规。

## 11. 验证清单

产品构建替换 demo OD 前，确认：

- `PKG_CANOPENNODE_USING_DEMO_OD` 已关闭。
- 产品 `OD.c` 只被编译一次。
- include path 能找到产品 `OD.h`。
- 启用的 CANopenNode Kconfig 功能在 OD 中有匹配对象。
- 产品 entries 的 SDO 访问符合预期。
- PDO mappings 合法，且不超过 classic CAN 帧长度。
- storage groups 与生成的 `OD_PERSIST_*` 符号匹配。
- identity objects 和 Node-ID 策略已经产品化。
- 跨线程共享的 OD 访问已经加锁保护。
