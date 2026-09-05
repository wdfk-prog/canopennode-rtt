/**
 * @file CO_lifecycle_RTT.c
 * @brief Fixed-capacity lifecycle extension registry for CANopenNode RT-Thread applications.
 */

#include "CO_app_RTT.h"
#include "CO_lifecycle_RTT.h"

#include <string.h>

#if !defined(PKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSIONS)
#error "CO_lifecycle_RTT.c requires PKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSIONS"
#endif /* !defined(PKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSIONS) */

#if defined(PKG_CANOPENNODE_RTT_LIFECYCLE_AUTOSTART)
/** Process-wide factory registration and auto-attach transaction state. */
typedef struct {
    /** Static factories sorted by deterministic attach order. */
    const CO_RTT_lifecycle_factory_t *factories[CO_RTT_LIFECYCLE_EXTENSION_CAPACITY];
    /** Number of valid factory entries. */
    uint8_t count;
    /** First component-registration error carried forward to application auto init. */
    rt_err_t firstError;
    /** Application currently executing startup-only auto attach; also blocks recursion/cross-app publication. */
    CANopenNodeRTT *autoAttachApp;
} CO_RTT_lifecycle_factory_registry_t;

static CO_RTT_lifecycle_factory_registry_t CO_RTT_lifecycleFactoryRegistry = {
    .firstError = RT_EOK,
};

/**
 * @brief Latch the first factory registration error for the later application-init gate.
 *
 * @param error Factory registration error to preserve.
 * @return The original @p error so callers can return it directly.
 */
static rt_err_t CO_RTT_lifecycleLatchFactoryError(rt_err_t error)
{
    if (CO_RTT_lifecycleFactoryRegistry.firstError == RT_EOK) {
        CO_RTT_lifecycleFactoryRegistry.firstError = error;
    }
    return error;
}

/**
 * @brief Release auto-owned suffix slots created after a transaction checkpoint.
 *
 * @param app Application whose lifecycle registry is being rolled back.
 * @param checkpoint Slot count that must remain after rollback.
 */
static void CO_RTT_lifecycleRollbackAutoSlots(CANopenNodeRTT *app, uint8_t checkpoint)
{
    while (app->lifecycle.count > checkpoint) {
        CO_RTT_lifecycle_slot_t *slot = &app->lifecycle.slots[app->lifecycle.count - 1U];

        if (slot->release != NULL) {
            slot->release(slot->context);
        }
        memset(slot, 0, sizeof(*slot));
        app->lifecycle.count--;
    }
}
#endif /* defined(PKG_CANOPENNODE_RTT_LIFECYCLE_AUTOSTART) */

/** Register one lifecycle slot together with its optional final context-release ownership. */
rt_err_t CO_RTT_lifecycleRegisterEx(CANopenNodeRTT *app, const CO_RTT_lifecycle_ops_t *ops, void *context,
                                    CO_RTT_lifecycle_context_release_t release)
{
    uint8_t i;

    if (app == NULL || ops == NULL || (release != NULL && context == NULL)) {
        return -RT_EINVAL;
    }
#if defined(PKG_CANOPENNODE_RTT_LIFECYCLE_AUTOSTART)
    if (CO_RTT_lifecycleFactoryRegistry.autoAttachApp != NULL) {
        if (CO_RTT_lifecycleFactoryRegistry.autoAttachApp != app) {
            return -RT_EBUSY;
        }
        /* Factory-created slots must publish final cleanup ownership atomically with the context. */
        if (release == NULL) {
            return -RT_EINVAL;
        }
    }
#endif /* defined(PKG_CANOPENNODE_RTT_LIFECYCLE_AUTOSTART) */
    if (app->mainThread != RT_NULL || app->rtThread != RT_NULL || app->rtTimer != RT_NULL
        || app->canOpenStack != NULL) {
        return -RT_EBUSY;
    }

    for (i = 0U; i < app->lifecycle.count; i++) {
        if (app->lifecycle.slots[i].ops == ops && app->lifecycle.slots[i].context == context) {
            return -RT_EBUSY;
        }
    }
    if (app->lifecycle.count >= CO_RTT_LIFECYCLE_EXTENSION_CAPACITY) {
        return -RT_EFULL;
    }

    app->lifecycle.slots[app->lifecycle.count].ops = ops;
    app->lifecycle.slots[app->lifecycle.count].context = context;
    app->lifecycle.slots[app->lifecycle.count].release = release;
    app->lifecycle.count++;
    return RT_EOK;
}

/**
 * @brief Register one caller-owned lifecycle extension without a final release callback.
 *
 * This wrapper preserves caller ownership of @p context and delegates all validation and publication to
 * CO_RTT_lifecycleRegisterEx().
 *
 * @param app CANopenNode RT-Thread application instance.
 * @param ops Persistent lifecycle callback table.
 * @param context Caller-owned extension context.
 * @return Registration status returned by CO_RTT_lifecycleRegisterEx().
 */
rt_err_t CO_RTT_lifecycleRegister(CANopenNodeRTT *app, const CO_RTT_lifecycle_ops_t *ops, void *context)
{
    return CO_RTT_lifecycleRegisterEx(app, ops, context, NULL);
}

/**
 * @brief Test whether the application has a slot registered with the specified callback table.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @param ops Callback-table identity to search for.
 * @return RT_TRUE when a matching slot exists, otherwise RT_FALSE.
 */
rt_bool_t CO_RTT_lifecycleHasOps(const CANopenNodeRTT *app, const CO_RTT_lifecycle_ops_t *ops)
{
    uint8_t i;

    if (app == NULL || ops == NULL) {
        return RT_FALSE;
    }
    for (i = 0U; i < app->lifecycle.count; i++) {
        if (app->lifecycle.slots[i].ops == ops) {
            return RT_TRUE;
        }
    }
    return RT_FALSE;
}

/**
 * @brief Test whether the application has any registered lifecycle extension.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @return RT_TRUE when at least one slot is registered, otherwise RT_FALSE.
 */
rt_bool_t CO_RTT_lifecycleHasExtensions(const CANopenNodeRTT *app)
{
    return (app != NULL && app->lifecycle.count > 0U) ? RT_TRUE : RT_FALSE;
}

#if defined(PKG_CANOPENNODE_RTT_LIFECYCLE_AUTOSTART)
/** Register one static auto factory in deterministic order and latch setup failures. */
rt_err_t CO_RTT_lifecycleFactoryRegister(const CO_RTT_lifecycle_factory_t *factory)
{
    uint8_t i;
    uint8_t insertAt;

    if (factory == NULL || factory->name == NULL || factory->name[0] == '\0' || factory->attach == NULL) {
        return CO_RTT_lifecycleLatchFactoryError(-RT_EINVAL);
    }
    if (CO_RTT_lifecycleFactoryRegistry.autoAttachApp != NULL) {
        return CO_RTT_lifecycleLatchFactoryError(-RT_EBUSY);
    }

    for (i = 0U; i < CO_RTT_lifecycleFactoryRegistry.count; i++) {
        const CO_RTT_lifecycle_factory_t *registered = CO_RTT_lifecycleFactoryRegistry.factories[i];

        if (registered == factory || registered->order == factory->order
            || strcmp(registered->name, factory->name) == 0) {
            return CO_RTT_lifecycleLatchFactoryError(-RT_EBUSY);
        }
    }
    if (CO_RTT_lifecycleFactoryRegistry.count >= CO_RTT_LIFECYCLE_EXTENSION_CAPACITY) {
        return CO_RTT_lifecycleLatchFactoryError(-RT_EFULL);
    }

    insertAt = CO_RTT_lifecycleFactoryRegistry.count;
    while (insertAt > 0U && CO_RTT_lifecycleFactoryRegistry.factories[insertAt - 1U]->order > factory->order) {
        CO_RTT_lifecycleFactoryRegistry.factories[insertAt] = CO_RTT_lifecycleFactoryRegistry.factories[insertAt - 1U];
        insertAt--;
    }
    CO_RTT_lifecycleFactoryRegistry.factories[insertAt] = factory;
    CO_RTT_lifecycleFactoryRegistry.count++;
    return RT_EOK;
}

/** Run all registered factories as one owned-slot publication transaction. */
rt_err_t CO_RTT_lifecycleAutoAttachAll(CANopenNodeRTT *app)
{
    uint8_t factoryIndex;
    uint8_t checkpoint;

    if (app == NULL) {
        return -RT_EINVAL;
    }
    if (CO_RTT_lifecycleFactoryRegistry.autoAttachApp != NULL) {
        return -RT_EBUSY;
    }
    if (app->mainThread != RT_NULL || app->rtThread != RT_NULL || app->rtTimer != RT_NULL
        || app->canOpenStack != NULL) {
        return -RT_EBUSY;
    }
    if (CO_RTT_lifecycleFactoryRegistry.firstError != RT_EOK) {
        return CO_RTT_lifecycleFactoryRegistry.firstError;
    }
    if (CO_RTT_lifecycleFactoryRegistry.count == 0U) {
        return -RT_EEMPTY;
    }

    checkpoint = app->lifecycle.count;
    CO_RTT_lifecycleFactoryRegistry.autoAttachApp = app;
    for (factoryIndex = 0U; factoryIndex < CO_RTT_lifecycleFactoryRegistry.count; factoryIndex++) {
        const CO_RTT_lifecycle_factory_t *factory = CO_RTT_lifecycleFactoryRegistry.factories[factoryIndex];
        uint8_t factoryCheckpoint = app->lifecycle.count;
        uint8_t slotIndex;
        rt_err_t ret = factory->attach(app, factory->context);

        if (ret != RT_EOK) {
            CO_RTT_lifecycleRollbackAutoSlots(app, checkpoint);
            CO_RTT_lifecycleFactoryRegistry.autoAttachApp = NULL;
            return ret;
        }
        if (app->lifecycle.count <= factoryCheckpoint) {
            CO_RTT_lifecycleRollbackAutoSlots(app, checkpoint);
            CO_RTT_lifecycleFactoryRegistry.autoAttachApp = NULL;
            return -RT_ERROR;
        }
        for (slotIndex = factoryCheckpoint; slotIndex < app->lifecycle.count; slotIndex++) {
            if (app->lifecycle.slots[slotIndex].release == NULL) {
                CO_RTT_lifecycleRollbackAutoSlots(app, checkpoint);
                CO_RTT_lifecycleFactoryRegistry.autoAttachApp = NULL;
                return -RT_EINVAL;
            }
        }
    }

    CO_RTT_lifecycleFactoryRegistry.autoAttachApp = NULL;
    return RT_EOK;
}
#endif /* defined(PKG_CANOPENNODE_RTT_LIFECYCLE_AUTOSTART) */

/**
 * @brief Initialize registered extension runtime resources in registration order.
 *
 * Successfully initialized slots retain deinitialization ownership. On failure, already initialized slots remain
 * marked so the application cleanup path can quiesce communication first and then tear them down in reverse order.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @return RT_EOK on success or the first extension initialization error.
 */
rt_err_t CO_RTT_lifecycleRuntimeInit(CANopenNodeRTT *app)
{
    uint8_t i;

    if (app == NULL) {
        return -RT_EINVAL;
    }

    for (i = 0U; i < app->lifecycle.count; i++) {
        CO_RTT_lifecycle_slot_t *slot = &app->lifecycle.slots[i];
        rt_err_t ret = RT_EOK;

        if (slot->ops->runtimeInit != NULL) {
            ret = slot->ops->runtimeInit(app, slot->context);
        }
        if (ret != RT_EOK) {
            /*
             * Keep previously initialized slots marked. The application cleanup
             * path first stops communication and drains CAN RX, then deinitializes
             * them in reverse order; rolling them back here would violate that
             * ownership ordering while communication bindings are still live.
             */
            return ret;
        }
        slot->runtimeInitialized = RT_TRUE;
        slot->deinitRequired = RT_TRUE;
    }

    return RT_EOK;
}

/**
 * @brief Start runtime-initialized extensions in registration order.
 *
 * Only slots whose runtime initialization completed successfully are eligible for their runtimeStart callback.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @return RT_EOK on success or the first extension start error.
 */
rt_err_t CO_RTT_lifecycleRuntimeStart(CANopenNodeRTT *app)
{
    uint8_t i;

    if (app == NULL) {
        return -RT_EINVAL;
    }

    for (i = 0U; i < app->lifecycle.count; i++) {
        CO_RTT_lifecycle_slot_t *slot = &app->lifecycle.slots[i];

        if (slot->runtimeInitialized == RT_TRUE && slot->ops->runtimeStart != NULL) {
            rt_err_t ret = slot->ops->runtimeStart(app, slot->context);
            if (ret != RT_EOK) {
                return ret;
            }
        }
    }

    return RT_EOK;
}

/**
 * @brief Stop current-generation extension processing in reverse registration order.
 *
 * This phase runs before CAN RX is drained; callbacks may stop new work but must leave communication bindings intact
 * until CO_RTT_lifecycleCommunicationQuiesced() establishes that the old receive path can no longer use them.
 *
 * @param app CANopenNode RT-Thread application instance.
 */
void CO_RTT_lifecycleCommunicationStop(CANopenNodeRTT *app)
{
    uint8_t i;

    if (app == NULL) {
        return;
    }

    for (i = app->lifecycle.count; i > 0U; i--) {
        CO_RTT_lifecycle_slot_t *slot = &app->lifecycle.slots[i - 1U];
        if (slot->communicationBound == RT_TRUE && slot->ops->communicationStop != NULL) {
            slot->ops->communicationStop(app, slot->context);
        }
    }
}

/**
 * @brief Release current-generation communication bindings after CAN RX has been drained.
 *
 * Bound slots are visited in reverse order and their communicationBound flag is cleared only after the optional
 * quiesced callback returns, preventing later teardown from treating stale generation bindings as live.
 *
 * @param app CANopenNode RT-Thread application instance.
 */
void CO_RTT_lifecycleCommunicationQuiesced(CANopenNodeRTT *app)
{
    uint8_t i;

    if (app == NULL) {
        return;
    }

    for (i = app->lifecycle.count; i > 0U; i--) {
        CO_RTT_lifecycle_slot_t *slot = &app->lifecycle.slots[i - 1U];
        if (slot->communicationBound == RT_TRUE) {
            if (slot->ops->communicationQuiesced != NULL) {
                slot->ops->communicationQuiesced(app, slot->context);
            }
            slot->communicationBound = RT_FALSE;
        }
    }
}

/**
 * @brief Bind registered extensions to the current CANopen/OD generation in registration order.
 *
 * If a callback fails, previously bound slots are quiesced in reverse order. The failing callback remains responsible
 * for undoing its own partial side effects because its slot is not published as communicationBound on failure.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @param co Current CANopenNode stack generation.
 * @param od Current Object Dictionary.
 * @return RT_EOK on success or the first communication-bind error.
 */
rt_err_t CO_RTT_lifecycleBindCommunication(CANopenNodeRTT *app, CO_t *co, OD_t *od)
{
    uint8_t i;

    if (app == NULL || co == NULL || od == NULL) {
        return -RT_EINVAL;
    }

    for (i = 0U; i < app->lifecycle.count; i++) {
        CO_RTT_lifecycle_slot_t *slot = &app->lifecycle.slots[i];
        rt_err_t ret = RT_EOK;

        if (slot->communicationBound == RT_TRUE) {
            return -RT_EBUSY;
        }
        if (slot->ops->communicationBind != NULL) {
            ret = slot->ops->communicationBind(app, co, od, slot->context);
        }
        if (ret != RT_EOK) {
            while (i > 0U) {
                CO_RTT_lifecycle_slot_t *rollbackSlot = &app->lifecycle.slots[--i];
                if (rollbackSlot->communicationBound == RT_TRUE) {
                    if (rollbackSlot->ops->communicationQuiesced != NULL) {
                        rollbackSlot->ops->communicationQuiesced(app, rollbackSlot->context);
                    }
                    rollbackSlot->communicationBound = RT_FALSE;
                }
            }
            return ret;
        }
        slot->communicationBound = RT_TRUE;
        if (slot->ops->communicationBind != NULL) {
            /* Bind-owned profile state survives communication reset and is released only at final teardown. */
            slot->deinitRequired = RT_TRUE;
        }
    }

    return RT_EOK;
}

/**
 * @brief Notify successfully bound extensions that the current communication generation is ready.
 *
 * Only slots that still own a current-generation binding receive the notification.
 *
 * @param app CANopenNode RT-Thread application instance.
 */
void CO_RTT_lifecycleCommunicationReady(CANopenNodeRTT *app)
{
    uint8_t i;

    if (app == NULL) {
        return;
    }

    for (i = 0U; i < app->lifecycle.count; i++) {
        CO_RTT_lifecycle_slot_t *slot = &app->lifecycle.slots[i];
        if (slot->communicationBound == RT_TRUE && slot->ops->communicationReady != NULL) {
            slot->ops->communicationReady(app, slot->context);
        }
    }
}

/**
 * @brief Dispatch synchronous extension processing under the caller-owned co_rt lock boundary.
 *
 * The registry is immutable while a runtime is active. This dispatcher therefore
 * only walks the fixed slot array; extensions own their bounded per-SYNC work.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @param dtUs Elapsed realtime period for this synchronous processing pass.
 */
void CO_RTT_lifecycleSynchronousProcess(CANopenNodeRTT *app, uint32_t dtUs)
{
    uint8_t i;

    if (app == NULL) {
        return;
    }

    for (i = 0U; i < app->lifecycle.count; i++) {
        CO_RTT_lifecycle_slot_t *slot = &app->lifecycle.slots[i];

        if (slot->runtimeInitialized == RT_TRUE && slot->communicationBound == RT_TRUE
            && slot->ops->synchronousProcess != NULL) {
            slot->ops->synchronousProcess(app, slot->context, dtUs);
        }
    }
}

/**
 * @brief Dispatch one realtime-timer notification to runtime-initialized extensions.
 *
 * This path executes from the shared RT-Thread timer callback, so extension callbacks must remain bounded and
 * non-blocking. Dispatch is independent of mainline timerNext configuration.
 *
 * @param app CANopenNode RT-Thread application instance.
 */
void CO_RTT_lifecycleRealtimeTick(CANopenNodeRTT *app)
{
    uint8_t i;

    if (app == NULL) {
        return;
    }

    for (i = 0U; i < app->lifecycle.count; i++) {
        CO_RTT_lifecycle_slot_t *slot = &app->lifecycle.slots[i];
        if (slot->runtimeInitialized == RT_TRUE && slot->ops->realtimeTick != NULL) {
            slot->ops->realtimeTick(app, slot->context);
        }
    }
}

/**
 * @brief Drain extension wake state in reverse order while the shared realtime timer is stopped.
 *
 * Only runtime-initialized slots participate, keeping reset wake cleanup aligned with resources that actually exist.
 *
 * @param app CANopenNode RT-Thread application instance.
 */
void CO_RTT_lifecycleResetWakeups(CANopenNodeRTT *app)
{
    uint8_t i;

    if (app == NULL) {
        return;
    }

    for (i = app->lifecycle.count; i > 0U; i--) {
        CO_RTT_lifecycle_slot_t *slot = &app->lifecycle.slots[i - 1U];
        if (slot->runtimeInitialized == RT_TRUE && slot->ops->resetWakeups != NULL) {
            slot->ops->resetWakeups(app, slot->context);
        }
    }
}

/**
 * @brief Perform final extension teardown in reverse registration order.
 *
 * deinitRequired, rather than runtimeInitialized, owns final callback cleanup so state acquired during communication
 * binding is released even when later runtime initialization fails. Auto-owned contexts are released and removed;
 * manual caller-owned slots remain registered so a failed application initialization can be retried.
 *
 * @param app CANopenNode RT-Thread application instance.
 */
void CO_RTT_lifecycleRuntimeDeinit(CANopenNodeRTT *app)
{
    uint8_t i;
    uint8_t writeIndex;

    if (app == NULL) {
        return;
    }

    for (i = app->lifecycle.count; i > 0U; i--) {
        CO_RTT_lifecycle_slot_t *slot = &app->lifecycle.slots[i - 1U];

        /* runtimeInitialized gates execution; deinitRequired owns final extension cleanup across resets. */
        if (slot->deinitRequired == RT_TRUE) {
            if (slot->ops->runtimeDeinit != NULL) {
                slot->ops->runtimeDeinit(app, slot->context);
            }
            slot->deinitRequired = RT_FALSE;
        }
        slot->runtimeInitialized = RT_FALSE;

        /* Auto-owned contexts are released only at final teardown, never on Communication Reset. */
        if (slot->release != NULL) {
            slot->release(slot->context);
            memset(slot, 0, sizeof(*slot));
        }
    }

    /* Keep manual caller-owned slots registered so a failed application init may be retried. */
    writeIndex = 0U;
    for (i = 0U; i < app->lifecycle.count; i++) {
        if (app->lifecycle.slots[i].ops != NULL) {
            if (writeIndex != i) {
                app->lifecycle.slots[writeIndex] = app->lifecycle.slots[i];
                memset(&app->lifecycle.slots[i], 0, sizeof(app->lifecycle.slots[i]));
            }
            writeIndex++;
        }
    }
    app->lifecycle.count = writeIndex;
}
