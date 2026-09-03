[中文](../zh/cia402-device-rtt.md)

# CiA 402 RT-Thread Device Thread and Communication Reset

Stage A4 integrates the A3 Pure-C `CO_402_device_manager_t` with `CANopenNodeRTT`. It does not change PDS FSA,
DriveIF, or generated-OD semantics. It owns the RT-Thread thread, generic lifecycle-extension integration, lock order, communication-init placement, and
Communication Reset lifecycle.

![CiA 402 RT-Thread thread and reset](../assets/cia402-device-rtt.svg)

## 1. Enable and attach

Relevant Kconfig options:

| Option | Default | Purpose |
|---|---:|---|
| `PKG_CANOPENNODE_CIA402` | `n` | Master CiA 402 switch. A4 adds no `CANopenNodeRTT` fields or RT resources when disabled. |
| `PKG_CANOPENNODE_CIA402_DEVICE` | `y` | A3 Pure-C Device core. |
| `PKG_CANOPENNODE_CIA402_DEVICE_RTT_THREAD` | `y` | Builds the A4 adapter and selects the generic lifecycle registry; only an attached instance creates thread/semaphore resources. |
| `PKG_CANOPENNODE_CIA402_DEVICE_RTT_AUTOSTART` | `n` | With default app auto init and RT-Thread component init, lets a registered CiA 402 factory allocate runtime/axis state and attach it automatically. |
| `PKG_CANOPENNODE_CIA402_DEVICE_RTT_DEMO` | `n` | Registers the package software-only factory and selects the generated demo OD for protocol/PDS/lifecycle validation. |
| `PKG_CANOPENNODE_CIA402_DEMO_AXIS_COUNT` | `3` | Number of software-only demo logical devices; intentionally limited to the generated OD range 1..3. |
| `PKG_CANOPENNODE_CIA402_THREAD_STACK_SIZE` | `2048` | `co_402` stack size. |
| `PKG_CANOPENNODE_CIA402_THREAD_PRIORITY` | `5` | `co_402` priority; it must remain below `co_rt`, so its numeric value must be larger. |

A Device product uses manual initialization and attaches persistent storage before `canopen_app_rtt_init()`:

```c
static CANopenNodeRTT app;
static CO_402_device_axis_t axes[3];
static CO_402_device_RTT_t cia402;
static const CO_402_device_axis_config_t axisConfigs[3] = {
    { .logicalDevice = 0U, .drive = &driveIf0, .driveObject = &motor0 },
    { .logicalDevice = 1U, .drive = &driveIf1, .driveObject = &motor1 },
    { .logicalDevice = 2U, .drive = &driveIf2, .driveObject = &motor2 },
};

static const CO_402_device_RTT_config_t cia402Config = {
    .axes = axes,
    .configs = axisConfigs,
    .axisCount = 3U,
};

CO_402_device_RTT_attach(&app, &cia402, &cia402Config);
canopen_app_rtt_init(&app, "can1", 1U, 1000U);
```

`attach()` initializes a caller-owned `CO_402_device_RTT_t`, stores axis/config pointers, and registers a static ops/context pair with the fixed-capacity `CO_RTT_lifecycle` registry. `CO_app_RTT.h` no longer includes a CiA 402 header or embeds CiA 402 runtime state. Attach creates no thread/semaphore, accesses no CAN hardware, and does not bind OD entries.
`app` and the `cia402` runtime must be zero-initialized before first use. Runtime, axis/config, DriveIF, and `driveObject` storage must outlive the instance. Attach explicitly receives the runtime as `CO_402_device_RTT_attach(app, runtime, config)`, keeping the generic app/lifecycle layer unaware of profile types. Products using this manual path should disable default app auto init for the same logical node.

### Optional automatic construction

`PKG_CANOPENNODE_CIA402_DEVICE_RTT_AUTOSTART=y` changes only object construction/ownership. It requires the default application, RT-Thread component initialization and heap support. Product code still owns the persistent axis configuration, DriveIF tables, and drive objects, but it no longer provides `CO_402_device_RTT_t` or `CO_402_device_axis_t[]` storage.

For package-level bring-up, enable `PKG_CANOPENNODE_CIA402_DEVICE_RTT_DEMO`. The demo selects autostart and the generated demo OD, provides immediate-DONE software DriveIF callbacks for logical devices 0..N-1, and component-registers the factory automatically. No application call to `CO_402_DEVICE_RTT_AUTOSTART_DEFINE()` is needed in that mode. The demo never controls a physical motor or power stage.

For a real product, keep the package demo disabled and declare exactly one product-owned factory:

```c
static const CO_402_device_axis_config_t axisConfigs[] = {
    { .logicalDevice = 0U, .drive = &driveIf0, .driveObject = &motor0 },
    { .logicalDevice = 1U, .drive = &driveIf1, .driveObject = &motor1 },
};

CO_402_DEVICE_RTT_AUTOSTART_DEFINE(product402, axisConfigs, RT_ARRAY_SIZE(axisConfigs));
```

The macro registers a static factory during RT-Thread component initialization. Later the default CANopen app calls generic `CO_RTT_lifecycleAutoAttachAll()` before `canopen_app_rtt_init()`. The CiA 402 factory allocates only one adapter owner plus the axis runtime array, then reuses the same attach implementation and lifecycle ops as manual mode. If autostart is enabled without either the package demo or a product factory, the generic registry remains empty and startup fails closed before the CANopen app starts. Duplicate adapters, allocation failure, or any factory failure are handled the same way.

The automatic allocation survives Communication Reset. Final application-init rollback/final lifecycle teardown invokes the slot release callback after `runtimeDeinit()`, frees the automatic axis/runtime owner, and removes only auto-owned slots. Manual slots remain caller-owned and registered.

## 2. Generic lifecycle registry and periodic thread

`CO_app_RTT.c` no longer calls any `CO_402_device_RTT_*()` API directly. Manual extensions use `CO_RTT_lifecycleRegister()`; auto factories use `CO_RTT_lifecycleRegisterEx()` with a final context-release callback. `PKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSION_CAPACITY` configures both the per-app extension registry and global auto-factory registry capacity (default 4, range 1..255). The registries themselves use no heap; only profile-specific auto factories may allocate their owned runtime contexts. Runtime init/start/reset remains a single generic dispatcher path for both manual and automatic attachments.

A4 still reuses the existing `rtTimer`:

```text
rtTimer -> rtSem     -> co_rt  (default priority 3)
        -> cia402Sem -> co_402 (default priority 5)
```

The timer releases `rtSem` first, then calls generic `CO_RTT_lifecycleRealtimeTick()`; the CiA 402 hook releases `cia402Sem`. This path is independent of `PKG_CANOPENNODE_GLOBAL_TIMERNEXT`, so `co_402` is periodically woken in both mainline modes. Each `co_402` token runs one `CO_402_device_process()` pass under:

```text
lifecycleMutex -> CO_LOCK_OD -> Pure-C PDS supervisor -> CO_UNLOCK_OD -> lifecycleMutex release
```

This matches the `co_rt` lock order and avoids inversion. DriveIF callbacks execute inside this critical section, so they
must remain non-blocking, must not sleep, and must not recursively acquire the wrapper lifecycle or OD lock. The default
priority keeps `co_rt` above `co_402`; final priority, WCET, and jitter require target measurement.

A4 allows the planned one-cycle pipeline: an RPDO command received in one realtime cycle can be processed by `co_402`,
and the resulting Statusword/feedback can be emitted by a later TPDO cycle.

## 3. OD binding order

For registered extensions, `CO_app_RTT.c` keeps only these generic lifecycle positions:

```text
CO_CANinit
-> CO_CANopenInit
-> CO_RTT_lifecycleBindCommunication
     -> CiA 402 communicationBind hook
-> CO_CANopenInitSRDO (when enabled)
-> CO_CANopenInitPDO
-> demo/mainline callback bind
-> CO_CANsetNormalMode
-> CO_RTT_lifecycleCommunicationReady
```

Controlword and Modes-of-operation extensions therefore exist before PDO initialization. With no registered extension, the generic no-op/empty-registry path requires no profile-specific branch and keeps existing demo behavior. If an attached OD does not satisfy the A3 contract, initialization fails
deterministically and logs the logical-device/index/sub-index diagnostic.

## 4. Communication Reset

On `CO_RESET_COMM`, the mainline stops the shared realtime timer, drains `rtSem` plus registered extension wake state, then takes
`lifecycleMutex`. Inside the lifecycle boundary it:

1. calls `CO_RTT_lifecycleCommunicationStop()`; the CiA 402 hook sets `communicationReady=false`;
2. disables the old CAN module and waits for its RX thread to exit, so no SDO callback can still use the old OD extensions;
3. calls `CO_RTT_lifecycleCommunicationQuiesced()` in reverse registration order; the CiA 402 hook removes only its owned OD extensions;
4. deletes the old `CO_t`, creates a new `CO_t`, and initializes CANopen communication;
5. calls `CO_RTT_lifecycleBindCommunication()` after `CO_CANopenInit()`; the CiA 402 hook reuses `CO_402_device_bindOD()`;
6. initializes SRDO/PDO, enters CAN normal mode, then calls `CO_RTT_lifecycleCommunicationReady()`;
7. releases the lifecycle mutex, refreshes the realtime baseline, and restarts the timer.

The thread resolves `app->canOpenStack` only after taking `lifecycleMutex`. It does not retain `CO_t`, `CANmodule`, or
`odMutex` pointers across reset. A thread that already consumed an old semaphore token either completes before reset owns
the lifecycle mutex or waits and resolves the new generation afterwards.

CiA 301 v4.2.0 defines Reset Communication as a reset of the communication part. A4 therefore reuses
`CO_402_device_bindOD()` instead of calling `CO_402_device_managerInit()` again, and does not invent a DriveIF power-off or
PDS-state reset policy. Product NMT/PDS power policy remains a later normative-matrix decision.

## 5. Disable and rollback

With `PKG_CANOPENNODE_CIA402=n`, `PKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSIONS` is not selected, so `CANopenNodeRTT` gains no lifecycle registry state; the generic app structure never embeds CiA 402 profile state, `CO_lifecycle_RTT.c`/the CiA 402 adapter are not selected by SCons, and no thread/semaphore/timer hook is added. The original realtime sequence and timerNext mainline branch remain unchanged. Disabling only `PKG_CANOPENNODE_CIA402_DEVICE_RTT_THREAD` retains the A3 Pure-C core without A4 RT resources. Keeping `PKG_CANOPENNODE_CIA402_DEVICE_RTT_AUTOSTART=n` preserves the manual zero-CiA402-heap path.

## 6. Validation boundary

The A4 target matrix covers CIA402 off/on, timerNext off/on, `CO_MULTIPLE_OD`, incompatible demo OD, and the CiA 402
multi-axis OD. This page states the implementation contract only. Build logs, target boot, three-axis SDO, repeated Reset
Communication, CAN traces, and WCET/jitter require actual execution evidence; Host/static checks do not prove MCU/HIL behavior.
