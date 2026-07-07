[English](../en/troubleshooting.md)

# 故障排查

本文列出常见集成失败和最快检查路径。

## 1. `PKG_USING_CANOPENNODE` 不可见

可能原因：

- 必要 RT-Thread 功能未启用。
- 父工程没有包含本软件包的 `Kconfig`。

先确认以下依赖已启用：

```text
RT_USING_HEAP
RT_USING_DEVICE
RT_USING_CAN
RT_USING_MUTEX
RT_USING_SEMAPHORE
```

再确认父级 package menu 已包含本软件包的 `Kconfig`。

## 2. 构建提示 `CANopenNode` 或 `CANopenNode/301` 缺失

可能原因：git 子模块没有初始化。

在仓库根目录执行：

```sh
git submodule update --init --recursive
```

随后验证：

```sh
test -d CANopenNode/301 && echo ok
```

## 3. 构建提示 `examples/demo_device/OD.h` 缺失

可能原因：

- `PKG_CANOPENNODE_USING_DEMO_OD` 已启用，但 demo OD 文件不存在。
- 软件包根目录结构被改变。

修复方式：

- 恢复 `examples/demo_device/OD.c` 和 `OD.h`。
- 或关闭 `PKG_CANOPENNODE_USING_DEMO_OD`，由应用/BSP 编译产品 `OD.c`。

## 4. 初始化在 CANopen 启动前失败

可能原因：

| 原因 | 检查 |
|---|---|
| CAN 设备名错误 | 使用 RT-Thread device list 命令，或检查 BSP CAN 注册名称。 |
| CAN 驱动未启用 | 确认 `RT_USING_CAN` 和 BSP CAN 外设支持。 |
| Node-ID 无效 | 使用 `1..127`，除非明确使用 LSS 未配置模式。 |
| Bitrate 无效 | 使用非零且 BSP 驱动支持的 bitrate。 |
| auto/manual init 重复 | 使用显式 `canopen_app_rtt_init()` 时关闭 `PKG_CANOPENNODE_APP_AUTO_INIT`。 |

## 5. 没有 CANopen boot-up 帧

按以下顺序检查：

1. CAN 物理接线、收发器使能脚和终端电阻。
2. 总线上所有节点 bitrate 是否一致。
3. RT-Thread CAN 设备名和 open/configuration 状态。
4. CAN 控制器是否处于 normal mode，而不是 silent 或 loopback mode。
5. CAN 分析仪是否能看到目标板发送的任意帧。
6. 启用 `PKG_CANOPENNODE_USING_DEBUG` 后，初始化日志是否报错。

Node-ID 为 `1` 时，通常 boot-up 帧 COB-ID 为 `0x701`，payload 为 `00`。

## 6. SDO timeout

可能原因：

- `PKG_CANOPENNODE_USING_SDO_SERVER` 未启用。
- master 使用了错误 Node-ID。
- 总线 bitrate 或终端电阻错误。
- OD 0x1200 server parameters 与预期默认 COB-ID 不一致。
- 节点尚未完成初始化。

检查：

```text
Node-ID 1 default SDO request COB-ID: 0x601
Node-ID 1 default SDO response COB-ID: 0x581
```

同时确认 tester/master 使用 classic CAN frame 和正确 bitrate。

## 7. PDO 不更新

检查：

1. `PKG_CANOPENNODE_USING_PDO` 已启用。
2. 目标方向对应的 `PKG_CANOPENNODE_RPDO` 或 `PKG_CANOPENNODE_TPDO` 已启用。
3. 节点处于 NMT operational 状态。
4. OD PDO communication 和 mapping parameters 合法。
5. event-driven TPDO 已配置 event timer 或事件触发逻辑。
6. synchronous PDO 场景存在 SYNC，且 `PKG_CANOPENNODE_PDO_SYNC` 已启用。
7. 应用线程访问共享 OD 变量时已做好保护。

## 8. CAN RX 只能走软件分发

这可能是正常行为。RT-Thread CAN HDR filtering 是可选能力，依赖 BSP 支持和可用 filter banks。如果 `PKG_CANOPENNODE_USING_RTT_CAN_FILTER` 已启用，但驱动无法将所有 RX buffers 分配到 HDR banks，本移植层会回退到软件分发。

软件分发适合 bring-up 和很多产品场景。只有在测量 CPU 负载和总线流量后，才需要重新评估硬件过滤器优化。

## 9. Storage 没有持久化

检查：

| Backend | 检查项 |
|---|---|
| DFS | `RT_USING_DFS` 已启用，mount point 存在，配置目录存在，路径长度符合 `PKG_CANOPENNODE_STORAGE_DFS_MAX_PATH`。 |
| EEPROM | `PKG_USING_AT24CXX` 已启用，I2C bus name 正确，AT24CXX address input 正确，storage offset 未与其他数据重叠。 |
| User | 应用提供 strong `co_storage_rtt_backend_get_ops()` 符号和长期有效的 ops table。 |

还要确认生成 OD 提供选中的 `OD_PERSIST_*` groups，并通过 SDO 测试 OD 0x1010/0x1011 访问。

## 10. Trace 不能启用

这是有意设计。`PKG_CANOPENNODE_USING_TRACE` 依赖一个不可用的内部选项，因为当前 trace 模块尚未适配本软件包使用的 SDO server 和 Object Dictionary APIs。

不要通过修改生成配置强行启用 trace。bring-up 诊断可使用 `PKG_CANOPENNODE_USING_DEBUG` 或 `PKG_CANOPENNODE_USING_FRAME_TRACE`。

## 11. 多实例问题

检查：

- 绑定多个 RT-Thread CAN 设备时，增大 `PKG_CANOPENNODE_CAN_BINDING_COUNT`。
- 使用独立的 `CANopenNodeRTT` 实例。
- 使用不同 CAN 设备名，或明确定义绑定所有权。
- 同一 CANopen 网络上使用唯一 Node-ID。
- 内置 AT24CXX EEPROM backend 不适合多实例，它被刻意限制为单 binding。

## 12. 调试策略

复杂 bring-up 建议：

1. 如果可用 `RT_USING_ULOG`，启用 `PKG_CANOPENNODE_USING_DEBUG`。
2. 仅临时启用 `PKG_CANOPENNODE_USING_FRAME_TRACE` 查看 RX/TX 帧。
3. 先使用 demo OD 和默认 SDO server。
4. 先验证 boot-up，再验证 SDO，再验证 PDO。
5. 基础通信稳定后再加入 storage。
6. 运行时和总线行为确认后，再替换 demo OD。
