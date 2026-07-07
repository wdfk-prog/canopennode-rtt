[English](../en/configuration.md)

# 配置指南

本软件包主要通过 `Kconfig` 配置。生成的配置会控制 RT-Thread 运行参数、参与编译的 CANopenNode 源文件，以及 CANopenNode `CO_CONFIG_*` 功能标志。

本文总结集成时最常用的选项。默认值和依赖关系以 `Kconfig` 文件为准。

## 1. 核心使能选项

| 选项 | 默认值 | 说明 |
|---|---:|---|
| `PKG_USING_CANOPENNODE` | `n` | 启用本软件包。依赖 heap、device、CAN、mutex 和 semaphore 支持。 |

该选项关闭时，本软件包不会向构建组添加源文件。

## 2. CAN 设备绑定

| 选项 | 默认值 | 说明 |
|---|---:|---|
| `PKG_CANOPENNODE_CAN_DEV_NAME` | `"can1"` | 软件包通用 RT-Thread CAN 设备名。 |
| `PKG_CANOPENNODE_CAN_BINDING_COUNT` | `1` | RT-Thread CAN 设备绑定表大小。多实例设计时才需要增大。 |
| `PKG_CANOPENNODE_USING_RTT_CAN_FILTER` | `n` | BSP 支持且所有 RX buffer 可分配过滤器时，使用 RT-Thread CAN HDR filters。 |

如果配置的 CAN 设备名无法被 BSP 的 `rt_device_find()` 找到，初始化会在 CANopen 通信启动前失败。

## 3. 运行线程选项

| 选项 | 默认值 | 用途 |
|---|---:|---|
| `PKG_CANOPENNODE_RX_THREAD_STACK_SIZE` | `2048` | CAN RX helper 线程栈。 |
| `PKG_CANOPENNODE_RX_THREAD_PRIORITY` | `2` | RX helper 线程优先级。RT-Thread 中数值越小优先级越高。 |
| `PKG_CANOPENNODE_RX_THREAD_TICK` | `10` | RX helper 时间片。 |
| `PKG_CANOPENNODE_RX_BATCH_SIZE` | `8` | 每轮 RX 最多读取的 CAN 帧数。 |
| `PKG_CANOPENNODE_MAIN_THREAD_STACK_SIZE` | `2048` | mainline 线程栈。 |
| `PKG_CANOPENNODE_MAIN_THREAD_PRIORITY` | `10` | mainline 处理优先级。 |
| `PKG_CANOPENNODE_MAIN_THREAD_TICK` | `10` | mainline 时间片。 |
| `PKG_CANOPENNODE_RT_THREAD_STACK_SIZE` | `2048` | realtime 处理线程栈。 |
| `PKG_CANOPENNODE_RT_THREAD_PRIORITY` | `3` | realtime 处理优先级。 |
| `PKG_CANOPENNODE_RT_THREAD_TICK` | `10` | realtime 时间片。 |
| `PKG_CANOPENNODE_TIMER_PERIOD_US` | `1000` | realtime 处理请求周期，单位微秒。 |

典型 CANopen 设备推荐优先级关系：

```text
RX helper priority <= realtime priority < mainline priority
```

由于 RT-Thread 数值越小优先级越高，默认 RX helper 优先级 `2`、realtime 优先级 `3`、mainline 优先级 `10` 符合该模型。

## 4. 应用自动初始化

| 选项 | 默认值 | 说明 |
|---|---:|---|
| `PKG_CANOPENNODE_APP_FIRST_HB_TIME_MS` | `500` | 首次 heartbeat producer 延迟。 |
| `PKG_CANOPENNODE_APP_SDO_SRV_TIMEOUT_MS` | `1000` | SDO server timeout。 |
| `PKG_CANOPENNODE_APP_SDO_CLI_TIMEOUT_MS` | `500` | 默认 SDO client timeout。 |
| `PKG_CANOPENNODE_APP_SDO_CLI_BLOCK` | `n` | 支持时默认启用 SDO client block transfer。 |
| `PKG_CANOPENNODE_APP_AUTO_INIT` | `y` | RT-Thread 应用初始化阶段创建一个默认实例。 |
| `PKG_CANOPENNODE_AUTO_INIT_CAN_DEV_NAME` | `"can1"` | auto init 使用的 CAN 设备名。 |
| `PKG_CANOPENNODE_AUTO_INIT_NODE_ID` | `1` | auto init 使用的 Node-ID。 |
| `PKG_CANOPENNODE_AUTO_INIT_BITRATE` | `1000` | auto init 使用的 bitrate。 |

产品需要显式创建 `CANopenNodeRTT` 实例时，应关闭 `PKG_CANOPENNODE_APP_AUTO_INIT`。

## 5. CANopenNode 通用标志

| 选项 | 默认值 | 说明 |
|---|---:|---|
| `PKG_CANOPENNODE_GLOBAL_CALLBACK_PRE` | `n` | 启用支持对象的 mainline callback-pre hooks。 |
| `PKG_CANOPENNODE_GLOBAL_RT_CALLBACK_PRE` | `n` | 启用支持对象的 realtime callback-pre hooks。 |
| `PKG_CANOPENNODE_GLOBAL_TIMERNEXT` | `n` | 支持对象计算 `timerNext_us`。 |
| `PKG_CANOPENNODE_GLOBAL_OD_DYNAMIC` | `y` | 支持对象在 OD 写入后重新读取通信参数。 |

除非产品明确禁止运行时修改通信参数，否则建议保持 dynamic OD 启用。

## 6. CiA 301 服务

### NMT 和 heartbeat

| 选项 | 默认值 | 说明 |
|---|---:|---|
| `PKG_CANOPENNODE_NMT_CALLBACK_CHANGE` | `n` | 本地 NMT 状态变化时触发应用回调。 |
| `PKG_CANOPENNODE_NMT_MASTER` | `n` | 允许本节点发送简单 NMT master 命令。 |
| `PKG_CANOPENNODE_USING_HB_CONS` | `y` | heartbeat consumer 支持。 |

### 旧式 node guarding

| 选项 | 默认值 | 说明 |
|---|---:|---|
| `PKG_CANOPENNODE_USING_NODE_GUARDING` | `n` | 仅用于旧系统兼容。demo CI profile 不启用；新设计优先使用 heartbeat producer/consumer 和 heartbeat time 配置。 |

Node guarding slave 还要求 OD 中存在 0x100C Guard Time 和 0x100D Lifetime Factor。当前 demo OD 没有这两个对象，因此 demo CI 矩阵明确排除 Node Guarding。

### Emergency

| 选项 | 默认值 | 说明 |
|---|---:|---|
| `PKG_CANOPENNODE_EM_PRODUCER` | `y` | 本地节点可以发送 EMCY 消息。 |
| `PKG_CANOPENNODE_EM_HISTORY` | `y` | 将近期 emergency 记录存入 OD 0x1003。 |
| `PKG_CANOPENNODE_EM_CONSUMER` | `n` | 接收远端节点 EMCY 消息。 |
| `PKG_CANOPENNODE_EM_ERR_STATUS_BITS_COUNT` | `80` | 错误状态 bit 数，合法范围 48..256 且为 8 的倍数。 |

### SDO

| 选项 | 默认值 | 说明 |
|---|---:|---|
| `PKG_CANOPENNODE_USING_SDO_SERVER` | `y` | master/tool 访问本节点 OD 通常需要开启。 |
| `PKG_CANOPENNODE_SDO_SRV_SEGMENTED` | `y` | 允许 server 传输大于 expedited frame 的对象。 |
| `PKG_CANOPENNODE_SDO_SRV_BLOCK` | `n` | 提升大对象传输效率，但需要更大 buffer 和 CRC16。 |
| `PKG_CANOPENNODE_SDO_SRV_BUFFER_SIZE` | `32` 或 `1000` | server block transfer 开启时默认更大。 |
| `PKG_CANOPENNODE_USING_SDO_CLIENT` | `n` | master、gateway 或本地工具访问远端节点时开启。 |
| `PKG_CANOPENNODE_SDO_CLI_BLOCK` | `n` | segmented transfer 开启时可启用 SDO client block transfer。 |
| `PKG_CANOPENNODE_SDO_CLI_BUFFER_SIZE` | `32` | SDO client 内部环形 buffer 大小。 |

### TIME、SYNC 和 PDO

| 选项 | 默认值 | 说明 |
|---|---:|---|
| `PKG_CANOPENNODE_USING_TIME` | `y` | 默认支持 TIME consumer。 |
| `PKG_CANOPENNODE_TIME_PRODUCER` | `n` | 只有网络时间生产者节点需要开启。 |
| `PKG_CANOPENNODE_USING_SYNC` | `y` | SYNC object 支持。 |
| `PKG_CANOPENNODE_SYNC_PRODUCER` | `y` | 本节点可以产生 SYNC。需要结合网络拓扑确认。 |
| `PKG_CANOPENNODE_USING_PDO` | `y` | PDO object 支持。 |
| `PKG_CANOPENNODE_RPDO` | `y` | 接收 PDO 支持。 |
| `PKG_CANOPENNODE_TPDO` | `y` | 发送 PDO 支持。 |
| `PKG_CANOPENNODE_PDO_SYNC` | `y` | PDO 同步传输支持。 |
| `PKG_CANOPENNODE_PDO_OD_IO_ACCESS` | `y` | PDO mapping 通过 OD IO accessors 访问。 |

## 7. Storage

| 选项 | 默认值 | 说明 |
|---|---:|---|
| `PKG_CANOPENNODE_USING_STORAGE` | `n` | 启用 CANopenNode storage 和 OD 0x1010/0x1011 行为。 |
| `PKG_CANOPENNODE_STORAGE_MAX_ENTRIES_COUNT` | `1` | 支持的最大 storage entry 数。 |
| `PKG_CANOPENNODE_STORAGE_PERSIST_COMM` | `y` | 持久化通信参数。要求生成的 OD 提供 `OD_PERSIST_COMM`。 |
| `PKG_CANOPENNODE_STORAGE_PERSIST_APP` | `n` | 持久化应用参数。要求生成的 OD 提供 `OD_PERSIST_APP`。 |
| `PKG_CANOPENNODE_STORAGE_PERSIST_MANU` | `n` | 持久化厂商参数。要求生成的 OD 提供 `OD_PERSIST_MANU`。 |

Storage backend 选择：

| Backend 选项 | 依赖 | 说明 |
|---|---|---|
| `PKG_CANOPENNODE_USING_STORAGE_DFS` | `RT_USING_DFS` | 文件持久化 backend。目录必须已存在。默认不保证掉电安全。 |
| `PKG_CANOPENNODE_USING_STORAGE_EEPROM` | `PKG_USING_AT24CXX` 且单 CAN binding | 使用 AT24CXX adapter 和 CANopenNode EEPROM storage。 |
| `PKG_CANOPENNODE_USING_STORAGE_USER` | 应用 backend | 应用必须提供 `co_storage_rtt_backend_get_ops()`。 |

开启 storage 时，必须且只能选择一个 storage backend。

## 8. LED、安全、LSS 和 gateway 选项

| 分组 | 主选项 | 默认值 | 说明 |
|---|---|---:|---|
| LED | `PKG_CANOPENNODE_USING_LEDS` | `y` | 启用 CiA 303-3 run/error LED 状态计算。 |
| RT-Thread PIN LED | `PKG_CANOPENNODE_LEDS_USING_RTT_PIN` | `n` | 驱动配置的 RT-Thread PIN 输出。 |
| GFC | `PKG_CANOPENNODE_USING_GFC` | `n` | 安全相关对象；仅在安全设计已验证时开启。 |
| SRDO | `PKG_CANOPENNODE_USING_SRDO` | `n` | 安全相关 PDO；需要 CRC16。 |
| LSS slave | `PKG_CANOPENNODE_USING_LSS_SLAVE` | `y` | 允许通过 LSS master 配置 Node-ID 和 bitrate。 |
| LSS master | `PKG_CANOPENNODE_USING_LSS_MASTER` | `n` | commissioning tool 或 gateway 场景开启。 |
| ASCII gateway | `PKG_CANOPENNODE_USING_GATEWAY_ASCII` | `n` | 仅编译 gateway helper；命令传输由应用提供。 |

## 9. Helper 和 debug 选项

| 选项 | 默认值 | 说明 |
|---|---:|---|
| `PKG_CANOPENNODE_USING_CRC16` | `n` | 需要 CRC16 的功能会自动选择。 |
| `PKG_CANOPENNODE_CRC16_EXTERNAL` | `n` | 仅当应用提供 CRC16 实现时开启。 |
| `PKG_CANOPENNODE_USING_FIFO` | `n` | SDO client 或 ASCII gateway 相关功能会自动选择。 |
| `PKG_CANOPENNODE_USING_TRACE` | 不可用 | 在 trace 适配当前 API 前刻意禁用。 |
| `PKG_CANOPENNODE_USING_DEBUG` | `n` | 启用本移植层的 RT-Thread ulog 诊断日志。 |
| `PKG_CANOPENNODE_USING_FRAME_TRACE` | `n` | 打印 CAN RX/TX 帧。仅建议 bring-up 诊断使用。 |

## 10. Demo Object Dictionary

| 选项 | 默认值 | 说明 |
|---|---:|---|
| `PKG_CANOPENNODE_USING_DEMO_OD` | `y` | 编译 `examples/demo_device/OD.c`，并添加 demo OD include path。 |

当 BSP 或应用通过自己的 SConscript 和 include path 提供自定义生成的 `OD.c` 与 `OD.h` 时，应关闭该选项。
