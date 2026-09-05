[中文](../zh/high-resolution-time.md)

# High-Resolution Time integration

This page describes the CANopenNode RT-Thread wrapper microsecond time source, the High-Res Timer contract, and the current implementation limits.

## 1. Scope

High-Resolution Time measures the actual elapsed microseconds between CANopenNode processing calls. It does not replace the periodic realtime-worker wakeup mechanism.

```text
RT-Thread software timer `co_tmr`
        -> rtSem
        -> co_rt thread

CO_time_RTT
        -> tick fallback or hardware counter
        -> actual elapsed_us
        -> CO_process*()
```

Therefore:

- `PKG_CANOPENNODE_TIMER_PERIOD_US` controls the requested realtime-worker wake period;
- High-Resolution Time provides the actual `timeDifference_us` used by processing calls;
- realtime wakeup remains `rt_timer -> rtSem -> co_rt`.

## 2. Kconfig

| Option | Default | Meaning |
|---|---:|---|
| `PKG_CANOPENNODE_USING_HIGH_RES_TIME` | `n` | Enable the dedicated hardware-timer microsecond source. The disabled path uses the RT-Thread tick fallback. |
| `PKG_CANOPENNODE_HIGH_RES_TIMER_NAME` | `"timer0"` | RT-Thread clock-timer device name used by High-Res mode. |

High-Res mode does not expose separate frequency, width, or direction settings. Those are fixed integration requirements.

## 3. High-Res disabled

With `PKG_CANOPENNODE_USING_HIGH_RES_TIME=n`:

- timestamps come from `rt_tick_get_millisecond()`;
- `CO_RTT_timeInit()` owns no exclusive hardware resource;
- `CO_RTT_timeDeinit()` is a no-op;
- the existing multi-`CANopenNodeRTT` capability remains available;
- resolution is still limited by the RT-Thread tick rate and is not sub-millisecond.

## 4. High-Res enabled

With `PKG_CANOPENNODE_USING_HIGH_RES_TIME=y`, the selected timer must be:

```text
dedicated RT-Thread clock timer
1 MHz
physical 32-bit counter
up-counting
start / stop / count_get available
```

At 1 MHz:

```text
1 count = 1 us
```

The wrapper reads `count_get()` directly as a `uint32_t` microsecond timestamp. RT-Thread starts the clock timer with a `UINT32_MAX` count duration, so at 1 MHz the High-Res hardware period is `UINT32_MAX us` (about `71.58 min`), one microsecond shorter than a modulo-`2^32` timestamp period. `CO_RTT_timeElapsedUs()` compensates that one-count difference across a High-Res wrap. The wrapper does not maintain a 64-bit timebase, software overflow extension, or floating-point time conversion.

### Single-instance restriction

The High-Res backend owns one package-wide dedicated hardware timer, so the current implementation intentionally supports only one `CANopenNodeRTT` instance while High-Res is enabled.

- The first instance owns the High-Res Timer after successful initialization.
- A second serial call to `CO_RTT_timeInit()` returns `-RT_EBUSY`.
- `CO_RTT_timeInit()` / `CO_RTT_timeDeinit()` are startup/shutdown lifecycle APIs and must be called serially.
- Concurrent High-Res init/deinit is not supported; `initialized` is an ownership guard, not a thread-synchronization primitive.
- The current implementation does not use a reference count to share one High-Res Timer.
- A product that requires multiple High-Res instances needs a new timer-ownership design; it must not simply ignore `-RT_EBUSY`.

This restriction applies only when `PKG_CANOPENNODE_USING_HIGH_RES_TIME=y`. The tick fallback is not restricted this way.

## 5. The current generic API cannot reliably identify a 32-bit timer

The physical timer must be 32-bit, but the wrapper **does not detect or reject a 16-bit timer by width at runtime**.

The reason is that `rt_clock_timer_info.maxcnt` is not a reliable per-device physical-width description on every RT-Thread BSP. In the current generic STM32 F4 timer configuration, multiple TIM devices can share one `TIM_DEV_INFO_CONFIG` whose `maxcnt` is `0xFFFF`; that metadata therefore cannot reliably distinguish a physical 16-bit timer from a physical 32-bit timer.

The implementation intentionally does not require:

```c
timer->info->maxcnt == UINT32_MAX
```

It also does not:

- whitelist STM32 device names such as `timer2` or `timer5`;
- inspect STM32 HAL objects or peripheral registers to infer timer width;
- reject a potentially valid 32-bit timer merely because shared BSP metadata reports `0xFFFF`.

### BSP/user responsibility

Before enabling High-Res, verify from the target MCU reference manual, BSP timer mapping, and product resource map that:

1. `PKG_CANOPENNODE_HIGH_RES_TIMER_NAME` maps to the intended physical TIM;
2. that physical TIM really has a 32-bit counter;
3. the timer is not used by PWM, input capture, a control loop, or another component.

On STM32F407, TIM2/TIM5 are typical 32-bit candidates, but the actual RT-Thread device name and resource ownership must be checked in the target BSP.

If a 16-bit timer is selected and it still passes the device-type, 1 MHz, up-counting, and required-operation checks, initialization may succeed. The wrapper will not raise a counter-width error. A 1 MHz 16-bit counter wraps about every `65.536 ms`, so elapsed-time behavior will not satisfy this design.

## 6. What the wrapper validates

High-Res initialization still validates:

- the device can be found with `rt_device_find()`;
- the device type is `RT_Device_Class_Timer`;
- required `start`, `stop`, and `count_get` operations are present;
- the clock timer runs at `1 MHz`;
- `cntmode == CLOCK_TIMER_CNTMODE_UP`;
- the device can be opened and started.

It does not validate:

- that the physical counter is really 32-bit;
- exclusive product-level ownership that is not represented by RT-Thread device-open semantics.

Timer selection is therefore a BSP integration contract, not a fully self-validating runtime setting.

## 7. 32-bit timestamp wrapping

The interface is:

```c
uint32_t CO_RTT_timeNowUs(void);
```

Elapsed time must use the wrapper helper:

```c
uint32_t elapsed_us = CO_RTT_timeElapsedUs(now_us, previous_us);
```

With the tick backend, the helper uses normal `uint32_t` unsigned subtraction. With High-Res active, the RT-Thread clock timer is started for `UINT32_MAX` counts, so the hardware counter wraps after `UINT32_MAX us`, not `2^32 us`. When `now_us < previous_us`, the helper uses that hardware period and avoids the `+1 us` error that raw unsigned subtraction would introduce across the High-Res wrap.

The real interval between two processing calls must remain shorter than one backend wrap period. Do not replace `CO_RTT_timeElapsedUs()` with a raw `now_us - previous_us` calculation in code that must work with both backends.

## 8. Communication reset

`CO_RESET_COMM` does not reopen or close the High-Res Timer. The time source remains active for the complete CANopenRTT instance lifetime. Communication reset only re-establishes the mainline and realtime elapsed baselines so stack-recreation time is not charged to the first processing call on the new communication stack.

## 9. Integration checklist

Before enabling High-Res, verify:

```text
[ ] only one CANopenNodeRTT instance is created
[ ] timer device name matches the target BSP registration
[ ] the physical timer is confirmed to be 32-bit
[ ] the timer can run at 1 MHz
[ ] the timer counts upward
[ ] the timer is dedicated to CANopen High-Res timing
[ ] start/stop/count_get are available
```

Successful initialization does not establish the physical counter width; the selected timer must still be confirmed from the MCU and BSP resource definition.
