/**
 * @file CO_401_device_RTT.h
 * @brief RT-Thread lifecycle adapter for the Pure-C CiA 401 Device core.
 */
#ifndef CO_401_DEVICE_RTT_H
#define CO_401_DEVICE_RTT_H

#include <rtthread.h>

#include "CO_401_device.h"
#include "CO_lifecycle_RTT.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct CANopenNodeRTT;
typedef struct CANopenNodeRTT CANopenNodeRTT;

/** Persistent product configuration copied into the lifecycle adapter. */
typedef struct {
    CO_401_device_config_t device; /**< IOIF and capability configuration; referenced product objects stay caller-owned. */
    /**
     * Optional product probe for the CiA 401 output-supervision prerequisite.
     * Return true only after the output-setting device's first Heartbeat or this
     * node's first NMT-master guard after power-on/application reset. While the
     * startup latch is false, the 0x6200/0x6411 OD write gate invokes this probe
     * synchronously so a qualifying event that happened before the write is visible
     * without waiting for the lower-priority profile worker.
     *
     * The probe is read-only and may be sampled by the worker and by an SDO/RPDO
     * write path concurrently; an output write may already own the OD lock. It must
     * be reentrant, bounded and non-blocking, must not acquire the OD/lifecycle
     * locks, and must not re-enter this adapter. The product must expose its monotonic
     * supervision fact with cross-context visibility (for example, an atomic latch).
     */
    bool (*outputSupervisionEstablished)(void *object, const CO_t *co);
    void *outputSupervisionObject; /**< Product-owned context shared by the readiness probe and fault classifier. */
    /**
     * Product classifier for an active CiA 401 output-supervision communication fault.
     * It is required for every attach path when fail-safe support is enabled for a configured
     * output capability; otherwise it is optional. Return true only for the Heartbeat timeout
     * of the device that sets the outputs or for the relevant node/life-guard failure. Do not
     * derive this result from the aggregate
     * CO_EM_HEARTBEAT_CONSUMER bit, because CANopenNode shares that bit across all monitored
     * Heartbeat consumers and node-guarding relationships. CAN bus-off is detected directly by
     * the adapter and does not need to be reported here.
     *
     * The worker invokes this callback only after output supervision has been established, while
     * the lifecycle generation is pinned and the OD lock is released between its supervision
     * snapshot and Device update phases. The callback must be bounded, non-blocking and must not
     * re-enter this adapter or acquire the OD lock.
     */
    bool (*outputSupervisionFaultActive)(void *object, const CO_t *co);
} CO_401_device_RTT_config_t;

/** Caller-owned RT-Thread adapter state registered as lifecycle context. */
typedef struct {
    CO_401_device_t device;             /**< Pure-C Device runtime bound to the current generated OD. */
    CO_401_device_RTT_config_t config;  /**< Persistent IO/capability configuration copied by attach. */
    CANopenNodeRTT *app;                /**< Attached application; caller-owned for the adapter lifetime. */
    rt_thread_t workerThread;           /**< Lower-priority bounded CiA 401 process worker. */
    struct rt_semaphore cia401Sem;      /**< Wake semaphore released by the shared realtime timer. */
    rt_atomic_t wakePending;            /**< Coalesced periodic wake: zero or one unconsumed worker request. */
    rt_bool_t attached;                 /**< True after successful lifecycle registration. */
    rt_bool_t deviceInitialized;        /**< True after the first Device/OD bind succeeds. */
    rt_bool_t semInitialized;           /**< True while @ref cia401Sem is initialized. */
    rt_bool_t communicationReady;       /**< Gate for the currently bound CANopen communication generation. */
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    CO_NMT_internalState_t nmtState;    /**< Latest mainline-published NMT state for output fail-safe processing. */
    rt_bool_t nmtStoppedApplyPending;   /**< Sticky Stopped edge until all required fail-safe backend writes succeed. */
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE)
    rt_bool_t analogWarningReported;    /**< True while the adapter owns the active 0x0080 warning error bit. */
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE */
} CO_401_device_RTT_t;

/**
 * @brief Attach one caller-owned CiA 401 Device runtime before application initialization.
 *
 * The function only registers lifecycle state. It does not create the worker or bind an OD;
 * those actions occur in lifecycle callbacks. @p runtime must be zero-initialized. The IOIF
 * table and @c ioObject referenced by @p config remain product-owned and must outlive the
 * CANopenNodeRTT instance.
 *
 * The co_401 worker follows lifecycleMutex -> CANopen OD lock ordering. IOIF callbacks execute
 * while both locks protect the current communication generation and must not recursively acquire
 * either lock. @c outputSupervisionFaultActive runs from the worker with the OD lock released.
 * @c outputSupervisionEstablished is a read-only startup-gate probe sampled eagerly by the
 * worker and synchronously by SDO/RPDO output writes; a write may already own the OD lock.
 * Neither callback may acquire adapter/OD locks.
 *
 * With analogue events enabled, SDO reads are identified by the active SDO server OD_IO
 * stream and commit 0x6401/0x6422 state when the OD read completes. Mapped TPDO state is
 * committed only after the RT-Thread CAN path accepts the exact frame. Local OD reads, PDO
 * serialization alone and failed/suppressed TPDO sends do not advance the communication state.
 * If outputs are enabled, network writes remain fail-closed until the product's
 * outputSupervisionEstablished probe reports the qualifying Heartbeat/node-guard
 * event during the write gate, or the application explicitly notifies the Pure-C
 * core under equivalent lifecycle/OD serialization. This makes an event processed
 * before an output write visible to that write without a worker scheduling delay.
 * The adapter cannot infer which remote node owns the
 * output-setting role from a generic RPDO COB-ID. When fail-safe support is enabled for any
 * configured output capability, @c outputSupervisionFaultActive is mandatory and is the
 * product-specific source for the corresponding Heartbeat/node-guard failure; unrelated
 * Heartbeat consumer timeouts are intentionally ignored.
 *
 * @param app Zero-initialized CANopenNode RT-Thread application instance.
 * @param runtime Zero-initialized caller-owned CiA 401 adapter storage.
 * @param config Persistent product IO/capability configuration.
 * @return RT_EOK on success, -RT_EINVAL for invalid arguments or a missing required output-fault
 *         classifier, -RT_EBUSY if the runtime or application is already attached/initialized,
 *         or the lifecycle registration error.
 */
rt_err_t CO_401_device_RTT_attach(CANopenNodeRTT *app, CO_401_device_RTT_t *runtime,
                                  const CO_401_device_RTT_config_t *config);

#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_AUTOSTART)
/** Lifecycle factory order reserved for the automatic local CiA 401 Device runtime. */
#define CO_401_DEVICE_RTT_AUTOSTART_FACTORY_ORDER 401U

/**
 * @brief Allocate and attach one lifecycle-owned CiA 401 adapter runtime.
 *
 * Only the adapter state is heap-owned. The IOIF and @c ioObject referenced by @p config
 * remain product-owned. Final lifecycle teardown releases the adapter after runtimeDeinit.
 * Output-capable auto-attached products must provide @c outputSupervisionEstablished because
 * the internally allocated runtime is not exposed for explicit supervision notification. The
 * probe is evaluated synchronously by the first output write after the qualifying event, so the
 * auto-attached path does not depend on a later worker pass. When
 * fail-safe support is enabled for any configured output capability, they must also provide
 * @c outputSupervisionFaultActive so a failure of the output-setting peer can be distinguished
 * from unrelated Heartbeat consumers.
 *
 * @param app Zero-initialized CANopenNode RT-Thread application instance.
 * @param config Persistent product IO/capability configuration.
 * @return RT_EOK on success, -RT_EINVAL for invalid arguments, -RT_EBUSY if this app already
 *         has a CiA 401 adapter, -RT_ENOMEM on allocation failure, or a lifecycle error.
 */
rt_err_t CO_401_device_RTT_autoAttach(CANopenNodeRTT *app, const CO_401_device_RTT_config_t *config);

/** Define and component-register one automatic CiA 401 factory for the default app. */
#define CO_401_DEVICE_RTT_AUTOSTART_DEFINE(name_, config_)                                                  \
    static rt_err_t name_##_co401_auto_attach(CANopenNodeRTT *app_, const void *context_)                  \
    {                                                                                                       \
        return CO_401_device_RTT_autoAttach(app_, (const CO_401_device_RTT_config_t *)context_);           \
    }                                                                                                       \
    static const CO_RTT_lifecycle_factory_t name_##_co401_factory = {                                      \
        .name = #name_,                                                                                     \
        .order = CO_401_DEVICE_RTT_AUTOSTART_FACTORY_ORDER,                                                 \
        .attach = name_##_co401_auto_attach,                                                                \
        .context = (config_),                                                                                \
    };                                                                                                      \
    static int name_##_co401_register(void)                                                                 \
    {                                                                                                       \
        return (int)CO_RTT_lifecycleFactoryRegister(&name_##_co401_factory);                               \
    }                                                                                                       \
    INIT_COMPONENT_EXPORT(name_##_co401_register)
#else
#define CO_401_DEVICE_RTT_AUTOSTART_DEFINE(name_, config_)
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_AUTOSTART */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* CO_401_DEVICE_RTT_H */
