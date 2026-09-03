/**
 * @file CO_lifecycle_RTT.h
 * @brief Fixed-capacity lifecycle extension registry for CANopenNode RT-Thread applications.
 */

#ifndef CO_LIFECYCLE_RTT_H_
#define CO_LIFECYCLE_RTT_H_

#include <rtthread.h>
#include <stdint.h>

#include "CANopen.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct CANopenNodeRTT;
typedef struct CANopenNodeRTT CANopenNodeRTT;

#if defined(PKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSIONS)
#ifndef PKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSION_CAPACITY
#define PKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSION_CAPACITY 4
#endif /* PKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSION_CAPACITY */

/* The registry count is uint8_t; reject capacities it cannot represent. */
#if (PKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSION_CAPACITY < 1) \
    || (PKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSION_CAPACITY > 255)
#error "PKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSION_CAPACITY must be in range 1..255"
#endif /* lifecycle extension capacity range */

/** Configured number of statically registered lifecycle extensions per application instance. */
#define CO_RTT_LIFECYCLE_EXTENSION_CAPACITY PKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSION_CAPACITY
#endif /* defined(PKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSIONS) */

/**
 * @brief Ordered callbacks owned by one optional CANopenNodeRTT runtime extension.
 *
 * Acquisition callbacks run in registration order and teardown callbacks run in
 * reverse registration order. A callback that returns an error must undo any
 * side effects it created before returning; the dispatcher can only roll back
 * extensions whose preceding callback completed successfully.
 *
 * `communicationStop` runs before CAN RX shutdown so an extension can reject new
 * generation work. `communicationQuiesced` runs only after CAN RX has stopped and
 * must not access the old CAN module or OD mutex; it may release only bindings it
 * owns. `communicationBind` runs after CO_CANopenInit() and before SRDO/PDO init.
 * `communicationReady` runs after the new CAN module reaches normal mode.
 *
 * `realtimeTick` executes from the shared RT-Thread timer callback. It must be
 * bounded and non-blocking: no sleep, unbounded lock, heap allocation, or normal
 * logging is permitted. This timer hook is independent of mainline timerNext.
 */
typedef struct {
    rt_err_t (*runtimeInit)(CANopenNodeRTT *app, void *context); /**< Create extension-owned RT resources. */
    rt_err_t (*runtimeStart)(CANopenNodeRTT *app, void *context); /**< Start extension-owned workers. */
    void (*communicationStop)(CANopenNodeRTT *app, void *context); /**< Stop accepting work for the current CO_t. */
    void (*communicationQuiesced)(CANopenNodeRTT *app, void *context); /**< Release old communication bindings. */
    rt_err_t (*communicationBind)(CANopenNodeRTT *app, CO_t *co, OD_t *od, void *context); /**< Bind new OD state. */
    void (*communicationReady)(CANopenNodeRTT *app, void *context); /**< Allow work on the new communication state. */
    void (*realtimeTick)(CANopenNodeRTT *app, void *context); /**< Bounded callback from the shared realtime timer. */
    void (*resetWakeups)(CANopenNodeRTT *app, void *context); /**< Drain extension wake state while the timer is stopped. */
    void (*runtimeDeinit)(CANopenNodeRTT *app, void *context); /**< Release final extension state/resources. */
} CO_RTT_lifecycle_ops_t;

/** Release function for a lifecycle-owned extension context at final teardown. */
typedef void (*CO_RTT_lifecycle_context_release_t)(void *context);

#if defined(PKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSIONS)

/** One statically registered lifecycle extension slot. */
typedef struct {
    const CO_RTT_lifecycle_ops_t *ops;                  /**< Caller-owned immutable callback table. */
    void *context;                                      /**< Context retained until final lifecycle teardown. */
    CO_RTT_lifecycle_context_release_t release;         /**< Optional final release for lifecycle-owned contexts. */
    rt_bool_t runtimeInitialized;                       /**< True after runtimeInit; gates start/realtimeTick. */
    rt_bool_t communicationBound;                       /**< True while current communication bindings are owned. */
    rt_bool_t deinitRequired;                           /**< Final cleanup ownership retained across reset. */
} CO_RTT_lifecycle_slot_t;

/** Configurable fixed-capacity registry embedded in CANopenNodeRTT only when an extension selects it. */
typedef struct {
    CO_RTT_lifecycle_slot_t slots[CO_RTT_LIFECYCLE_EXTENSION_CAPACITY]; /**< Registration-order slots. */
    uint8_t count; /**< Number of valid slots; immutable while a CANopen runtime is active. */
} CO_RTT_lifecycle_t;

/**
 * @brief Register one lifecycle extension with optional final context ownership.
 *
 * Registration stores pointers only and creates no RT-Thread resource. If @p release
 * is non-NULL, lifecycle final teardown calls it after runtimeDeinit and removes the
 * slot, so the context may be heap-owned by an auto factory. Communication Reset does
 * not invoke @p release. Registration is rejected once CANopen runtime initialization
 * has started.
 *
 * @param app Zero-initialized CANopenNode RT-Thread application instance.
 * @param ops Persistent immutable lifecycle callback table.
 * @param context Extension context. It must remain valid until release or application lifetime end.
 * @param release Optional final context release callback; NULL keeps caller ownership.
 * @return RT_EOK on success, -RT_EINVAL for invalid arguments, -RT_EBUSY after
 *         runtime initialization has started or for a duplicate registration,
 *         or -RT_EFULL when the fixed registry is full.
 */
rt_err_t CO_RTT_lifecycleRegisterEx(CANopenNodeRTT *app, const CO_RTT_lifecycle_ops_t *ops, void *context,
                                    CO_RTT_lifecycle_context_release_t release);

/**
 * @brief Register one caller-owned lifecycle extension before CANopen runtime initialization.
 *
 * This is the caller-owned wrapper around CO_RTT_lifecycleRegisterEx() with a
 * NULL release callback. Once canopen_app_rtt_init() begins, the registry is
 * immutable so timer/thread dispatch can iterate it without another lock.
 *
 * @param app Zero-initialized CANopenNode RT-Thread application instance.
 * @param ops Persistent immutable lifecycle callback table.
 * @param context Persistent extension-owned context.
 * @return RT_EOK on success, -RT_EINVAL for invalid arguments, -RT_EBUSY after
 *         runtime initialization has started or for a duplicate registration,
 *         or -RT_EFULL when the fixed registry is full.
 */
rt_err_t CO_RTT_lifecycleRegister(CANopenNodeRTT *app, const CO_RTT_lifecycle_ops_t *ops, void *context);

/**
 * @brief Check whether an extension using the specified callback table is registered.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @param ops Lifecycle callback table identity to find.
 * @return RT_TRUE when at least one slot uses @p ops, otherwise RT_FALSE.
 */
rt_bool_t CO_RTT_lifecycleHasOps(const CANopenNodeRTT *app, const CO_RTT_lifecycle_ops_t *ops);

/**
 * @brief Check whether at least one lifecycle extension is registered.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @return RT_TRUE when at least one extension is registered, otherwise RT_FALSE.
 */
rt_bool_t CO_RTT_lifecycleHasExtensions(const CANopenNodeRTT *app);

#if defined(PKG_CANOPENNODE_RTT_LIFECYCLE_AUTOSTART)
/** Auto-factory callback that constructs and registers one or more owned lifecycle contexts. */
typedef rt_err_t (*CO_RTT_lifecycle_auto_attach_t)(CANopenNodeRTT *app, const void *factoryContext);

/** Static profile-neutral auto factory registered during RT-Thread component initialization. */
typedef struct {
    const char *name;                       /**< Persistent unique factory name. */
    uint16_t order;                         /**< Unique deterministic attach order; lower values run first. */
    CO_RTT_lifecycle_auto_attach_t attach;  /**< Create and register lifecycle-owned context(s). */
    const void *context;                    /**< Persistent factory configuration passed to attach. */
} CO_RTT_lifecycle_factory_t;

/**
 * @brief Register one static auto factory before application auto initialization.
 *
 * The global factory registry is fixed-capacity and allocation-free. Duplicate
 * descriptor pointers, names, or order values are rejected so link order cannot
 * silently decide lifecycle order. The first registration error is latched and
 * later makes CO_RTT_lifecycleAutoAttachAll() fail closed before any factory runs.
 * Registration is intentionally lock-free and must complete during system/component
 * initialization before concurrent app start.
 *
 * @param factory Persistent static factory descriptor.
 * @return RT_EOK on success, -RT_EINVAL for an invalid descriptor, -RT_EBUSY for
 *         duplicate descriptor/name/order, or -RT_EFULL when the factory registry is full.
 */
rt_err_t CO_RTT_lifecycleFactoryRegister(const CO_RTT_lifecycle_factory_t *factory);

/**
 * @brief Transactionally run all registered auto factories for one application instance.
 *
 * Factories execute in ascending order before canopen_app_rtt_init(). During this
 * transaction, CO_RTT_lifecycleRegisterEx() rejects slots without a release callback,
 * so cleanup ownership is published atomically with each auto-created context. If any
 * factory fails, all slots added by this call are released in reverse order and the
 * pre-existing manual registry prefix is restored. Recursive auto attach is rejected.
 *
 * @param app Zero-initialized CANopenNode RT-Thread application instance.
 * @return RT_EOK on success, -RT_EEMPTY when no factory is registered, or the
 *         first factory/ownership validation error.
 */
rt_err_t CO_RTT_lifecycleAutoAttachAll(CANopenNodeRTT *app);
#endif /* defined(PKG_CANOPENNODE_RTT_LIFECYCLE_AUTOSTART) */

/**
 * @brief Initialize registered extension RT resources in registration order.
 *
 * A failing callback owns rollback of its own partial initialization. A successful
 * communicationBind or runtimeInit may establish extension state that survives a
 * communication reset; the registry tracks that final cleanup ownership separately
 * from runtimeInitialized so early application-init failures still reach runtimeDeinit.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @return RT_EOK on success or the first callback error.
 */
rt_err_t CO_RTT_lifecycleRuntimeInit(CANopenNodeRTT *app);

/**
 * @brief Start initialized extensions in registration order.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @return RT_EOK on success or the first callback error.
 */
rt_err_t CO_RTT_lifecycleRuntimeStart(CANopenNodeRTT *app);

/**
 * @brief Stop current-generation processing in reverse registration order.
 *
 * This phase occurs before CAN RX shutdown. Hooks must stop accepting new work,
 * but must leave communication-owned memory/bindings intact until the quiesced
 * phase proves that no RX callback can still use them.
 *
 * @param app CANopenNode RT-Thread application instance.
 */
void CO_RTT_lifecycleCommunicationStop(CANopenNodeRTT *app);

/**
 * @brief Release current-generation bindings in reverse order after CAN RX is drained.
 *
 * The old CAN module and its OD mutex are no longer valid synchronization
 * primitives at this point. Hooks must not acquire or dereference them.
 *
 * @param app CANopenNode RT-Thread application instance.
 */
void CO_RTT_lifecycleCommunicationQuiesced(CANopenNodeRTT *app);

/**
 * @brief Bind extensions after CO_CANopenInit() and before SRDO/PDO initialization.
 *
 * A failing callback must undo its own partial binding before returning. The
 * dispatcher rolls back previously successful slots in reverse order.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @param co Current CANopenNode stack generation.
 * @param od Current generated Object Dictionary.
 * @return RT_EOK on success or the first callback error.
 */
rt_err_t CO_RTT_lifecycleBindCommunication(CANopenNodeRTT *app, CO_t *co, OD_t *od);

/**
 * @brief Mark successfully bound extensions ready after CAN reaches normal mode.
 *
 * @param app CANopenNode RT-Thread application instance.
 */
void CO_RTT_lifecycleCommunicationReady(CANopenNodeRTT *app);

/**
 * @brief Dispatch one bounded notification from the shared realtime timer callback.
 *
 * This callback path is independent of `PKG_CANOPENNODE_GLOBAL_TIMERNEXT`. Hooks
 * execute in timer-callback context and must only perform bounded, non-blocking
 * wake/snapshot work.
 *
 * @param app CANopenNode RT-Thread application instance.
 */
void CO_RTT_lifecycleRealtimeTick(CANopenNodeRTT *app);

/**
 * @brief Drain extension wake state while the shared realtime timer is stopped.
 *
 * @param app CANopenNode RT-Thread application instance.
 */
void CO_RTT_lifecycleResetWakeups(CANopenNodeRTT *app);

/**
 * @brief Deinitialize extension state/resources in reverse registration order.
 *
 * Communication bindings must already have reached the quiesced phase before
 * this function runs. Final cleanup ownership is independent of runtimeInitialized:
 * runtimeDeinit may run after a successful communicationBind even when application
 * initialization failed before that extension reached runtimeInit. After callback
 * teardown, slots with a non-NULL release callback are released and removed; manual
 * caller-owned slots remain registered so a failed initialization can be retried.
 *
 * @param app CANopenNode RT-Thread application instance.
 */
void CO_RTT_lifecycleRuntimeDeinit(CANopenNodeRTT *app);

#else

/* Feature-off macros preserve the original runtime call graph and add no per-instance state or RT resource. */
static inline rt_err_t CO_RTT_lifecycleRegisterEx(CANopenNodeRTT *app, const CO_RTT_lifecycle_ops_t *ops,
                                                   void *context, CO_RTT_lifecycle_context_release_t release)
{
    (void)app;
    (void)ops;
    (void)context;
    (void)release;
    return -RT_ENOSYS;
}

static inline rt_err_t CO_RTT_lifecycleRegister(CANopenNodeRTT *app, const CO_RTT_lifecycle_ops_t *ops, void *context)
{
    return CO_RTT_lifecycleRegisterEx(app, ops, context, NULL);
}

#define CO_RTT_lifecycleHasOps(app, ops)                (RT_FALSE)
#define CO_RTT_lifecycleHasExtensions(app)              (RT_FALSE)
#define CO_RTT_lifecycleRuntimeInit(app)                (RT_EOK)
#define CO_RTT_lifecycleRuntimeStart(app)               (RT_EOK)
#define CO_RTT_lifecycleCommunicationStop(app)          do { (void)(app); } while (0)
#define CO_RTT_lifecycleCommunicationQuiesced(app)      do { (void)(app); } while (0)
#define CO_RTT_lifecycleBindCommunication(app, co, od)  (RT_EOK)
#define CO_RTT_lifecycleCommunicationReady(app)         do { (void)(app); } while (0)
#define CO_RTT_lifecycleRealtimeTick(app)               do { (void)(app); } while (0)
#define CO_RTT_lifecycleResetWakeups(app)               do { (void)(app); } while (0)
#define CO_RTT_lifecycleRuntimeDeinit(app)               do { (void)(app); } while (0)

#endif /* defined(PKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSIONS) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_LIFECYCLE_RTT_H_ */
