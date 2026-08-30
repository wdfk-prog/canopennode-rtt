[English](../en/high-resolution-time.md)

# High-Resolution Time 集成说明

本文说明 CANopenNode RT-Thread wrapper 的微秒时间源、High-Res Timer 约束以及当前实现的边界。

## 1. 作用范围

High-Resolution Time 只负责测量 CANopenNode processing 两次调用之间实际经过的微秒数。
它不会替换 realtime worker 的周期唤醒机制。

```text
RT-Thread software timer `co_tmr`
        -> rtSem
        -> co_rt thread

CO_time_RTT
        -> tick fallback 或 hardware counter
        -> actual elapsed_us
        -> CO_process*()
```

因此：

- `PKG_CANOPENNODE_TIMER_PERIOD_US` 决定 realtime worker 的请求唤醒周期；
- High-Resolution Time 决定每次 processing 实际使用的 `timeDifference_us`；
- realtime 唤醒仍保持 `rt_timer -> rtSem -> co_rt`。

## 2. Kconfig

| 选项 | 默认值 | 说明 |
|---|---:|---|
| `PKG_CANOPENNODE_USING_HIGH_RES_TIME` | `n` | 启用专用硬件 Timer 微秒时间源。关闭时使用 RT-Thread tick fallback。 |
| `PKG_CANOPENNODE_HIGH_RES_TIMER_NAME` | `"timer0"` | High-Res 模式使用的 RT-Thread clock timer 设备名。 |

High-Res 模式不提供 Timer 频率、位宽或方向的额外配置项。它们是固定集成约束。

## 3. High-Res 关闭时

`PKG_CANOPENNODE_USING_HIGH_RES_TIME=n` 时：

- 时间源使用 `rt_tick_get_millisecond()`；
- `CO_RTT_timeInit()` 不占用独占硬件资源；
- `CO_RTT_timeDeinit()` 为 no-op；
- 保持原有多个 `CANopenNodeRTT` 实例能力；
- 时间分辨率仍受 RT-Thread tick rate 限制，不提供亚毫秒精度。

## 4. High-Res 开启时

`PKG_CANOPENNODE_USING_HIGH_RES_TIME=y` 时，选择的 Timer 必须满足：

```text
dedicated RT-Thread clock timer
1 MHz
physical 32-bit counter
up-counting
start / stop / count_get available
```

一个 1 MHz counter 满足：

```text
1 count = 1 us
```

wrapper 直接读取 `count_get()` 作为 `uint32_t` 微秒时间戳。RT-Thread 使用 `UINT32_MAX` 作为
clock timer 的 count duration，因此在 1 MHz 下 High-Res 硬件周期为 `UINT32_MAX us`（约 `71.58 min`），
比 modulo-`2^32` 时间戳周期少 `1 us`。`CO_RTT_timeElapsedUs()` 会在 High-Res 回绕时补偿这一计数差异；
wrapper 不维护 64-bit 时间、软件 overflow extension 或浮点时间换算。

### 单实例限制

High-Res backend 拥有一个 package-wide dedicated hardware Timer，因此当前实现明确只支持一个
`CANopenNodeRTT` 实例。

- 第一个实例初始化成功后占有 High-Res Timer；
- 第二个实例串行再次调用 `CO_RTT_timeInit()` 返回 `-RT_EBUSY`；
- `CO_RTT_timeInit()` / `CO_RTT_timeDeinit()` 属于启动/关闭生命周期 API，调用方必须串行调用；
- 不支持多个线程并发执行 High-Res init/deinit；`initialized` 只是 ownership guard，不是线程同步原语；
- 当前实现不使用 refcount 共享同一个 High-Res Timer；
- 如果产品需要 High-Res 多实例，需要重新设计 Timer ownership，不能直接忽略 `-RT_EBUSY`。

该限制只适用于 `PKG_CANOPENNODE_USING_HIGH_RES_TIME=y`。Tick fallback 不受此限制。

## 5. 32-bit Timer 宽度无法由当前通用 API 可靠识别

物理 Timer 必须为 32-bit，但当前 wrapper **不会在运行时识别或拒绝 16-bit Timer**。

原因是 RT-Thread 通用 clock-timer API 提供的 `rt_clock_timer_info.maxcnt` 在部分 BSP 中不是每个
具体 Timer 的真实物理位宽描述。以当前 STM32 F4 通用 timer 配置为例，不同 TIM 设备可能共享同一份
`TIM_DEV_INFO_CONFIG`，其中 `maxcnt` 为 `0xFFFF`；因此仅检查 `maxcnt` 无法可靠区分实际 16-bit 和
32-bit Timer。

本实现因此不做以下检查：

```c
timer->info->maxcnt == UINT32_MAX
```

也不会：

- 根据 `timer2` / `timer5` 设备名建立 STM32 白名单；
- 访问 STM32 HAL 或外设寄存器来猜测位宽；
- 因 `maxcnt == 0xFFFF` 就拒绝一个实际可能为 32-bit 的 Timer。

### 用户/BSP 的责任

启用 High-Res 前，必须根据目标 MCU reference manual、BSP Timer 映射和产品资源占用表确认：

1. `PKG_CANOPENNODE_HIGH_RES_TIMER_NAME` 对应哪个物理 TIM；
2. 该物理 TIM 的 counter 确实是 32-bit；
3. Timer 未被 PWM、input capture、控制环或其他模块占用。

对于 STM32F407，可优先核对 TIM2/TIM5 等 32-bit general-purpose Timer，但实际设备名和占用情况必须
以目标 BSP 为准。

如果误选 16-bit Timer，只要它仍能通过设备类型、1 MHz、UP mode 和 required ops 检查，初始化可能
成功，wrapper 不会因为位宽错误主动报错。1 MHz 的 16-bit counter 约每 `65.536 ms` 回绕一次，
此时 elapsed-time 将不满足本设计要求。

## 6. wrapper 实际检查什么

High-Res 初始化仍会检查：

- `rt_device_find()` 能找到设备；
- 设备类型为 `RT_Device_Class_Timer`；
- `start` / `stop` / `count_get` 等 required ops 存在；
- RT-Thread clock timer 实际频率为 `1 MHz`；
- `cntmode == CLOCK_TIMER_CNTMODE_UP`；
- Timer 能正常 open/start。

不会检查：

- 物理 counter 是否真正为 32-bit；
- Timer 是否被另一个业务以未被 RT-Thread open ownership 检出的方式复用。

因此 High-Res Timer 选择属于 BSP 集成配置，不是完全可自校验的运行时配置。

## 7. 32-bit 时间戳与回绕

时间接口为：

```c
uint32_t CO_RTT_timeNowUs(void);
```

elapsed 必须通过 wrapper helper 计算：

```c
uint32_t elapsed_us = CO_RTT_timeElapsedUs(now_us, previous_us);
```

Tick backend 下，该 helper 使用常规 `uint32_t` 无符号减法。High-Res 启用时，RT-Thread clock timer
以 `UINT32_MAX` 个 count 运行，因此硬件 counter 在 `UINT32_MAX us` 后回绕，而不是在 `2^32 us` 后
回绕。当 `now_us < previous_us` 时，helper 按该硬件周期计算，从而避免直接无符号减法在 High-Res
跨回绕时多计算 `1 us`。

两次 processing 之间的真实间隔必须小于一个 backend 的回绕周期。需要同时兼容 Tick 与 High-Res 时，
不要用裸的 `now_us - previous_us` 替代 `CO_RTT_timeElapsedUs()`。

## 8. Communication Reset

`CO_RESET_COMM` 不重新打开或关闭 High-Res Timer。时间源在整个 CANopenRTT 实例生命周期内保持运行；
communication reset 只重新建立 mainline 和 realtime 的 elapsed baseline，避免把 stack 重建时间错误计入
新 communication stack 的首轮 processing。

## 9. 集成检查清单

启用 High-Res 前确认：

```text
[ ] 只创建一个 CANopenNodeRTT 实例
[ ] Timer device name 与目标 BSP 注册名称一致
[ ] 物理 Timer 确认为 32-bit
[ ] Timer 能运行在 1 MHz
[ ] Timer 为 up-counting
[ ] Timer 为 CANopen High-Res 独占资源
[ ] start/stop/count_get 可用
[ ] 目标板观察到非 1000 us 整数倍的 elapsed
[ ] 完成长时间 wrap/稳定性验证
```

未完成目标板验证前，不应仅根据初始化成功判断 Timer 选择正确，因为错误的 16-bit Timer 不保证被代码识别。
