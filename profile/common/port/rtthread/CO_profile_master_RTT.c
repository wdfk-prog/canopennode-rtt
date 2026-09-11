/**
 * @file CO_profile_master_RTT.c
 * @brief CANopenNode RT-Thread mainline transport for remote Device Profile controllers.
 */

#include <string.h>

#include "CO_profile_master_RTT.h"
#include "CO_app_RTT.h"
#include "CO_lifecycle_RTT.h"
#include "OD.h"
#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
#include "CO_mainline_RTT.h"
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */

#if (((CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_ENABLE) == 0)
#error "CO_profile_master_RTT requires CANopenNode SDO client support"
#endif /* (((CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_ENABLE) == 0) */

/** Convert one RT-Thread admission/IPC result into the portable Controller call contract. */
static CO_profile_call_result_t portableCallResult(rt_err_t ret)
{
    CO_profile_call_result_t result = {CO_PROFILE_CALL_OK, 0};

    if (ret == RT_EOK) {
        return result;
    }
    result.backendError = (int32_t)ret;
    if (ret == -RT_EINVAL) {
        result.status = CO_PROFILE_CALL_INVALID_ARGUMENT;
    } else if (ret == -RT_EBUSY) {
        result.status = CO_PROFILE_CALL_BUSY;
    } else {
        result.status = CO_PROFILE_CALL_BACKEND_ERROR;
    }
    return result;
}

static CO_profile_call_result_t portableSdoUpload(void *context, uint8_t remoteNodeId, uint16_t index,
                                                   uint8_t subIndex, uint8_t expectedSize,
                                                   CO_profile_transfer_result_t *result)
{
    return portableCallResult(CO_profileMasterRTT_sdoUpload((CO_profile_master_RTT_t *)context, remoteNodeId,
                                                            index, subIndex, expectedSize, result));
}

static CO_profile_call_result_t portableSdoDownload(void *context, uint8_t remoteNodeId, uint16_t index,
                                                     uint8_t subIndex, const void *data, uint8_t size,
                                                     CO_profile_transfer_result_t *result)
{
    return portableCallResult(CO_profileMasterRTT_sdoDownload((CO_profile_master_RTT_t *)context, remoteNodeId,
                                                              index, subIndex, data, size, result));
}

static CO_profile_call_result_t portableNmtCommand(void *context, CO_profile_nmt_command_t command,
                                                    uint8_t nodeId, CO_profile_transfer_result_t *result)
{
    return portableCallResult(CO_profileMasterRTT_nmtCommand((CO_profile_master_RTT_t *)context,
                                                             (CO_NMT_command_t)command, nodeId, result));
}

static const CO_profile_transport_ops_t portableTransportOps = {
    .sdoUpload = portableSdoUpload,
    .sdoDownload = portableSdoDownload,
    .nmtCommand = portableNmtCommand,
};

/*
 * Non-negative admissionState values are active caller counts. Teardown adds one negative bias atomically,
 * which both closes admission and snapshots every caller that can still touch the RT-Thread IPC objects.
 */
#define CO_PROFILE_MASTER_RTT_ADMISSION_CLOSED_BIAS ((rt_atomic_t)-0x40000000)

static rt_bool_t admissionEnter(CO_profile_master_RTT_t *runtime)
{
    const rt_atomic_t previous = rt_atomic_add(&runtime->admissionState, 1);

    if (previous < 0) {
        /* Closure won the atomic race; undo the tentative count without touching any detachable IPC object. */
        (void)rt_atomic_sub(&runtime->admissionState, 1);
        return RT_FALSE;
    }
    return RT_TRUE;
}

static void admissionLeave(CO_profile_master_RTT_t *runtime)
{
    const rt_atomic_t previous = rt_atomic_sub(&runtime->admissionState, 1);

    if (previous < 0) {
        /* This caller was included in teardown's snapshot, so release exactly one drain token before detach. */
        (void)rt_sem_release(&runtime->admissionDrainSem);
    }
}

static rt_atomic_t admissionClose(CO_profile_master_RTT_t *runtime)
{
    return rt_atomic_add(&runtime->admissionState, CO_PROFILE_MASTER_RTT_ADMISSION_CLOSED_BIAS);
}

static void admissionDrain(CO_profile_master_RTT_t *runtime, rt_atomic_t admitted)
{
    while (admitted > 0) {
        if (rt_sem_take(&runtime->admissionDrainSem, RT_WAITING_FOREVER) == RT_EOK) {
            admitted--;
        }
    }
}

/** Return the active number of SDO clients for static- or multiple-OD builds. */
static uint8_t sdoClientCount(const CO_t *co)
{
#ifdef CO_MULTIPLE_OD
    return (co != NULL && co->config != NULL) ? co->config->CNT_SDO_CLI : 0U;
#else
    (void)co;
    return (uint8_t)OD_CNT_SDO_CLI;
#endif /* CO_MULTIPLE_OD */
}

/** Wake the CANopen mainline immediately when event-driven scheduling is enabled. */
static void wakeMainline(CO_profile_master_RTT_t *runtime)
{
#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
    CO_RTT_mainlineWakeup(runtime->app);
#else
    (void)runtime;
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */
}

/** Reset the public result before one operation is admitted. requestMutex must be held. */
static void clearResultLocked(CO_profile_master_RTT_t *runtime)
{
    (void)memset(&runtime->result, 0, sizeof(runtime->result));
    runtime->result.status = CO_PROFILE_TRANSFER_LOCAL_ERROR;
}

/** Publish one terminal request result. requestMutex must be held. */
static void finishLocked(CO_profile_master_RTT_t *runtime, CO_profile_transfer_status_t status,
                         uint32_t abortCode, int32_t localError)
{
    runtime->result.status = status;
    runtime->result.abortCode = abortCode;
    runtime->result.localError = localError;
    if (status != CO_PROFILE_TRANSFER_OK) {
        runtime->result.size = 0U;
        (void)memset(runtime->result.data, 0, sizeof(runtime->result.data));
    }
    runtime->phase = CO_PROFILE_MASTER_RTT_PHASE_IDLE;
    runtime->requestKind = CO_PROFILE_MASTER_RTT_REQUEST_NONE;
}

/** Cancel an admitted request without dereferencing the current CANopen generation. */
static void cancelActive(CO_profile_master_RTT_t *runtime)
{
    rt_bool_t signal = RT_FALSE;

    if (runtime == NULL || runtime->ipcInitialized != RT_TRUE) {
        return;
    }
    if (rt_mutex_take(&runtime->requestMutex, RT_WAITING_FOREVER) != RT_EOK) {
        return;
    }
    if (runtime->phase != CO_PROFILE_MASTER_RTT_PHASE_IDLE) {
        finishLocked(runtime, CO_PROFILE_TRANSFER_CANCELED, 0U, 0);
        signal = RT_TRUE;
    }
    (void)rt_mutex_release(&runtime->requestMutex);
    if (signal == RT_TRUE) {
        (void)rt_sem_release(&runtime->completionSem);
    }
}

/** Publish communication-generation pointers/readiness with the strongest currently available serialization. */
static rt_err_t publishCommunicationState(CO_profile_master_RTT_t *runtime, CO_t *co,
                                          CO_SDOclient_t *client, rt_bool_t ready)
{
    rt_err_t ret;

    if (runtime == NULL) {
        return -RT_EINVAL;
    }
    if (runtime->ipcInitialized != RT_TRUE) {
        /*
         * Initial CANopen communication bind/ready precedes lifecycle RuntimeInit(). No
         * controller call can be admitted before IPC initialization, so publishing the
         * generation directly here is race-free and preserves that established order.
         */
        runtime->co = co;
        runtime->client = client;
        runtime->communicationReady = ready;
        if (ready != RT_TRUE) {
            runtime->mainlineReady = RT_FALSE;
        }
        return RT_EOK;
    }
    ret = rt_mutex_take(&runtime->requestMutex, RT_WAITING_FOREVER);
    if (ret != RT_EOK) {
        return ret;
    }
    runtime->co = co;
    runtime->client = client;
    runtime->communicationReady = ready;
    if (ready != RT_TRUE) {
        runtime->mainlineReady = RT_FALSE;
    }
    (void)rt_mutex_release(&runtime->requestMutex);
    return RT_EOK;
}

/** Map one CANopenNode SDO terminal error into the stack-neutral transfer contract. */
static void finishSdoErrorLocked(CO_profile_master_RTT_t *runtime, CO_SDO_return_t sdoRet,
                                 CO_SDO_abortCode_t abortCode)
{
    if (abortCode == CO_SDO_AB_TIMEOUT) {
        finishLocked(runtime, CO_PROFILE_TRANSFER_TIMEOUT, (uint32_t)abortCode, 0);
    } else if (abortCode != CO_SDO_AB_NONE) {
        finishLocked(runtime, CO_PROFILE_TRANSFER_ABORT, (uint32_t)abortCode, 0);
    } else {
        finishLocked(runtime, CO_PROFILE_TRANSFER_LOCAL_ERROR, 0U, (int32_t)sdoRet);
    }
}

/** Drain all currently available upload bytes while retaining only the bounded scalar payload. */
static void drainUploadLocked(CO_profile_master_RTT_t *runtime)
{
    uint8_t buffer[CO_PROFILE_CONTROLLER_VALUE_SIZE_MAX];
    size_t readSize;

    do {
        size_t i;

        readSize = CO_SDOclientUploadBufRead(runtime->client, buffer, sizeof(buffer));
        for (i = 0U; i < readSize; i++) {
            if (runtime->uploadSize < runtime->requestSize) {
                runtime->uploadData[runtime->uploadSize++] = buffer[i];
            } else {
                runtime->uploadOverflow = RT_TRUE;
            }
        }
    } while (readSize == sizeof(buffer));
}

/** Start the pending request on the current CANopen generation. requestMutex must be held. */
static rt_bool_t startPendingLocked(CO_profile_master_RTT_t *runtime)
{
    CO_SDO_return_t sdoRet;

    if (runtime->requestKind == CO_PROFILE_MASTER_RTT_REQUEST_NMT) {
#if (((CO_CONFIG_NMT) & CO_CONFIG_NMT_MASTER) != 0)
        CO_ReturnError_t err;

        if (runtime->co == NULL || runtime->co->NMT == NULL) {
            finishLocked(runtime, CO_PROFILE_TRANSFER_CANCELED, 0U, 0);
            return RT_TRUE;
        }
        err = CO_NMT_sendCommand(runtime->co->NMT, runtime->nmtCommand, runtime->remoteNodeId);
        if (err == CO_ERROR_NO) {
            finishLocked(runtime, CO_PROFILE_TRANSFER_OK, 0U, 0);
        } else {
            finishLocked(runtime, CO_PROFILE_TRANSFER_LOCAL_ERROR, 0U, (int32_t)err);
        }
#else
        finishLocked(runtime, CO_PROFILE_TRANSFER_LOCAL_ERROR, 0U, (int32_t)-RT_ENOSYS);
#endif /* (((CO_CONFIG_NMT) & CO_CONFIG_NMT_MASTER) != 0) */
        return RT_TRUE;
    }

    if (runtime->client == NULL || runtime->co == NULL) {
        finishLocked(runtime, CO_PROFILE_TRANSFER_CANCELED, 0U, 0);
        return RT_TRUE;
    }

    sdoRet = CO_SDOclient_setup(runtime->client, (uint32_t)CO_CAN_ID_SDO_CLI + runtime->remoteNodeId,
                                (uint32_t)CO_CAN_ID_SDO_SRV + runtime->remoteNodeId, runtime->remoteNodeId);
    if (sdoRet != CO_SDO_RT_ok_communicationEnd) {
        finishLocked(runtime, CO_PROFILE_TRANSFER_LOCAL_ERROR, 0U, (int32_t)sdoRet);
        return RT_TRUE;
    }

    if (runtime->requestKind == CO_PROFILE_MASTER_RTT_REQUEST_UPLOAD) {
        sdoRet = CO_SDOclientUploadInitiate(runtime->client, runtime->index, runtime->subIndex,
                                            runtime->config.sdoTimeoutMs, false);
        if (sdoRet != CO_SDO_RT_ok_communicationEnd) {
            CO_SDOclientClose(runtime->client);
            finishLocked(runtime, CO_PROFILE_TRANSFER_LOCAL_ERROR, 0U, (int32_t)sdoRet);
            return RT_TRUE;
        }
        runtime->uploadSize = 0U;
        runtime->uploadOverflow = RT_FALSE;
        (void)memset(runtime->uploadData, 0, sizeof(runtime->uploadData));
        runtime->phase = CO_PROFILE_MASTER_RTT_PHASE_UPLOAD;
        return RT_FALSE;
    }

    sdoRet = CO_SDOclientDownloadInitiate(runtime->client, runtime->index, runtime->subIndex,
                                          runtime->requestSize, runtime->config.sdoTimeoutMs, false);
    if (sdoRet != CO_SDO_RT_ok_communicationEnd) {
        CO_SDOclientClose(runtime->client);
        finishLocked(runtime, CO_PROFILE_TRANSFER_LOCAL_ERROR, 0U, (int32_t)sdoRet);
        return RT_TRUE;
    }
    if (CO_SDOclientDownloadBufWrite(runtime->client, runtime->requestData, runtime->requestSize)
        != runtime->requestSize) {
        CO_SDOclientClose(runtime->client);
        finishLocked(runtime, CO_PROFILE_TRANSFER_LOCAL_ERROR, 0U, (int32_t)-RT_EFULL);
        return RT_TRUE;
    }
    runtime->phase = CO_PROFILE_MASTER_RTT_PHASE_DOWNLOAD;
    return RT_FALSE;
}

/** Process one mainline-owned SDO iteration. requestMutex must be held. */
static rt_bool_t processSdoLocked(CO_profile_master_RTT_t *runtime, uint32_t dtUs, uint32_t *timerNextUs)
{
    CO_SDO_abortCode_t abortCode = CO_SDO_AB_NONE;
    CO_SDO_return_t sdoRet;

    if (runtime->phase == CO_PROFILE_MASTER_RTT_PHASE_UPLOAD) {
        size_t sizeIndicated = 0U;
        size_t sizeTransferred = 0U;

        sdoRet = CO_SDOclientUpload(runtime->client, dtUs, false, &abortCode, &sizeIndicated,
                                    &sizeTransferred, timerNextUs);
        if (sdoRet == CO_SDO_RT_uploadDataBufferFull || sdoRet == CO_SDO_RT_ok_communicationEnd) {
            drainUploadLocked(runtime);
        }
        if (sdoRet < CO_SDO_RT_ok_communicationEnd) {
            CO_SDOclientClose(runtime->client);
            finishSdoErrorLocked(runtime, sdoRet, abortCode);
            return RT_TRUE;
        }
        if (sdoRet == CO_SDO_RT_ok_communicationEnd) {
            CO_SDOclientClose(runtime->client);
            if (runtime->uploadOverflow == RT_TRUE || runtime->uploadSize != runtime->requestSize
                || sizeTransferred != runtime->requestSize
                || (sizeIndicated != 0U && sizeIndicated != runtime->requestSize)) {
                finishLocked(runtime, CO_PROFILE_TRANSFER_LOCAL_ERROR, 0U, (int32_t)-RT_EINVAL);
            } else {
                runtime->result.size = runtime->uploadSize;
                (void)memcpy(runtime->result.data, runtime->uploadData, runtime->uploadSize);
                finishLocked(runtime, CO_PROFILE_TRANSFER_OK, 0U, 0);
            }
            return RT_TRUE;
        }
        return RT_FALSE;
    }

    if (runtime->phase == CO_PROFILE_MASTER_RTT_PHASE_DOWNLOAD) {
        size_t sizeTransferred = 0U;

        sdoRet = CO_SDOclientDownload(runtime->client, dtUs, false, false, &abortCode,
                                      &sizeTransferred, timerNextUs);
        if (sdoRet < CO_SDO_RT_ok_communicationEnd) {
            CO_SDOclientClose(runtime->client);
            finishSdoErrorLocked(runtime, sdoRet, abortCode);
            return RT_TRUE;
        }
        if (sdoRet == CO_SDO_RT_ok_communicationEnd) {
            CO_SDOclientClose(runtime->client);
            if (sizeTransferred != runtime->requestSize) {
                finishLocked(runtime, CO_PROFILE_TRANSFER_LOCAL_ERROR, 0U, (int32_t)-RT_EINVAL);
            } else {
                runtime->result.size = 0U;
                finishLocked(runtime, CO_PROFILE_TRANSFER_OK, 0U, 0);
            }
            return RT_TRUE;
        }
    }
    return RT_FALSE;
}

/** Execute one transport pass in the CANopen mainline thread. */
static void onMainlineProcess(CANopenNodeRTT *app, void *context, uint32_t dtUs,
                              CO_NMT_reset_cmd_t resetStatus, uint32_t *timerNextUs)
{
    CO_profile_master_RTT_t *runtime = (CO_profile_master_RTT_t *)context;
    rt_bool_t signal = RT_FALSE;
    rt_bool_t sdoStarted = RT_FALSE;

    (void)app;
    if (runtime == NULL || runtime->ipcInitialized != RT_TRUE) {
        return;
    }
    /* co_main must never wait behind an application thread; a new caller wakes the next pass after publication. */
    if (rt_mutex_take(&runtime->requestMutex, 0) != RT_EOK) {
        return;
    }

    if (resetStatus != CO_RESET_NOT) {
        runtime->mainlineReady = RT_FALSE;
        if (runtime->phase != CO_PROFILE_MASTER_RTT_PHASE_IDLE) {
            if ((runtime->phase == CO_PROFILE_MASTER_RTT_PHASE_UPLOAD
                 || runtime->phase == CO_PROFILE_MASTER_RTT_PHASE_DOWNLOAD)
                && runtime->client != NULL) {
                CO_SDOclientClose(runtime->client);
            }
            finishLocked(runtime, CO_PROFILE_TRANSFER_CANCELED, 0U, 0);
            signal = RT_TRUE;
        }
    } else if (runtime->communicationReady == RT_TRUE) {
        /* A blocking caller is serviceable only after the actual co_main owner has executed this generation. */
        runtime->mainlineReady = RT_TRUE;
        if (runtime->phase == CO_PROFILE_MASTER_RTT_PHASE_PENDING) {
            signal = startPendingLocked(runtime);
            sdoStarted = (signal != RT_TRUE
                          && (runtime->phase == CO_PROFILE_MASTER_RTT_PHASE_UPLOAD
                              || runtime->phase == CO_PROFILE_MASTER_RTT_PHASE_DOWNLOAD));
        }
        if (runtime->phase == CO_PROFILE_MASTER_RTT_PHASE_UPLOAD
            || runtime->phase == CO_PROFILE_MASTER_RTT_PHASE_DOWNLOAD) {
            /* dtUs covers the interval before this callback; a transaction initiated here has accrued none of it. */
            signal = processSdoLocked(runtime, sdoStarted == RT_TRUE ? 0U : dtUs, timerNextUs);
        }
    }

    (void)rt_mutex_release(&runtime->requestMutex);
    if (signal == RT_TRUE) {
        (void)rt_sem_release(&runtime->completionSem);
    }
}

static rt_err_t onRuntimeInit(CANopenNodeRTT *app, void *context)
{
    CO_profile_master_RTT_t *runtime = (CO_profile_master_RTT_t *)context;
    rt_err_t ret;

    (void)app;
    if (runtime == NULL || runtime->ipcInitialized == RT_TRUE) {
        return -RT_EINVAL;
    }

    ret = rt_mutex_init(&runtime->callMutex, "pf_call", RT_IPC_FLAG_PRIO);
    if (ret != RT_EOK) {
        return ret;
    }
    ret = rt_mutex_init(&runtime->requestMutex, "pf_req", RT_IPC_FLAG_PRIO);
    if (ret != RT_EOK) {
        (void)rt_mutex_detach(&runtime->callMutex);
        return ret;
    }
    ret = rt_sem_init(&runtime->completionSem, "pf_done", 0U, RT_IPC_FLAG_FIFO);
    if (ret != RT_EOK) {
        (void)rt_mutex_detach(&runtime->requestMutex);
        (void)rt_mutex_detach(&runtime->callMutex);
        return ret;
    }
    ret = rt_sem_init(&runtime->admissionDrainSem, "pf_drain", 0U, RT_IPC_FLAG_PRIO);
    if (ret != RT_EOK) {
        (void)rt_sem_detach(&runtime->completionSem);
        (void)rt_mutex_detach(&runtime->requestMutex);
        (void)rt_mutex_detach(&runtime->callMutex);
        return ret;
    }
    rt_atomic_store(&runtime->admissionState, 0);
    runtime->ipcInitialized = RT_TRUE;
    return RT_EOK;
}

static void onCommunicationStop(CANopenNodeRTT *app, void *context)
{
    CO_profile_master_RTT_t *runtime = (CO_profile_master_RTT_t *)context;

    (void)app;
    if (runtime == NULL) {
        return;
    }
    (void)publishCommunicationState(runtime, runtime->co, runtime->client, RT_FALSE);
    cancelActive(runtime);
}

static void onCommunicationQuiesced(CANopenNodeRTT *app, void *context)
{
    CO_profile_master_RTT_t *runtime = (CO_profile_master_RTT_t *)context;

    (void)app;
    if (runtime != NULL) {
        (void)publishCommunicationState(runtime, NULL, NULL, RT_FALSE);
    }
}

static rt_err_t onCommunicationBind(CANopenNodeRTT *app, CO_t *co, OD_t *od, void *context)
{
    CO_profile_master_RTT_t *runtime = (CO_profile_master_RTT_t *)context;

    (void)app;
    (void)od;
    if (runtime == NULL || co == NULL || co->SDOclient == NULL
        || runtime->config.sdoClientIndex >= sdoClientCount(co)) {
        return -RT_EINVAL;
    }
#if (((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO) != 0)
    /* CANopenNode binds the ASCII Gateway SDO service to SDOclient[0]; never publish two owners for that client. */
    if (runtime->config.sdoClientIndex == 0U) {
        return -RT_EBUSY;
    }
#endif /* (((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO) != 0) */

    return publishCommunicationState(runtime, co, &co->SDOclient[runtime->config.sdoClientIndex], RT_FALSE);
}

static void onCommunicationReady(CANopenNodeRTT *app, void *context)
{
    CO_profile_master_RTT_t *runtime = (CO_profile_master_RTT_t *)context;

    (void)app;
    if (runtime != NULL && runtime->co != NULL && runtime->client != NULL) {
        (void)publishCommunicationState(runtime, runtime->co, runtime->client, RT_TRUE);
    }
}

static void onRuntimeDeinit(CANopenNodeRTT *app, void *context)
{
    CO_profile_master_RTT_t *runtime = (CO_profile_master_RTT_t *)context;
    rt_atomic_t admitted = 0;

    (void)app;
    if (runtime == NULL) {
        return;
    }
    if (runtime->ipcInitialized == RT_TRUE) {
        /* Close first, cancel any in-flight request, then drain every pre-close caller before IPC detach. */
        admitted = admissionClose(runtime);
        (void)publishCommunicationState(runtime, runtime->co, runtime->client, RT_FALSE);
    }
    cancelActive(runtime);

    if (runtime->ipcInitialized == RT_TRUE) {
        admissionDrain(runtime, admitted);
        (void)rt_sem_detach(&runtime->completionSem);
        (void)rt_mutex_detach(&runtime->requestMutex);
        (void)rt_mutex_detach(&runtime->callMutex);
        (void)rt_sem_detach(&runtime->admissionDrainSem);
        runtime->ipcInitialized = RT_FALSE;
    }
    runtime->co = NULL;
    runtime->client = NULL;
}

static const CO_RTT_lifecycle_ops_t lifecycleOps = {
    .runtimeInit = onRuntimeInit,
    .communicationStop = onCommunicationStop,
    .communicationQuiesced = onCommunicationQuiesced,
    .communicationBind = onCommunicationBind,
    .communicationReady = onCommunicationReady,
    .runtimeDeinit = onRuntimeDeinit,
    .mainlineProcess = onMainlineProcess,
};

rt_err_t CO_profileMasterRTT_attach(CANopenNodeRTT *app, CO_profile_master_RTT_t *runtime,
                                    const CO_profile_master_RTT_config_t *config)
{
    rt_err_t ret;

    if (app == NULL || runtime == NULL || config == NULL || config->sdoTimeoutMs == 0U) {
        return -RT_EINVAL;
    }
    if (runtime->attached == RT_TRUE || app->mainThread != RT_NULL || app->rtThread != RT_NULL
        || app->rtTimer != RT_NULL || app->canOpenStack != NULL) {
        return -RT_EBUSY;
    }
    /* One shared transport owns the app's profile SDO channel; all profile Controllers reuse that instance. */
    if (CO_RTT_lifecycleHasOps(app, &lifecycleOps) == RT_TRUE) {
        return -RT_EBUSY;
    }

    (void)memset(runtime, 0, sizeof(*runtime));
    runtime->app = app;
    runtime->config = *config;
    runtime->phase = CO_PROFILE_MASTER_RTT_PHASE_IDLE;
    runtime->requestKind = CO_PROFILE_MASTER_RTT_REQUEST_NONE;
    if (!CO_profileTransport_init(&runtime->portableTransport, runtime, &portableTransportOps)) {
        (void)memset(runtime, 0, sizeof(*runtime));
        return -RT_ERROR;
    }

    ret = CO_RTT_lifecycleRegister(app, &lifecycleOps, runtime);
    if (ret != RT_EOK) {
        (void)memset(runtime, 0, sizeof(*runtime));
        return ret;
    }
    runtime->attached = RT_TRUE;
    return RT_EOK;
}

/** Submit one serialized request and wait until the mainline publishes a terminal result. */
static rt_err_t submitAndWait(CO_profile_master_RTT_t *runtime, CO_profile_master_RTT_request_kind_t kind,
                              uint8_t remoteNodeId, uint16_t index, uint8_t subIndex, const void *data,
                              uint8_t size, CO_NMT_command_t nmtCommand, CO_profile_transfer_result_t *result)
{
    rt_err_t ret;

    if (runtime == NULL || result == NULL || runtime->attached != RT_TRUE || runtime->app == NULL) {
        return -RT_EINVAL;
    }
    if (runtime->ipcInitialized != RT_TRUE) {
        return -RT_EBUSY;
    }
    if (runtime->app->mainThread != RT_NULL && rt_thread_self() == runtime->app->mainThread) {
        return -RT_EBUSY;
    }

    if (admissionEnter(runtime) != RT_TRUE) {
        return -RT_EBUSY;
    }

    ret = rt_mutex_take(&runtime->callMutex, RT_WAITING_FOREVER);
    if (ret != RT_EOK) {
        admissionLeave(runtime);
        return ret;
    }
    ret = rt_mutex_take(&runtime->requestMutex, RT_WAITING_FOREVER);
    if (ret != RT_EOK) {
        (void)rt_mutex_release(&runtime->callMutex);
        admissionLeave(runtime);
        return ret;
    }
    if (runtime->communicationReady != RT_TRUE || runtime->mainlineReady != RT_TRUE
        || runtime->co == NULL || runtime->client == NULL
        || runtime->phase != CO_PROFILE_MASTER_RTT_PHASE_IDLE) {
        (void)rt_mutex_release(&runtime->requestMutex);
        (void)rt_mutex_release(&runtime->callMutex);
        admissionLeave(runtime);
        return -RT_EBUSY;
    }

    (void)rt_sem_control(&runtime->completionSem, RT_IPC_CMD_RESET, RT_NULL);
    clearResultLocked(runtime);
    runtime->requestKind = kind;
    runtime->phase = CO_PROFILE_MASTER_RTT_PHASE_PENDING;
    runtime->remoteNodeId = remoteNodeId;
    runtime->index = index;
    runtime->subIndex = subIndex;
    runtime->requestSize = size;
    runtime->nmtCommand = nmtCommand;
    runtime->uploadSize = 0U;
    runtime->uploadOverflow = RT_FALSE;
    (void)memset(runtime->requestData, 0, sizeof(runtime->requestData));
    if (data != NULL && size > 0U) {
        (void)memcpy(runtime->requestData, data, size);
    }
    (void)rt_mutex_release(&runtime->requestMutex);

    wakeMainline(runtime);
    do {
        ret = rt_sem_take(&runtime->completionSem, RT_WAITING_FOREVER);
    } while (ret != RT_EOK);

    ret = rt_mutex_take(&runtime->requestMutex, RT_WAITING_FOREVER);
    if (ret == RT_EOK) {
        *result = runtime->result;
        (void)rt_mutex_release(&runtime->requestMutex);
    }
    (void)rt_mutex_release(&runtime->callMutex);
    admissionLeave(runtime);
    return ret;
}

rt_err_t CO_profileMasterRTT_sdoUpload(CO_profile_master_RTT_t *runtime, uint8_t remoteNodeId,
                                       uint16_t index, uint8_t subIndex, uint8_t expectedSize,
                                       CO_profile_transfer_result_t *result)
{
    if (remoteNodeId == 0U || remoteNodeId > 127U || index == 0U || expectedSize == 0U
        || expectedSize > CO_PROFILE_CONTROLLER_VALUE_SIZE_MAX) {
        return -RT_EINVAL;
    }
    return submitAndWait(runtime, CO_PROFILE_MASTER_RTT_REQUEST_UPLOAD, remoteNodeId, index, subIndex, NULL,
                         expectedSize, (CO_NMT_command_t)0, result);
}

rt_err_t CO_profileMasterRTT_sdoDownload(CO_profile_master_RTT_t *runtime, uint8_t remoteNodeId,
                                         uint16_t index, uint8_t subIndex, const void *data, uint8_t size,
                                         CO_profile_transfer_result_t *result)
{
    if (remoteNodeId == 0U || remoteNodeId > 127U || index == 0U || data == NULL || size == 0U
        || size > CO_PROFILE_CONTROLLER_VALUE_SIZE_MAX) {
        return -RT_EINVAL;
    }
    return submitAndWait(runtime, CO_PROFILE_MASTER_RTT_REQUEST_DOWNLOAD, remoteNodeId, index, subIndex, data,
                         size, (CO_NMT_command_t)0, result);
}

/** Return true only for CiA 301 NMT command bytes which may be emitted on the network. */
static rt_bool_t nmtCommandValid(CO_NMT_command_t command)
{
    return (command == CO_NMT_ENTER_OPERATIONAL || command == CO_NMT_ENTER_STOPPED
            || command == CO_NMT_ENTER_PRE_OPERATIONAL || command == CO_NMT_RESET_NODE
            || command == CO_NMT_RESET_COMMUNICATION)
               ? RT_TRUE
               : RT_FALSE;
}

rt_err_t CO_profileMasterRTT_nmtCommand(CO_profile_master_RTT_t *runtime, CO_NMT_command_t command,
                                        uint8_t nodeId, CO_profile_transfer_result_t *result)
{
    if (nodeId > 127U || nmtCommandValid(command) != RT_TRUE) {
        return -RT_EINVAL;
    }
    return submitAndWait(runtime, CO_PROFILE_MASTER_RTT_REQUEST_NMT, nodeId, 0U, 0U, NULL, 0U, command, result);
}
CO_profile_transport_t *CO_profileMasterRTT_transport(CO_profile_master_RTT_t *runtime)
{
    if (runtime == NULL || runtime->attached != RT_TRUE || runtime->portableTransport.ops == NULL) {
        return NULL;
    }
    return &runtime->portableTransport;
}

