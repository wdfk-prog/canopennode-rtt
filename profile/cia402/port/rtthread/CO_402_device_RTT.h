/**
 * @file CO_402_device_RTT.h
 * @brief RT-Thread lifecycle adapter for the Pure-C CiA 402 Device manager.
 */

#ifndef CO_402_DEVICE_RTT_H
#define CO_402_DEVICE_RTT_H

#include <rtthread.h>

#include "CO_402_device.h"

#if defined(PKG_CANOPENNODE_CIA402_DEVICE_RTT_AUTOSTART)
#include "CO_lifecycle_RTT.h"
#endif /* defined(PKG_CANOPENNODE_CIA402_DEVICE_RTT_AUTOSTART) */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct CANopenNodeRTT;
typedef struct CANopenNodeRTT CANopenNodeRTT;

/** Persistent caller-owned axis storage attached before CANopen application initialization. */
typedef struct {
    CO_402_device_axis_t *axes;                    /**< Axis runtime array with @ref axisCount elements. */
    const CO_402_device_axis_config_t *configs;   /**< Immutable axis configuration array. */
    uint8_t axisCount;                             /**< Number of configured local logical devices. */
} CO_402_device_RTT_config_t;

/** Product-owned persistent configuration used by the optional heap-backed auto factory. */
typedef struct {
    const CO_402_device_axis_config_t *configs; /**< Persistent axis/DriveIF configuration array. */
    uint8_t axisCount;                         /**< Number of local logical devices to construct. */
} CO_402_device_RTT_autostart_config_t;

/** Caller-owned RT-Thread adapter state registered as lifecycle context. */
typedef struct {
    CO_402_device_manager_t manager;       /**< Pure-C Device manager bound to the generated OD. */
    CO_402_device_RTT_config_t config;     /**< Persistent pointer configuration copied by attach. */
    rt_thread_t workerThread;              /**< Lower-priority non-blocking PDS supervisor thread. */
    struct rt_semaphore cia402Sem;         /**< Wake semaphore released by the shared realtime timer. */
    rt_bool_t attached;                    /**< True after successful lifecycle registration. */
    rt_bool_t managerInitialized;          /**< True after the initial manager/OD binding succeeds. */
    rt_bool_t semInitialized;              /**< True while cia402Sem is an initialized RT-Thread object. */
    rt_bool_t communicationReady;          /**< Gate for processing the current CANopen stack generation. */
    CANopenNodeRTT *app;                    /**< Attached application used by the co_402 thread; caller-owned. */
} CO_402_device_RTT_t;

/**
 * @brief Attach a local CiA 402 Device runtime before CANopen application initialization.
 *
 * This function initializes the caller-owned @p runtime, stores persistent axis/config
 * pointers, and registers that runtime with the generic CANopenNodeRTT lifecycle registry.
 * It does not create RT-Thread objects, start a thread, touch hardware, or bind the Object
 * Dictionary. @p runtime, @p axes, @p configs, and every DriveIF object must remain valid
 * for the CANopenNodeRTT instance lifetime.
 *
 * The co_402 thread executes DriveIF callbacks while holding lifecycleMutex and
 * the CANopenNode OD lock. DriveIF callbacks must therefore remain non-blocking
 * and must not recursively acquire either wrapper lifecycle or OD lock.
 *
 * @param app Zero-initialized CANopenNode RT-Thread application instance.
 * @param runtime Zero-initialized caller-owned CiA 402 RT-Thread runtime storage.
 * @param config Persistent local Device axis configuration.
 * @return RT_EOK on success, -RT_EINVAL for invalid arguments, -RT_EBUSY if
 *         the application has already been attached or initialized, or
 *         the lifecycle registry error when registration cannot be completed.
 */
rt_err_t CO_402_device_RTT_attach(CANopenNodeRTT *app, CO_402_device_RTT_t *runtime,
                                   const CO_402_device_RTT_config_t *config);

#if defined(PKG_CANOPENNODE_CIA402_DEVICE_RTT_AUTOSTART)
/** Fixed lifecycle factory order reserved for the single automatic local CiA 402 Device runtime. */
#define CO_402_DEVICE_RTT_AUTOSTART_FACTORY_ORDER 402U

/**
 * @brief Allocate and attach one lifecycle-owned CiA 402 Device runtime.
 *
 * Only the adapter runtime and CO_402_device_axis_t array are heap-owned. @p config,
 * every DriveIF table, and every driveObject remain product-owned and must outlive
 * the default CANopenNodeRTT instance. On failure no lifecycle slot or heap object
 * remains. Communication Reset reuses the same allocation; final lifecycle teardown
 * releases it after runtimeDeinit.
 *
 * @param app Zero-initialized CANopenNode RT-Thread application instance.
 * @param config Persistent product-owned automatic axis configuration.
 * @return RT_EOK on success, -RT_EINVAL for invalid configuration, -RT_EBUSY when
 *         this app already has a CiA 402 Device lifecycle adapter, -RT_ENOMEM for
 *         allocation failure, or the lifecycle registration error.
 */
rt_err_t CO_402_device_RTT_autoAttach(CANopenNodeRTT *app, const CO_402_device_RTT_autostart_config_t *config);

#if defined(PKG_CANOPENNODE_CIA402_DEVICE_RTT_MSH)
/**
 * @brief Bind the optional MSH frontend after the local Device worker has started.
 *
 * The frontend stores the pointers only; every command later acquires the
 * application lifecycle mutex and OD lock before touching the manager.
 *
 * @param app Running default CANopenNode RT-Thread application instance.
 * @param runtime Started local Device runtime owned by @p app.
 */
void CO_402_device_RTT_mshBind(CANopenNodeRTT *app, CO_402_device_RTT_t *runtime);

/**
 * @brief Remove the optional MSH binding before Device runtime teardown.
 *
 * Lifecycle teardown calls this while the application lifecycle mutex excludes
 * command execution, so clearing the binding happens before runtime storage or
 * its semaphore can be released.
 *
 * @param app Application instance previously supplied to the MSH frontend.
 * @param runtime Device runtime previously supplied to the MSH frontend.
 */
void CO_402_device_RTT_mshUnbind(CANopenNodeRTT *app, CO_402_device_RTT_t *runtime);
#endif /* defined(PKG_CANOPENNODE_CIA402_DEVICE_RTT_MSH) */

/**
 * @brief Define and component-register the automatic CiA 402 factory for the default app.
 *
 * Exactly one definition is expected when PKG_CANOPENNODE_CIA402_DEVICE_RTT_AUTOSTART
 * is enabled. @p axisCount_ must be a compile-time constant in the supported
 * logical-device range; the macro rejects an invalid count before the uint8_t
 * configuration field is initialized. The axis/config/DriveIF objects referenced
 * by @p configs_ remain product-owned; only runtime axis state is dynamically allocated.
 */
#define CO_402_DEVICE_RTT_AUTOSTART_DEFINE(name_, configs_, axisCount_)                                      \
    typedef char name_##_co402_axis_count_must_be_valid[                                                     \
        ((axisCount_) > 0U && (axisCount_) <= CO_402_LOGICAL_DEVICE_COUNT_MAX) ? 1 : -1];                    \
    static const CO_402_device_RTT_autostart_config_t name_##_co402_autostart_config = {                    \
        .configs = (configs_),                                                                               \
        .axisCount = (uint8_t)(axisCount_),                                                                  \
    };                                                                                                       \
    /** Attach the generated CiA 402 runtime for this factory. */                                             \
    static rt_err_t name_##_co402_auto_factory_attach(CANopenNodeRTT *app_, const void *context_)           \
    {                                                                                                        \
        return CO_402_device_RTT_autoAttach(                                                                 \
            app_, (const CO_402_device_RTT_autostart_config_t *)context_);                                   \
    }                                                                                                        \
    static const CO_RTT_lifecycle_factory_t name_##_co402_auto_factory = {                                  \
        .name = #name_,                                                                                      \
        .order = CO_402_DEVICE_RTT_AUTOSTART_FACTORY_ORDER,                                                  \
        .attach = name_##_co402_auto_factory_attach,                                                         \
        .context = &name_##_co402_autostart_config,                                                          \
    };                                                                                                       \
    /** Register the generated factory during RT-Thread component initialization. */                         \
    static int name_##_co402_auto_factory_register(void)                                                     \
    {                                                                                                        \
        return (int)CO_RTT_lifecycleFactoryRegister(&name_##_co402_auto_factory);                            \
    }                                                                                                        \
    INIT_COMPONENT_EXPORT(name_##_co402_auto_factory_register)
#else
#define CO_402_DEVICE_RTT_AUTOSTART_DEFINE(name_, configs_, axisCount_)
#endif /* defined(PKG_CANOPENNODE_CIA402_DEVICE_RTT_AUTOSTART) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_DEVICE_RTT_H */
