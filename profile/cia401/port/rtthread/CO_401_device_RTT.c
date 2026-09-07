/**
 * @file CO_401_device_RTT.c
 * @brief RT-Thread lifecycle integration for the Pure-C CiA 401 Device core.
 */
#include <string.h>

#include "CO_401_device_RTT.h"
#include "CO_app_RTT.h"
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS) \
    && !defined(PKG_CANOPENNODE_RTT_CAN_TX_SUCCESS_OBSERVER)
#error "CiA 401 analogue events require PKG_CANOPENNODE_RTT_CAN_TX_SUCCESS_OBSERVER"
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS && !PKG_CANOPENNODE_RTT_CAN_TX_SUCCESS_OBSERVER */

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
#include "OD.h"
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */

#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE) \
    || defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
#include "301/CO_Emergency.h"
#endif /* defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE)
          || defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
          || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE) */
#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE)
#define CO_401_RTT_EMCY_STATUS_BIT 0x48U
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE */

/** Detach only extensions still owned by this adapter after old CAN RX is quiesced. */
static void unbindOwnedExtensions(CO_401_device_RTT_t *runtime)
{
    if (runtime == NULL || runtime->deviceInitialized != RT_TRUE) {
        return;
    }
    if (runtime->device.bound.digitalOutput8 != NULL
        && runtime->device.bound.digitalOutput8->extension == &runtime->device.digitalOutput8Extension) {
        (void)OD_extension_init(runtime->device.bound.digitalOutput8, NULL);
    }
    if (runtime->device.bound.analogOutput16 != NULL
        && runtime->device.bound.analogOutput16->extension == &runtime->device.analogOutput16Extension) {
        (void)OD_extension_init(runtime->device.bound.analogOutput16, NULL);
    }
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    if (runtime->device.bound.digitalInput8 != NULL
        && runtime->device.bound.digitalInput8->extension == &runtime->device.digitalInput8Extension) {
        (void)OD_extension_init(runtime->device.bound.digitalInput8, NULL);
    }
    if (runtime->device.bound.digitalInputFilter8 != NULL
        && runtime->device.bound.digitalInputFilter8->extension == &runtime->device.digitalInputFilter8Extension) {
        (void)OD_extension_init(runtime->device.bound.digitalInputFilter8, NULL);
    }
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS */
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
    if (runtime->device.bound.analogInput16 != NULL
        && runtime->device.bound.analogInput16->extension == &runtime->device.analogInput16Extension) {
        (void)OD_extension_init(runtime->device.bound.analogInput16, NULL);
    }
    if (runtime->device.bound.analogInterruptSource != NULL
        && runtime->device.bound.analogInterruptSource->extension == &runtime->device.analogInterruptSourceExtension) {
        (void)OD_extension_init(runtime->device.bound.analogInterruptSource, NULL);
    }
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */
    /*
     * Quiesced is the last legal access to the retired OD generation. Invalidate cached entries now so
     * the next bind cannot dereference storage after the previous generation owner releases it.
     */
    (void)memset(&runtime->device.bound, 0, sizeof(runtime->device.bound));
    runtime->device.od = NULL;
    runtime->device.odBound = false;
}

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
static bool hasOutputCommunicationFault(const CO_401_device_RTT_t *runtime, const CO_t *co,
                                        bool supervisionEstablished)
{
    if (co == NULL) {
        return false;
    }

    if (co->em != NULL && CO_isError(co->em, CO_EM_CAN_TX_BUS_OFF)) {
        return true;
    }
    /*
     * CO_EM_HEARTBEAT_CONSUMER is shared by all Heartbeat consumers and node-guarding users,
     * so that aggregate bit cannot identify the peer responsible for setting this device's outputs.
     */
    if (runtime != NULL && supervisionEstablished
        && runtime->config.outputSupervisionFaultActive != NULL) {
        return runtime->config.outputSupervisionFaultActive(runtime->config.outputSupervisionObject, co);
    }
    return false;
}
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */

static bool outputSupervisionEstablishedProbe(void *object)
{
    CO_401_device_RTT_t *runtime = (CO_401_device_RTT_t *)object;
    CANopenNodeRTT *app;
    CO_t *co;

    if (runtime == NULL || runtime->communicationReady != RT_TRUE
        || runtime->deviceInitialized != RT_TRUE
        || runtime->config.outputSupervisionEstablished == NULL) {
        return false;
    }

    app = runtime->app;
    co = app != NULL ? app->canOpenStack : NULL;
    if (co == NULL || co->CANmodule == NULL || co->NMT == NULL
        || co->nodeIdUnconfigured || !co->CANmodule->CANnormal) {
        return false;
    }

    /*
     * The worker and output OD write gate may sample this read-only probe concurrently.
     * Product code publishes a monotonic supervision fact with cross-context visibility;
     * peer identity still comes from the product and never from the aggregate HB bit.
     */
    return runtime->config.outputSupervisionEstablished(runtime->config.outputSupervisionObject, co);
}

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
static uint16_t sdoServerCount(const CO_t *co)
{
#ifdef CO_MULTIPLE_OD
    return (co != NULL && co->config != NULL) ? co->config->CNT_SDO_SRV : 0U;
#else
    (void)co;
    return (uint16_t)OD_CNT_SDO_SRV;
#endif /* CO_MULTIPLE_OD */
}

static uint16_t tpdoCount(const CO_t *co)
{
#ifdef CO_MULTIPLE_OD
    return (co != NULL && co->config != NULL) ? co->config->CNT_TPDO : 0U;
#else
    (void)co;
    return (uint16_t)OD_CNT_TPDO;
#endif /* CO_MULTIPLE_OD */
}

static bool isSdoReadStream(void *object, const OD_stream_t *stream)
{
    CO_401_device_RTT_t *runtime = (CO_401_device_RTT_t *)object;
    CANopenNodeRTT *app;
    CO_t *co;
    uint16_t i;

    if (runtime == NULL || stream == NULL || runtime->communicationReady != RT_TRUE) {
        return false;
    }
    app = runtime->app;
    co = app != NULL ? app->canOpenStack : NULL;
    if (co == NULL || co->SDOserver == NULL) {
        return false;
    }

    /*
     * Match the SDO server's persistent OD_IO stream object, not the wire-protocol
     * state. Expedited, segmented and block uploads all read through this stream.
     */
    for (i = 0U; i < sdoServerCount(co); i++) {
        if (stream == &co->SDOserver[i].OD_IO.stream && co->SDOserver[i].OD == runtime->device.od) {
            return true;
        }
    }
    return false;
}

static CO_TPDO_t *findTpdoByTxBuffer(CO_t *co, const CO_CANtx_t *buffer)
{
    uint16_t i;

    if (co == NULL || co->TPDO == NULL || buffer == NULL) {
        return NULL;
    }

    for (i = 0U; i < tpdoCount(co); i++) {
        if (co->TPDO[i].CANtxBuff == buffer) {
            return &co->TPDO[i];
        }
    }
    return NULL;
}

static uint16_t tpdoMappedLengthBits(const OD_IO_t *io)
{
#if ((CO_CONFIG_PDO)&CO_CONFIG_PDO_BITWISE_MAPPING) != 0
    return (uint16_t)io->stream.dataOffset;
#else
    return (uint16_t)(io->stream.dataOffset * 8U);
#endif /* ((CO_CONFIG_PDO)&CO_CONFIG_PDO_BITWISE_MAPPING) != 0 */
}

static bool readTpdoBits(const CO_CANtx_t *buffer, uint16_t bitOffset, uint16_t bitLength, uint32_t *value)
{
    uint32_t decoded = 0U;
    uint16_t bit;

    if (buffer == NULL || value == NULL || bitLength == 0U || bitLength > 32U
        || buffer->DLC > sizeof(buffer->data)
        || (uint32_t)bitOffset + bitLength > (uint32_t)buffer->DLC * 8U) {
        return false;
    }

    /* CANopenNode bitwise PDO mapping appends each object's low-order bits at the cumulative wire bit offset. */
    for (bit = 0U; bit < bitLength; bit++) {
        const uint16_t frameBit = (uint16_t)(bitOffset + bit);
        const uint8_t byte = buffer->data[frameBit >> 3];
        const uint8_t mask = (uint8_t)(1U << (frameBit & 7U));

        if ((byte & mask) != 0U) {
            decoded |= (uint32_t)1UL << bit;
        }
    }
    *value = decoded;
    return true;
}

static void commitSuccessfulTpdo(CO_401_device_RTT_t *runtime, CO_TPDO_t *tpdo, const CO_CANtx_t *buffer)
{
    CO_PDO_common_t *pdo = &tpdo->PDO_common;
    uint16_t bitOffset = 0U;
    uint8_t i;

    for (i = 0U; i < pdo->mappedObjectsCount; i++) {
        OD_IO_t *io = &pdo->OD_IO[i];
        const uint16_t mappedBits = tpdoMappedLengthBits(io);
        uint32_t communicated;

        if (io->stream.index == CO_401_INDEX_ANALOG_INPUT_16 && mappedBits > 0U && mappedBits <= 16U
            && readTpdoBits(buffer, bitOffset, mappedBits, &communicated)) {
            if (mappedBits == 16U) {
                CO_401_device_commitAnalogInputTpdoCommunication(&runtime->device, io->stream.subIndex,
                                                                  (int16_t)(uint16_t)communicated);
            } else {
                /* A partial wire value retires only the event retry; it is not a valid delta reference. */
                CO_401_device_retireAnalogInputTpdoEvent(&runtime->device, io->stream.subIndex);
            }
        } else if (io->stream.index == CO_401_INDEX_ANALOG_INTERRUPT_SOURCE && mappedBits > 0U
                   && mappedBits <= 32U && readTpdoBits(buffer, bitOffset, mappedBits, &communicated)) {
            /* communicated contains only the transmitted low-order bits, so unmapped source bits remain latched. */
            CO_401_device_commitAnalogSourceCommunication(&runtime->device, io->stream.subIndex, communicated);
        }
        bitOffset = (uint16_t)(bitOffset + mappedBits);
    }
}

/** Commit TPDO-derived CiA 401 state only after the RT-Thread CAN path accepted the frame. */
static void onCanTxSuccess(void *object, CO_CANtx_t *buffer)
{
    CO_401_device_RTT_t *runtime = (CO_401_device_RTT_t *)object;
    CANopenNodeRTT *app;
    CO_t *co;
    CO_TPDO_t *tpdo;

    if (runtime == NULL || runtime->communicationReady != RT_TRUE || runtime->deviceInitialized != RT_TRUE) {
        return;
    }

    app = runtime->app;
    co = app != NULL ? app->canOpenStack : NULL;
    if (co == NULL || co->CANmodule == NULL || buffer == NULL) {
        return;
    }

    tpdo = findTpdoByTxBuffer(co, buffer);
    if (tpdo == NULL) {
        return;
    }

    /* Normal RT-Thread TPDO processing holds the OD lock across CO_process_TPDO() and CO_CANsend(). */
    if (app->rtThread != RT_NULL && rt_thread_self() == app->rtThread) {
        commitSuccessfulTpdo(runtime, tpdo, buffer);
    } else {
        CO_LOCK_OD(co->CANmodule);
        commitSuccessfulTpdo(runtime, tpdo, buffer);
        CO_UNLOCK_OD(co->CANmodule);
    }
}
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */

/** Preserve NMT transition facts in mainline order instead of reconstructing them from worker snapshots. */
static void onNmtStateChanged(CANopenNodeRTT *app, void *context, CO_NMT_internalState_t state)
{
    CO_401_device_RTT_t *runtime = (CO_401_device_RTT_t *)context;
    CO_t *co;
#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE)
    CO_401_error_event_t event;
    bool haveEvent = false;
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE */

    /*
     * Pin the current CO_t/OD generation before publishing the edge into profile state. This keeps the same
     * lifecycleMutex -> OD-lock order as the worker and prevents Communication Reset from retiring the generation
     * between the transition snapshot and its warning/fail-safe obligation.
     */
    if (app == NULL || runtime == NULL
        || rt_mutex_take(&app->lifecycleMutex, RT_WAITING_FOREVER) != RT_EOK) {
        return;
    }

    co = app->canOpenStack;
    if (runtime->communicationReady == RT_TRUE && runtime->deviceInitialized == RT_TRUE
        && co != NULL && co->CANmodule != NULL && co->NMT != NULL
        && !co->nodeIdUnconfigured && co->CANmodule->CANnormal) {
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
        runtime->nmtState = state;
        if (state == CO_NMT_STOPPED) {
            /* Stopped is a one-shot fail-safe obligation even if a later state arrives before the worker runs. */
            runtime->nmtStoppedApplyPending = RT_TRUE;
        }
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */

        CO_LOCK_OD(co->CANmodule);
        CO_401_device_setNmtOperational(&runtime->device, state == CO_NMT_OPERATIONAL);
#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE)
        if (co->em != NULL) {
            haveEvent = CO_401_device_takeErrorEvent(&runtime->device, &event);
        }
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE */
        CO_UNLOCK_OD(co->CANmodule);

#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE)
        if (co->em != NULL) {
            if (state != CO_NMT_OPERATIONAL && runtime->analogWarningReported == RT_TRUE) {
                CO_errorReset(co->em, CO_401_RTT_EMCY_STATUS_BIT, 0U);
                runtime->analogWarningReported = RT_FALSE;
            }
            if (haveEvent) {
                CO_errorReport(co->em, CO_401_RTT_EMCY_STATUS_BIT, event.errorCode, 0U);
                runtime->analogWarningReported = RT_TRUE;
            }
        }
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE */
    }

    (void)rt_mutex_release(&app->lifecycleMutex);
}

/** Process one bounded profile pass under lifecycleMutex -> OD lock ordering. */
static void workerEntry(void *parameter)
{
    CO_401_device_RTT_t *runtime = (CO_401_device_RTT_t *)parameter;
    CANopenNodeRTT *app = runtime->app;

    while (1) {
        CO_t *co;

        if (rt_sem_take(&runtime->cia401Sem, RT_WAITING_FOREVER) != RT_EOK) {
            continue;
        }
        /* A token represents "process the latest state", so one in-flight pass re-opens exactly one future wake slot. */
        rt_atomic_store(&runtime->wakePending, 0);
        if (rt_mutex_take(&app->lifecycleMutex, RT_WAITING_FOREVER) != RT_EOK) {
            continue;
        }

        co = app->canOpenStack;
        if (runtime->communicationReady == RT_TRUE
            && runtime->deviceInitialized == RT_TRUE
            && co != NULL && co->CANmodule != NULL && co->NMT != NULL
            && !co->nodeIdUnconfigured && co->CANmodule->CANnormal) {
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
            const bool stopped = runtime->nmtState == CO_NMT_STOPPED;
            const bool applyStopped = stopped || runtime->nmtStoppedApplyPending == RT_TRUE;
            bool outputSupervisionReady;
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */

            /*
             * SDO/RPDO output writes publish this monotonic latch under the OD lock.
             * Use the same lock for the worker's false->true update and snapshot so
             * the two contexts never race on outputSupervisionReady.
             */
            CO_LOCK_OD(co->CANmodule);
            if (!runtime->device.outputSupervisionReady
                && runtime->config.outputSupervisionEstablished != NULL
                && outputSupervisionEstablishedProbe(runtime)) {
                CO_401_device_notifyOutputSupervision(&runtime->device);
            }
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
            outputSupervisionReady = runtime->device.outputSupervisionReady;
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
            CO_UNLOCK_OD(co->CANmodule);

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
            /*
             * Product peer-fault classification stays outside the OD lock by callback contract.
             * The supervision snapshot is monotonic; a later latch is observed on the next worker pass.
             */
            const bool communicationFault = hasOutputCommunicationFault(runtime, co, outputSupervisionReady);
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */

            /*
             * lifecycleMutex pins this CO_t generation across both OD-serialized phases.
             * Re-enter the OD lock after the product callback so Device state and process-image
             * updates remain serialized without invoking product fault logic under that lock.
             */
            CO_LOCK_OD(co->CANmodule);
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
            /*
             * A transient Stopped edge stays sticky until every required fail-safe backend write returns OK.
             * Merely attempting one process pass is insufficient because BUSY/ERROR keeps the old physical output.
             */
            CO_401_device_setNmtStopped(&runtime->device, applyStopped);
            CO_401_device_setCommunicationFault(&runtime->device, communicationFault);
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
            CO_401_device_process(&runtime->device);
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
            if (runtime->nmtStoppedApplyPending == RT_TRUE && runtime->device.failSafeOutputApplyComplete) {
                runtime->nmtStoppedApplyPending = RT_FALSE;
                if (!stopped) {
                    CO_401_device_setNmtStopped(&runtime->device, false);
                }
            }
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
            CO_UNLOCK_OD(co->CANmodule);
        }

        (void)rt_mutex_release(&app->lifecycleMutex);
    }
}

/** Bind/rebind the Pure-C Device before SRDO/PDO cache current OD IO callbacks. */
static rt_err_t onBind(CANopenNodeRTT *app, CO_t *co, OD_t *od, void *context)
{
    CO_401_device_RTT_t *runtime = (CO_401_device_RTT_t *)context;
    CO_401_init_diag_t diag;
    CO_401_init_error_t result;

    (void)app;
    (void)co;
    if (runtime->deviceInitialized != RT_TRUE) {
        result = CO_401_device_init(&runtime->device, od, &runtime->config.device, &diag);
        if (result == CO_401_INIT_OK) {
            runtime->deviceInitialized = RT_TRUE;
        }
    } else {
        runtime->device.od = od;
        result = CO_401_device_bindOD(&runtime->device, &diag);
    }
    if (result == CO_401_INIT_OK) {
        CO_401_device_setOutputSupervisionProbe(
            &runtime->device, runtime,
            runtime->config.outputSupervisionEstablished != NULL ? outputSupervisionEstablishedProbe : NULL);
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
        /* Classify SDO reads by OD_IO stream identity; no SDO wire-format parsing is required. */
        CO_401_device_setSdoReadMatcher(&runtime->device, runtime, isSdoReadStream);
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */
    }
    return result == CO_401_INIT_OK ? RT_EOK : -RT_ERROR;
}

/** Stop the worker from observing a communication generation being torn down. */
static void onStop(CANopenNodeRTT *app, void *context)
{
    CO_401_device_RTT_t *runtime = (CO_401_device_RTT_t *)context;

    /* Close the generation gate before unregistering so an already captured callback fails closed. */
    runtime->communicationReady = RT_FALSE;
    CO_401_device_setOutputSupervisionProbe(&runtime->device, NULL, NULL);
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
    CO_401_device_setSdoReadMatcher(&runtime->device, NULL, NULL);
    if (app != NULL && app->canOpenStack != NULL && app->canOpenStack->CANmodule != NULL) {
        /*
         * Reset/deinit callers quiesce CANopen processing with lifecycleMutex before this hook,
         * so no TPDO sender can retain a callback capture when the runtime is retired.
         */
        CO_RTT_CANsetTxSuccessCallback(app->canOpenStack->CANmodule, NULL, NULL);
    }
#else
    (void)app;
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    /* Retire generation-local transition obligations before the next OD is published. */
    runtime->nmtState = CO_NMT_UNKNOWN;
    runtime->nmtStoppedApplyPending = RT_FALSE;
    CO_401_device_setNmtStopped(&runtime->device, false);
    CO_401_device_setCommunicationFault(&runtime->device, false);
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
    /* Old-generation NMT edge/pending warning state must not survive Communication Reset. */
    CO_401_device_resetCommunicationState(&runtime->device);
#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE)
    runtime->analogWarningReported = RT_FALSE;
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE */
}

/** Release old OD extension ownership only after CAN RX has quiesced. */
static void onQuiesced(CANopenNodeRTT *app, void *context)
{
    (void)app;
    unbindOwnedExtensions((CO_401_device_RTT_t *)context);
}

/** Publish the newly bound communication generation to the worker. */
static void onReady(CANopenNodeRTT *app, void *context)
{
    CO_401_device_RTT_t *runtime = (CO_401_device_RTT_t *)context;

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
    if (app != NULL && app->canOpenStack != NULL && app->canOpenStack->CANmodule != NULL) {
        CO_RTT_CANsetTxSuccessCallback(app->canOpenStack->CANmodule, runtime, onCanTxSuccess);
    }
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    runtime->nmtState = (app != NULL && app->canOpenStack != NULL && app->canOpenStack->NMT != NULL)
                            ? CO_NMT_getInternalState(app->canOpenStack->NMT)
                            : CO_NMT_UNKNOWN;
    runtime->nmtStoppedApplyPending = RT_FALSE;
#elif !defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
    (void)app;
#endif /* output fail-safe or no app consumer */
    runtime->communicationReady = RT_TRUE;
}

/** Allocate RT resources only after the initial Device/OD binding succeeds. */
static rt_err_t onRuntimeInit(CANopenNodeRTT *app, void *context)
{
    CO_401_device_RTT_t *runtime = (CO_401_device_RTT_t *)context;
    rt_err_t ret;

    (void)app;
    if (runtime->deviceInitialized != RT_TRUE
        || runtime->semInitialized == RT_TRUE || runtime->workerThread != RT_NULL) {
        return -RT_EBUSY;
    }
    if (PKG_CANOPENNODE_CIA401_THREAD_PRIORITY <= PKG_CANOPENNODE_RT_THREAD_PRIORITY) {
        return -RT_EINVAL;
    }

    ret = rt_sem_init(&runtime->cia401Sem, "401_sem", 0U, RT_IPC_FLAG_FIFO);
    if (ret != RT_EOK) {
        return ret;
    }
    rt_atomic_store(&runtime->wakePending, 0);
    runtime->semInitialized = RT_TRUE;
    runtime->workerThread = rt_thread_create(
        "co_401", workerEntry, runtime, PKG_CANOPENNODE_CIA401_THREAD_STACK_SIZE,
        PKG_CANOPENNODE_CIA401_THREAD_PRIORITY, PKG_CANOPENNODE_RT_THREAD_TICK);
    if (runtime->workerThread == RT_NULL) {
        (void)rt_sem_detach(&runtime->cia401Sem);
        runtime->semInitialized = RT_FALSE;
        return -RT_ENOMEM;
    }
    return RT_EOK;
}

static rt_err_t onRuntimeStart(CANopenNodeRTT *app, void *context)
{
    CO_401_device_RTT_t *runtime = (CO_401_device_RTT_t *)context;

    (void)app;
    return runtime->workerThread != RT_NULL ? rt_thread_startup(runtime->workerThread) : -RT_EINVAL;
}

static void onTick(CANopenNodeRTT *app, void *context)
{
    CO_401_device_RTT_t *runtime = (CO_401_device_RTT_t *)context;

    (void)app;
    /* Timer ticks request processing of the latest process image; historical tick count is intentionally coalesced. */
    if (runtime->semInitialized == RT_TRUE && rt_atomic_load(&runtime->wakePending) == 0) {
        rt_atomic_store(&runtime->wakePending, 1);
        if (rt_sem_release(&runtime->cia401Sem) != RT_EOK) {
            rt_atomic_store(&runtime->wakePending, 0);
        }
    }
}

/** Drain the coalesced wake while the realtime timer is stopped so old work cannot reach the new generation. */
static void onResetWakeups(CANopenNodeRTT *app, void *context)
{
    CO_401_device_RTT_t *runtime = (CO_401_device_RTT_t *)context;

    (void)app;
    if (runtime->semInitialized == RT_TRUE) {
        (void)rt_sem_control(&runtime->cia401Sem, RT_IPC_CMD_RESET, RT_NULL);
        rt_atomic_store(&runtime->wakePending, 0);
    }
}

static void onRuntimeDeinit(CANopenNodeRTT *app, void *context)
{
    CO_401_device_RTT_t *runtime = (CO_401_device_RTT_t *)context;

    runtime->communicationReady = RT_FALSE;
    onResetWakeups(app, context);
    if (runtime->workerThread != RT_NULL) {
        (void)rt_thread_delete(runtime->workerThread);
        runtime->workerThread = RT_NULL;
    }
    if (runtime->semInitialized == RT_TRUE) {
        (void)rt_sem_detach(&runtime->cia401Sem);
        runtime->semInitialized = RT_FALSE;
    }
    (void)memset(&runtime->device, 0, sizeof(runtime->device));
    runtime->deviceInitialized = RT_FALSE;
}

static const CO_RTT_lifecycle_ops_t lifecycleOps = {
    .runtimeInit = onRuntimeInit,
    .runtimeStart = onRuntimeStart,
    .communicationStop = onStop,
    .communicationQuiesced = onQuiesced,
    .communicationBind = onBind,
    .communicationReady = onReady,
    .realtimeTick = onTick,
    .resetWakeups = onResetWakeups,
    .runtimeDeinit = onRuntimeDeinit,
    .nmtStateChanged = onNmtStateChanged,
};

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
static bool requiresOutputFaultClassifier(const CO_401_device_RTT_config_t *config)
{
    bool required = false;

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    required = config->device.digitalOutputBanks != 0U;
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE */
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    required = required || config->device.analogOutputChannels != 0U;
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
    return required;
}
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */

static rt_err_t attachImpl(CANopenNodeRTT *app, CO_401_device_RTT_t *runtime,
                           const CO_401_device_RTT_config_t *config,
                           CO_RTT_lifecycle_context_release_t release)
{
    rt_err_t ret;

    if (app == NULL || runtime == NULL || config == NULL || config->device.io == NULL) {
        return -RT_EINVAL;
    }
    if (runtime->attached == RT_TRUE || app->mainThread != RT_NULL || app->rtThread != RT_NULL
        || app->rtTimer != RT_NULL || app->canOpenStack != NULL) {
        return -RT_EBUSY;
    }
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    if (requiresOutputFaultClassifier(config) && config->outputSupervisionFaultActive == NULL) {
        /* Peer identity must be available before lifecycle registration; the aggregate Heartbeat bit is ambiguous. */
        return -RT_EINVAL;
    }
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */

    (void)memset(runtime, 0, sizeof(*runtime));
    runtime->app = app;
    runtime->config = *config;
    ret = CO_RTT_lifecycleRegisterEx(app, &lifecycleOps, runtime, release);
    if (ret != RT_EOK) {
        (void)memset(runtime, 0, sizeof(*runtime));
        return ret;
    }
    runtime->attached = RT_TRUE;
    return RT_EOK;
}

rt_err_t CO_401_device_RTT_attach(CANopenNodeRTT *app, CO_401_device_RTT_t *runtime,
                                  const CO_401_device_RTT_config_t *config)
{
    return attachImpl(app, runtime, config, NULL);
}

#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_AUTOSTART)
static void releaseAuto(void *context)
{
    rt_free(context);
}

rt_err_t CO_401_device_RTT_autoAttach(CANopenNodeRTT *app, const CO_401_device_RTT_config_t *config)
{
    CO_401_device_RTT_t *runtime;
    rt_err_t ret;

    if (app == NULL || config == NULL || config->device.io == NULL) {
        return -RT_EINVAL;
    }
    if (CO_RTT_lifecycleHasOps(app, &lifecycleOps) == RT_TRUE) {
        return -RT_EBUSY;
    }
    if ((config->device.digitalOutputBanks != 0U || config->device.analogOutputChannels != 0U)
        && config->outputSupervisionEstablished == NULL) {
        /* Auto-attached output runtimes are opaque, so no later explicit supervision notification is possible. */
        return -RT_EINVAL;
    }
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    if (requiresOutputFaultClassifier(config) && config->outputSupervisionFaultActive == NULL) {
        /* Reject before allocation; attachImpl repeats the shared contract for every public attach path. */
        return -RT_EINVAL;
    }
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */

    runtime = rt_calloc(1U, sizeof(*runtime));
    if (runtime == NULL) {
        return -RT_ENOMEM;
    }
    ret = attachImpl(app, runtime, config, releaseAuto);
    if (ret != RT_EOK) {
        rt_free(runtime);
    }
    return ret;
}
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_AUTOSTART */
