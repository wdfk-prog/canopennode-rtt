/**
 * @file CO_402_device_RTT.c
 * @brief RT-Thread thread and communication-lifecycle adapter for CiA 402 Device.
 */

#define LOG_TAG "canopen.402"
#define LOG_LVL LOG_LVL_DBG

#include "CO_app_RTT.h"
#include "CO_402_device_RTT.h"
#include "CO_lifecycle_RTT.h"
#include "co_rtt_log.h"

#include <string.h>

#if defined(PKG_CANOPENNODE_CIA402_DEVICE_RTT_AUTOSTART)
/** Heap owner used only by the optional automatic factory; runtime is first for release casting. */
typedef struct {
    CO_402_device_RTT_t runtime;
    CO_402_device_axis_t *axes;
} CO_402_device_RTT_auto_owner_t;
#endif /* defined(PKG_CANOPENNODE_CIA402_DEVICE_RTT_AUTOSTART) */

/** Detach only OD extensions still owned by this Device runtime. */
static void CO_402_device_RTT_unbindOwnedExtensions(CO_402_device_RTT_t *runtime)
{
    uint8_t axisIndex;

    if (runtime == NULL || !runtime->managerInitialized || runtime->manager.axes == NULL) {
        return;
    }

    for (axisIndex = 0U; axisIndex < runtime->manager.axisCount; axisIndex++) {
        CO_402_device_axis_t *axis = &runtime->manager.axes[axisIndex];

        if (axis->od.controlword != NULL
            && axis->od.controlword->extension == &axis->od.controlwordExtension) {
            (void)OD_extension_init(axis->od.controlword, NULL);
        }
        if (axis->od.modesOfOperation != NULL
            && axis->od.modesOfOperation->extension == &axis->od.modeExtension) {
            (void)OD_extension_init(axis->od.modesOfOperation, NULL);
        }
    }

    runtime->manager.odBound = false;
}

/** Process one bounded supervisor pass under lifecycleMutex -> OD lock ordering. */
static void CO_402_device_RTT_threadEntry(void *parameter)
{
    CO_402_device_RTT_t *runtime = (CO_402_device_RTT_t *)parameter;
    CANopenNodeRTT *app = runtime->app;

    while (1) {
        CO_t *co;

        if (rt_sem_take(&runtime->cia402Sem, RT_WAITING_FOREVER) != RT_EOK) {
            continue;
        }

        if (rt_mutex_take(&app->lifecycleMutex, RT_WAITING_FOREVER) != RT_EOK) {
            continue;
        }

        co = app->canOpenStack;
        if (runtime->communicationReady == RT_TRUE
            && runtime->managerInitialized == RT_TRUE
            && co != NULL && co->CANmodule != NULL
            && !co->nodeIdUnconfigured && co->CANmodule->CANnormal) {
            /*
             * Keep the same lock order as co_rt. The thread is lower priority and
             * executes exactly one non-blocking Pure-C supervisor pass per token,
             * bounding priority-inheritance delay if a realtime tick arrives here.
             */
            CO_LOCK_OD(co->CANmodule);
            CO_402_device_process(&runtime->manager);
            CO_UNLOCK_OD(co->CANmodule);
        }

        (void)rt_mutex_release(&app->lifecycleMutex);
    }
}

/** Stop this Device from processing the communication generation being torn down. */
static void CO_402_device_RTT_onCommunicationStop(CANopenNodeRTT *app, void *context)
{
    CO_402_device_RTT_t *runtime = (CO_402_device_RTT_t *)context;

    (void)app;
    runtime->communicationReady = RT_FALSE;
}

/** Release Device-owned OD extensions only after the wrapper has drained old CAN RX. */
static void CO_402_device_RTT_onCommunicationQuiesced(CANopenNodeRTT *app, void *context)
{
    (void)app;
    CO_402_device_RTT_unbindOwnedExtensions((CO_402_device_RTT_t *)context);
}

/** Bind or rebind the Pure-C manager before SRDO/PDO cache the current OD IO. */
static rt_err_t CO_402_device_RTT_onCommunicationBind(CANopenNodeRTT *app, CO_t *co, OD_t *od, void *context)
{
    CO_402_device_RTT_t *runtime = (CO_402_device_RTT_t *)context;
    CO_402_init_diag_t diag;
    CO_402_init_error_t result;

    (void)co;
    memset(&diag, 0, sizeof(diag));
    if (runtime->managerInitialized != RT_TRUE) {
        result = CO_402_device_managerInit(&runtime->manager, od, runtime->config.axes,
                                           runtime->config.configs, runtime->config.axisCount, &diag);
        if (result == CO_402_INIT_OK) {
            runtime->managerInitialized = RT_TRUE;
        }
    } else {
        /*
         * Communication Reset recreates communication objects, not the physical
         * drive. Rebind the generated OD without resetting PDS state or issuing
         * any DriveIF power transition; that policy remains profile/product-owned.
         */
        runtime->manager.od = od;
        result = CO_402_device_bindOD(&runtime->manager, &diag);
    }

    if (result != CO_402_INIT_OK) {
        CO_RTT_LOG_E("CiA402 OD bind failed: dev=%s err=%d logical=%u index=0x%04x sub=%u",
                     app->canName != NULL ? app->canName : "?", (int)diag.error,
                     diag.logicalDevice, diag.index, diag.subIndex);
        return -RT_ERROR;
    }

    return RT_EOK;
}

/** Enable Device processing only after the current CAN module reached normal mode. */
static void CO_402_device_RTT_onCommunicationReady(CANopenNodeRTT *app, void *context)
{
    (void)app;
    ((CO_402_device_RTT_t *)context)->communicationReady = RT_TRUE;
}

/** Create the co_402 semaphore/thread after the initial OD binding has succeeded. */
static rt_err_t CO_402_device_RTT_onRuntimeInit(CANopenNodeRTT *app, void *context)
{
    CO_402_device_RTT_t *runtime = (CO_402_device_RTT_t *)context;
    rt_err_t ret;

    (void)app;
    if (runtime->managerInitialized != RT_TRUE || runtime->semInitialized == RT_TRUE
        || runtime->workerThread != RT_NULL) {
        return -RT_EBUSY;
    }
    if (PKG_CANOPENNODE_CIA402_THREAD_PRIORITY <= PKG_CANOPENNODE_RT_THREAD_PRIORITY) {
        CO_RTT_LOG_E("CiA402 thread priority must be lower than co_rt: co402=%u co_rt=%u",
                     PKG_CANOPENNODE_CIA402_THREAD_PRIORITY, PKG_CANOPENNODE_RT_THREAD_PRIORITY);
        return -RT_EINVAL;
    }

    ret = rt_sem_init(&runtime->cia402Sem, "402_sem", 0U, RT_IPC_FLAG_FIFO);
    if (ret != RT_EOK) {
        return ret;
    }
    runtime->semInitialized = RT_TRUE;

    runtime->workerThread = rt_thread_create("co_402", CO_402_device_RTT_threadEntry, runtime,
                                       PKG_CANOPENNODE_CIA402_THREAD_STACK_SIZE,
                                       PKG_CANOPENNODE_CIA402_THREAD_PRIORITY,
                                       PKG_CANOPENNODE_RT_THREAD_TICK);
    if (runtime->workerThread == RT_NULL) {
        (void)rt_sem_detach(&runtime->cia402Sem);
        runtime->semInitialized = RT_FALSE;
        return -RT_ENOMEM;
    }

    return RT_EOK;
}

/** Start the Device thread after co_rt has been started. */
static rt_err_t CO_402_device_RTT_onRuntimeStart(CANopenNodeRTT *app, void *context)
{
    CO_402_device_RTT_t *runtime = (CO_402_device_RTT_t *)context;
    rt_err_t ret;

    if (runtime->workerThread == RT_NULL || runtime->semInitialized != RT_TRUE) {
        return -RT_ERROR;
    }

    ret = rt_thread_startup(runtime->workerThread);
    if (ret == RT_EOK) {
        CO_RTT_LOG_I("CiA402 Device thread started: dev=%s axes=%u prio=%u", app->canName,
                     runtime->config.axisCount, PKG_CANOPENNODE_CIA402_THREAD_PRIORITY);
    }
    return ret;
}

/** Release one co_402 token from the existing shared realtime timer callback. */
static void CO_402_device_RTT_onRealtimeTick(CANopenNodeRTT *app, void *context)
{
    CO_402_device_RTT_t *runtime = (CO_402_device_RTT_t *)context;

    (void)app;
    if (runtime->semInitialized == RT_TRUE) {
        (void)rt_sem_release(&runtime->cia402Sem);
    }
}

/** Drain queued co_402 wake tokens while the shared realtime timer is stopped. */
static void CO_402_device_RTT_onResetWakeups(CANopenNodeRTT *app, void *context)
{
    CO_402_device_RTT_t *runtime = (CO_402_device_RTT_t *)context;

    (void)app;
    if (runtime->semInitialized == RT_TRUE) {
        (void)rt_sem_control(&runtime->cia402Sem, RT_IPC_CMD_RESET, RT_NULL);
    }
}

/** Delete Device thread resources after communication bindings have been quiesced. */
static void CO_402_device_RTT_onRuntimeDeinit(CANopenNodeRTT *app, void *context)
{
    CO_402_device_RTT_t *runtime = (CO_402_device_RTT_t *)context;

    (void)app;
    runtime->communicationReady = RT_FALSE;
    CO_402_device_RTT_onResetWakeups(app, context);

    if (runtime->workerThread != RT_NULL) {
        (void)rt_thread_delete(runtime->workerThread);
        runtime->workerThread = RT_NULL;
    }
    if (runtime->semInitialized == RT_TRUE) {
        (void)rt_sem_detach(&runtime->cia402Sem);
        runtime->semInitialized = RT_FALSE;
    }

    memset(&runtime->manager, 0, sizeof(runtime->manager));
    runtime->managerInitialized = RT_FALSE;
}

/** Static ops table registered by attach; registration order defines lifecycle ordering. */
static const CO_RTT_lifecycle_ops_t CO_402_device_RTT_lifecycleOps = {
    .runtimeInit = CO_402_device_RTT_onRuntimeInit,
    .runtimeStart = CO_402_device_RTT_onRuntimeStart,
    .communicationStop = CO_402_device_RTT_onCommunicationStop,
    .communicationQuiesced = CO_402_device_RTT_onCommunicationQuiesced,
    .communicationBind = CO_402_device_RTT_onCommunicationBind,
    .communicationReady = CO_402_device_RTT_onCommunicationReady,
    .realtimeTick = CO_402_device_RTT_onRealtimeTick,
    .resetWakeups = CO_402_device_RTT_onResetWakeups,
    .runtimeDeinit = CO_402_device_RTT_onRuntimeDeinit,
};

/** Shared attach implementation for caller-owned and lifecycle-owned adapter contexts. */
static rt_err_t CO_402_device_RTT_attachImpl(CANopenNodeRTT *app, CO_402_device_RTT_t *runtime,
                                              const CO_402_device_RTT_config_t *config,
                                              CO_RTT_lifecycle_context_release_t release)
{
    rt_err_t ret;

    if (app == NULL || runtime == NULL || config == NULL || config->axes == NULL || config->configs == NULL
        || config->axisCount == 0U || config->axisCount > CO_402_LOGICAL_DEVICE_COUNT_MAX) {
        return -RT_EINVAL;
    }
    if (runtime->attached == RT_TRUE || app->mainThread != RT_NULL || app->rtThread != RT_NULL
        || app->rtTimer != RT_NULL || app->canOpenStack != NULL) {
        return -RT_EBUSY;
    }
    if (PKG_CANOPENNODE_CIA402_THREAD_PRIORITY <= PKG_CANOPENNODE_RT_THREAD_PRIORITY) {
        CO_RTT_LOG_E("CiA402 thread priority must be lower than co_rt: co402=%u co_rt=%u",
                     PKG_CANOPENNODE_CIA402_THREAD_PRIORITY, PKG_CANOPENNODE_RT_THREAD_PRIORITY);
        return -RT_EINVAL;
    }

    memset(runtime, 0, sizeof(*runtime));
    runtime->app = app;
    runtime->config = *config;

    ret = CO_RTT_lifecycleRegisterEx(app, &CO_402_device_RTT_lifecycleOps, runtime, release);
    if (ret != RT_EOK) {
        memset(runtime, 0, sizeof(*runtime));
        return ret;
    }

    runtime->attached = RT_TRUE;
    return RT_EOK;
}

rt_err_t CO_402_device_RTT_attach(CANopenNodeRTT *app, CO_402_device_RTT_t *runtime,
                                   const CO_402_device_RTT_config_t *config)
{
    return CO_402_device_RTT_attachImpl(app, runtime, config, NULL);
}

#if defined(PKG_CANOPENNODE_CIA402_DEVICE_RTT_AUTOSTART)
/** Release the heap owner after lifecycle runtimeDeinit has finished using its runtime context. */
static void CO_402_device_RTT_releaseAutoContext(void *context)
{
    CO_402_device_RTT_auto_owner_t *owner = (CO_402_device_RTT_auto_owner_t *)context;

    if (owner == NULL) {
        return;
    }
    if (owner->axes != NULL) {
        rt_free(owner->axes);
        owner->axes = NULL;
    }
    rt_free(owner);
}

/** Allocate the CiA 402 auto-owned runtime and publish it through lifecycle registration. */
rt_err_t CO_402_device_RTT_autoAttach(CANopenNodeRTT *app, const CO_402_device_RTT_autostart_config_t *config)
{
    CO_402_device_RTT_auto_owner_t *owner;
    CO_402_device_RTT_config_t runtimeConfig;
    rt_err_t ret;

    if (app == NULL || config == NULL || config->configs == NULL || config->axisCount == 0U
        || config->axisCount > CO_402_LOGICAL_DEVICE_COUNT_MAX) {
        return -RT_EINVAL;
    }
    if (CO_RTT_lifecycleHasOps(app, &CO_402_device_RTT_lifecycleOps) == RT_TRUE) {
        return -RT_EBUSY;
    }

    owner = (CO_402_device_RTT_auto_owner_t *)rt_calloc(1U, sizeof(*owner));
    if (owner == NULL) {
        return -RT_ENOMEM;
    }
    owner->axes = (CO_402_device_axis_t *)rt_calloc(config->axisCount, sizeof(*owner->axes));
    if (owner->axes == NULL) {
        rt_free(owner);
        return -RT_ENOMEM;
    }

    runtimeConfig.axes = owner->axes;
    runtimeConfig.configs = config->configs;
    runtimeConfig.axisCount = config->axisCount;
    ret = CO_402_device_RTT_attachImpl(app, &owner->runtime, &runtimeConfig,
                                       CO_402_device_RTT_releaseAutoContext);
    if (ret != RT_EOK) {
        rt_free(owner->axes);
        rt_free(owner);
        return ret;
    }

    return RT_EOK;
}
#endif /* defined(PKG_CANOPENNODE_CIA402_DEVICE_RTT_AUTOSTART) */
