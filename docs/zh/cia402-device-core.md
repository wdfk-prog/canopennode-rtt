[English](../en/cia402-device-core.md)

# CiA 402 Pure-C Device Core 与 OD Binding

本文说明多轴 Pure-C Device core 的实现边界和运行时契约。该核心不引入 RT-Thread、STM32/BSP 或运行时 OD 生成逻辑。

## 1. Device core 职责

Device core 把已经由 XDD 生成的 CiA 402 对象绑定到本地多轴运行时，并为每个 axis 提供独立 PDS supervisor：

- `CO_402_device_manager_t` 管理 caller-owned axis/config/runtime 数组；
- `logicalDevice` 映射到对应的 profile OD base；
- 初始化时拒绝重复 logical-device；
- 查找、校验并缓存每个 axis 的必需 OD entry；
- 在 Controlword 和 Modes of operation 上建立受控 OD extension ownership；
- 解码 Controlword、执行 PDS 状态迁移并编码 Statusword；
- 用 `CO_402_drive_if_t` 把设备状态机与产品电机/功率级实现隔离；
- DriveIF 的 `BUSY/DONE/ERROR` 允许状态迁移跨多个 supervisor 周期完成；
- 所有 manager/axis/config/DriveIF storage 均由应用持有，Device core 不分配 heap。

![CiA 402 Pure-C Device core](../assets/cia402-device-core.svg)

Device core 不实现 RT-Thread worker、STM32 HAL、PWM/ADC/FOC、PP/PV/HM/CSP/CSV/CST mode runtime，也不修改 `CANopenNode/` core。

## 2. 多轴 logical-device 与 OD base

`logicalDevice` 是零基 logical-device 编号。当前公共 helper 使用 0x0800 的 profile-instance stride：

```text
odBase = 0x6000 + logicalDevice * 0x0800
object = odBase + (axis0Index - 0x6000)
```

| logicalDevice | OD base | Controlword | Statusword |
|---:|---:|---:|---:|
| 0 | `0x6000` | `0x6040` | `0x6041` |
| 1 | `0x6800` | `0x6840` | `0x6841` |
| 2 | `0x7000` | `0x7040` | `0x7041` |

配置数组不要求连续，例如 logical-device 0 和 2 可以同时使用；同一个 logical-device 被两个 axis 配置时初始化返回 `CO_402_INIT_DUPLICATE_AXIS`。

## 3. OD Binding 契约

XDD/生成后的 `OD.c`、`OD.h` 是对象存在性和语义数据类型的 source of truth。Device core 运行时不会创建隐藏对象，只验证 CANopenNode runtime 可以看到的结构和属性。

每个配置 axis 当前要求以下对象：

| Axis0 index | 含义 | 长度 | Device core 要求的 access/mapping |
|---:|---|---:|---|
| `0x603F` | Error code | 2 | SDO-R, MB |
| `0x6040` | Controlword | 2 | SDO-RW, RPDO, MB |
| `0x6041` | Statusword | 2 | SDO-R, TPDO, MB |
| `0x6060` | Modes of operation | 1 | SDO-RW, RPDO |
| `0x6061` | Modes of operation display | 1 | SDO-R, TPDO |
| `0x6064` | Position actual value | 4 | SDO-R, TPDO, MB |
| `0x606C` | Velocity actual value | 4 | SDO-R, TPDO, MB |
| `0x607A` | Target position | 4 | SDO-RW, RPDO, MB |
| `0x60FF` | Target velocity | 4 | SDO-RW, RPDO, MB |
| `0x6502` | Supported drive modes | 4 | SDO-R, MB |

初始化逐项检查：

1. 对象存在；
2. OD entry 是单 sub-index 的 scalar `VAR`；
3. byte length 与表中一致；
4. 全部 runtime-visible attribute 与 Device core contract 一致，包括拒绝未声明的 SRDO mapping 和 `ODA_STR`；
5. Controlword/Modes of operation 的 extension 没有被其他 subsystem 占用。

所有 axis 和对象全部通过后才安装 forwarding extension，因此 validation failure 不会留下“部分新 extension 已安装”的 manager。

> CANopenNode 当前 runtime OD metadata 不保存 XDD 中完整的语义 data-type 信息，例如有符号/无符号类型名称。Device core 因此只能在 runtime 验证 object code、sub-index 结构、byte width 和 access/mapping；INTEGER/UNSIGNED 等语义类型仍由 XDD/生成物负责。这不是 CiA 402 conformance 声明。

## 4. PDS supervisor 与 DriveIF

Device core 每次 `CO_402_device_process()` 对每个 axis 只运行一个状态 handler。DriveIF 返回 `BUSY` 后，普通请求仍由当前 callback 独占并在下一 supervisor 周期继续轮询；但 Quick-stop、Disable-voltage 可以在 callback 边界转移 ownership，fault reaction 的优先级更高。新的 safety callback 必须在返回前同步 supersede 旧 owner 留下的物理动作，不能让两个 DriveIF 动作并行。

当前 DriveIF 的状态动作分别是：

| Callback | PDS 目的 |
|---|---|
| `shutdown` | 完成到 Ready to switch on 的产品动作 |
| `switchOn` | 完成到 Switched on 的产品动作 |
| `enableOperation` | 进入 Operation enabled |
| `disableVoltage` | 移除 drive voltage 并进入 Switch on disabled |
| `disableOperation` | 从 Operation enabled 回到 Switched on |
| `quickStop` | 启动/继续 quick-stop action |
| `faultReaction` | 执行 fault reaction |
| `faultReset` | 执行允许的 fault reset |

返回值语义：

- `CO_402_DRIVE_BUSY`：普通请求保持当前 state/owner 并在下一 supervisor 周期继续轮询；更高优先级 safety 请求可在 callback 边界替换该 owner；
- `CO_402_DRIVE_DONE`：动作完成，提交 target PDS state；
- `CO_402_DRIVE_ERROR`：普通迁移动作进入 Fault reaction active；fault-reaction 本身结束或失败后进入 Fault。

Device core 不把 Controlword `0x000F` 当成从 Switch on disabled/Ready to switch on 自动跨越多个状态的快捷命令；host/controller 必须按状态逐步发命令。Quick stop active 当前也不会用 `0x000F` 直接恢复 Operation enabled，因为 Device core 尚未绑定 Quick stop option code 等对应 policy 对象；在规范矩阵和对象契约明确前不隐式发明恢复策略。

Fault Reset 使用独立的边沿事务语义：supervisor 在每次成功读取 Controlword 后都记录 bit 7，只有处于 Fault 时观察到新的 `0 -> 1` 才启动一次 `faultReset`。回调返回 `BUSY` 后事务会锁存并在后续周期继续推进，直到 `DONE` 或 `ERROR`；普通 Controlword state command 不会抢占已接受的 Fault Reset，但 Controlword 读取失败会立即把 ownership 转移到 fault reaction。bit 7 持续保持为 1 不会在后续再次进入 Fault 时自动触发 reset。普通 PDS command 解码会忽略 bit 7，因此 bit 7 的旧高电平也不会屏蔽 Shutdown/Enable operation 等正常状态命令。

## 5. 生命周期与无 heap 约束

应用必须保证以下对象在 manager 使用期间持续有效：

```c
CO_402_device_manager_t manager;
CO_402_device_axis_t axes[AXIS_COUNT];
CO_402_device_axis_config_t configs[AXIS_COUNT];
```

`configs[*].drive` 和 `configs[*].driveObject` 同样必须保持有效。Device core 不调用 `malloc/calloc/realloc/free`，也不持有 RT-Thread object。

## 6. 可选日志挂载

Pure-C CiA 402 代码只调用 `CO_402_LOG_E/W/I/D` 宏，默认定义为空操作，不依赖 `printf`、RT-Thread、ULOG 或 BSP。日志点集中在实际 PDS state 变化、DriveIF 错误、Controlword 读取失败以及 Fault Reset edge/完成/失败，不在每个 supervisor 周期持续打印。

如需外部日志后端，在编译 CiA 402 源码时定义 `CO_402_LOG_CUSTOM_HEADER` 为自定义头文件。仓库提供可选的 RT-Thread 适配头 `profile/cia402/port/rtthread/CO_402_log_RTT.h`，其内部把上述宏映射到现有 `CO_RTT_LOG_*`，再由 `co_rtt_log.h` 按 `PKG_CANOPENNODE_USING_DEBUG && RT_USING_ULOG` 接到 ULOG。例如：

```text
-DCO_402_LOG_CUSTOM_HEADER=\"CO_402_log_RTT.h\"
```

使用该适配头时，需要让编译器能搜索 `profile/cia402/port/rtthread`。未定义 `CO_402_LOG_CUSTOM_HEADER` 时，Pure-C 构建行为和依赖保持不变。
