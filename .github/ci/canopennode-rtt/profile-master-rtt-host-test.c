/**
 * @file profile-master-rtt-host-test.c
 * @brief Host contract checks for the CANopenNode RT-Thread profile Master transport.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "CO_profile_master_RTT.h"
#include "CO_lifecycle_RTT.h"

#define TEST_ASSERT(expr)                                                                \
    do {                                                                                 \
        if (!(expr)) {                                                                   \
            fprintf(stderr, "PROFILE_MASTER_RTT_HOST_FAIL:%s:%d:%s\n",                  \
                    __func__, __LINE__, #expr);                                           \
            return false;                                                                \
        }                                                                                \
    } while (0)

static const CO_RTT_lifecycle_ops_t *registeredOps;
static void *registeredContext;
static CANopenNodeRTT *registeredApp;
static struct rt_thread mainThreadObject = {.id = 1U};
static struct rt_thread callerThreadObject = {.id = 2U};
static rt_thread_t currentThread = &callerThreadObject;
static CO_NMT_reset_cmd_t ownerResetStatus = CO_RESET_NOT;
static unsigned resetAfterOwnerPass;
static unsigned ownerPassCount;
static uint32_t ownerPassDtUs = 1000U;

static uint8_t fakeUploadData[16];
static size_t fakeUploadSize;
static size_t fakeUploadCursor;
static size_t fakeUploadIndicated;
static CO_SDO_abortCode_t fakeUploadAbort;
static unsigned fakeUploadWaitPasses;
static unsigned fakeUploadCalls;
static uint32_t fakeUploadDtUs[16];
static uint8_t fakeDownloadData[16];
static size_t fakeDownloadSize;
static CO_SDO_abortCode_t fakeDownloadAbort;
static unsigned fakeDownloadWaitPasses;
static unsigned fakeDownloadCalls;
static uint32_t fakeDownloadDtUs[16];
static uint8_t fakeSetupNode;
static uint16_t fakeIndex;
static uint8_t fakeSubIndex;
static uint16_t fakeTimeoutMs;
static unsigned fakeCloseCalls;
static unsigned fakeNmtCalls;
static CO_NMT_command_t fakeNmtCommand;
static uint8_t fakeNmtNode;
static unsigned fakeMutexInitCalls;
static unsigned fakeMutexInitFailAt;
static unsigned fakeMutexDetachCalls;
static unsigned fakeMutexTakeCalls;
static unsigned fakeSemInitCalls;
static unsigned fakeSemInitFailAt;
static unsigned fakeSemDetachCalls;
static unsigned fakeAdmissionDrainWaitCalls;
static CO_profile_master_RTT_t *fakeAdmissionDrainRuntime;
static bool fakeDetachBeforeAdmissionDrain;

static void resetFakeTransfer(void)
{
    (void)memset(fakeUploadData, 0, sizeof(fakeUploadData));
    fakeUploadSize = 0U;
    fakeUploadCursor = 0U;
    fakeUploadIndicated = 0U;
    fakeUploadAbort = CO_SDO_AB_NONE;
    fakeUploadWaitPasses = 0U;
    fakeUploadCalls = 0U;
    (void)memset(fakeUploadDtUs, 0, sizeof(fakeUploadDtUs));
    (void)memset(fakeDownloadData, 0, sizeof(fakeDownloadData));
    fakeDownloadSize = 0U;
    fakeDownloadAbort = CO_SDO_AB_NONE;
    fakeDownloadWaitPasses = 0U;
    fakeDownloadCalls = 0U;
    (void)memset(fakeDownloadDtUs, 0, sizeof(fakeDownloadDtUs));
    fakeSetupNode = 0U;
    fakeIndex = 0U;
    fakeSubIndex = 0U;
    fakeTimeoutMs = 0U;
    fakeCloseCalls = 0U;
    fakeNmtCalls = 0U;
    fakeNmtCommand = (CO_NMT_command_t)0;
    fakeNmtNode = 0U;
    fakeMutexInitCalls = 0U;
    fakeMutexInitFailAt = 0U;
    fakeMutexDetachCalls = 0U;
    fakeMutexTakeCalls = 0U;
    fakeSemInitCalls = 0U;
    fakeSemInitFailAt = 0U;
    fakeSemDetachCalls = 0U;
    fakeAdmissionDrainWaitCalls = 0U;
    fakeAdmissionDrainRuntime = NULL;
    fakeDetachBeforeAdmissionDrain = false;
    ownerResetStatus = CO_RESET_NOT;
    resetAfterOwnerPass = 0U;
    ownerPassCount = 0U;
    ownerPassDtUs = 1000U;
}

rt_err_t CO_RTT_lifecycleRegister(CANopenNodeRTT *app, const CO_RTT_lifecycle_ops_t *ops, void *context)
{
    registeredApp = app;
    registeredOps = ops;
    registeredContext = context;
    return RT_EOK;
}

rt_bool_t CO_RTT_lifecycleHasOps(const CANopenNodeRTT *app, const CO_RTT_lifecycle_ops_t *ops)
{
    return (registeredApp == app && registeredOps == ops) ? RT_TRUE : RT_FALSE;
}

static void resetLifecycleRegistration(void)
{
    registeredOps = NULL;
    registeredContext = NULL;
    registeredApp = NULL;
}

void CO_RTT_mainlineWakeup(CANopenNodeRTT *app)
{
    (void)app;
}

rt_err_t rt_mutex_init(struct rt_mutex *mutex, const char *name, unsigned flag)
{
    (void)name;
    (void)flag;
    fakeMutexInitCalls++;
    if (fakeMutexInitFailAt != 0U && fakeMutexInitCalls == fakeMutexInitFailAt) {
        return -RT_ERROR;
    }
    mutex->initialized = RT_TRUE;
    mutex->locked = RT_FALSE;
    return RT_EOK;
}

rt_err_t rt_mutex_detach(struct rt_mutex *mutex)
{
    fakeMutexDetachCalls++;
    mutex->initialized = RT_FALSE;
    mutex->locked = RT_FALSE;
    return RT_EOK;
}

rt_err_t rt_mutex_take(struct rt_mutex *mutex, int timeout)
{
    fakeMutexTakeCalls++;
    if (mutex->initialized != RT_TRUE) {
        return -RT_EINVAL;
    }
    if (mutex->locked == RT_TRUE) {
        return timeout == 0 ? -RT_EBUSY : -RT_ERROR;
    }
    mutex->locked = RT_TRUE;
    return RT_EOK;
}

rt_err_t rt_mutex_release(struct rt_mutex *mutex)
{
    if (mutex->initialized != RT_TRUE || mutex->locked != RT_TRUE) {
        return -RT_EINVAL;
    }
    mutex->locked = RT_FALSE;
    return RT_EOK;
}

rt_err_t rt_sem_init(struct rt_semaphore *sem, const char *name, unsigned value, unsigned flag)
{
    (void)name;
    (void)flag;
    fakeSemInitCalls++;
    if (fakeSemInitFailAt != 0U && fakeSemInitCalls == fakeSemInitFailAt) {
        return -RT_ERROR;
    }
    sem->initialized = RT_TRUE;
    sem->value = value;
    return RT_EOK;
}

rt_err_t rt_sem_detach(struct rt_semaphore *sem)
{
    fakeSemDetachCalls++;
    sem->initialized = RT_FALSE;
    sem->value = 0U;
    return RT_EOK;
}

static void runOwnerPass(void)
{
    uint32_t timerNextUs = UINT32_MAX;
    rt_thread_t previous = currentThread;

    currentThread = &mainThreadObject;
    ownerPassCount++;
    if (resetAfterOwnerPass != 0U && ownerPassCount > resetAfterOwnerPass) {
        ownerResetStatus = CO_RESET_COMM;
    }
    registeredOps->mainlineProcess(registeredApp, registeredContext, ownerPassDtUs, ownerResetStatus, &timerNextUs);
    currentThread = previous;
}

rt_err_t rt_sem_take(struct rt_semaphore *sem, int timeout)
{
    unsigned guard = 0U;

    (void)timeout;
    if (sem->initialized != RT_TRUE) {
        return -RT_EINVAL;
    }
    if (fakeAdmissionDrainRuntime != NULL
        && sem == &fakeAdmissionDrainRuntime->admissionDrainSem && sem->value == 0U) {
        fakeAdmissionDrainWaitCalls++;
        if (fakeMutexDetachCalls != 0U || fakeSemDetachCalls != 0U
            || rt_atomic_load(&fakeAdmissionDrainRuntime->admissionState) >= 0) {
            fakeDetachBeforeAdmissionDrain = true;
        }
        (void)rt_atomic_sub(&fakeAdmissionDrainRuntime->admissionState, 1);
        sem->value++;
        fakeAdmissionDrainRuntime = NULL;
    }
    while (sem->value == 0U && guard++ < 16U) {
        runOwnerPass();
    }
    if (sem->value == 0U) {
        return -RT_ERROR;
    }
    sem->value--;
    return RT_EOK;
}

rt_err_t rt_sem_release(struct rt_semaphore *sem)
{
    if (sem->initialized != RT_TRUE) {
        return -RT_EINVAL;
    }
    sem->value++;
    return RT_EOK;
}

rt_err_t rt_sem_control(struct rt_semaphore *sem, int cmd, void *arg)
{
    (void)arg;
    if (sem->initialized != RT_TRUE || cmd != RT_IPC_CMD_RESET) {
        return -RT_EINVAL;
    }
    sem->value = 0U;
    return RT_EOK;
}

rt_thread_t rt_thread_self(void)
{
    return currentThread;
}


CO_SDO_return_t CO_SDOclient_setup(CO_SDOclient_t *client, uint32_t clientToServer,
                                   uint32_t serverToClient, uint8_t nodeId)
{
    (void)client;
    TEST_ASSERT(clientToServer == (uint32_t)CO_CAN_ID_SDO_CLI + nodeId);
    TEST_ASSERT(serverToClient == (uint32_t)CO_CAN_ID_SDO_SRV + nodeId);
    fakeSetupNode = nodeId;
    return CO_SDO_RT_ok_communicationEnd;
}

CO_SDO_return_t CO_SDOclientUploadInitiate(CO_SDOclient_t *client, uint16_t index, uint8_t subIndex,
                                           uint16_t timeoutMs, bool_t blockEnable)
{
    (void)client;
    (void)blockEnable;
    fakeIndex = index;
    fakeSubIndex = subIndex;
    fakeTimeoutMs = timeoutMs;
    fakeUploadCursor = 0U;
    return CO_SDO_RT_ok_communicationEnd;
}

CO_SDO_return_t CO_SDOclientUpload(CO_SDOclient_t *client, uint32_t dtUs, bool_t sendAbort,
                                   CO_SDO_abortCode_t *abortCode, size_t *sizeIndicated,
                                   size_t *sizeTransferred, uint32_t *timerNextUs)
{
    (void)client;
    (void)sendAbort;
    (void)timerNextUs;
    if (fakeUploadCalls < (sizeof(fakeUploadDtUs) / sizeof(fakeUploadDtUs[0]))) {
        fakeUploadDtUs[fakeUploadCalls] = dtUs;
    }
    fakeUploadCalls++;
    if (fakeUploadAbort != CO_SDO_AB_NONE) {
        *abortCode = fakeUploadAbort;
        return CO_SDO_RT_endedWithServerAbort;
    }
    if (fakeUploadCalls <= fakeUploadWaitPasses) {
        return CO_SDO_RT_waitingResponse;
    }
    *abortCode = CO_SDO_AB_NONE;
    *sizeIndicated = fakeUploadIndicated;
    *sizeTransferred = fakeUploadSize;
    return CO_SDO_RT_ok_communicationEnd;
}

size_t CO_SDOclientUploadBufRead(CO_SDOclient_t *client, uint8_t *buffer, size_t count)
{
    size_t remaining;
    size_t copy;

    (void)client;
    remaining = fakeUploadSize - fakeUploadCursor;
    copy = remaining < count ? remaining : count;
    if (copy > 0U) {
        (void)memcpy(buffer, &fakeUploadData[fakeUploadCursor], copy);
        fakeUploadCursor += copy;
    }
    return copy;
}

CO_SDO_return_t CO_SDOclientDownloadInitiate(CO_SDOclient_t *client, uint16_t index, uint8_t subIndex,
                                             size_t size, uint16_t timeoutMs, bool_t blockEnable)
{
    (void)client;
    (void)blockEnable;
    fakeIndex = index;
    fakeSubIndex = subIndex;
    fakeTimeoutMs = timeoutMs;
    fakeDownloadSize = size;
    return CO_SDO_RT_ok_communicationEnd;
}

size_t CO_SDOclientDownloadBufWrite(CO_SDOclient_t *client, const uint8_t *data, size_t count)
{
    (void)client;
    (void)memcpy(fakeDownloadData, data, count);
    fakeDownloadSize = count;
    return count;
}

CO_SDO_return_t CO_SDOclientDownload(CO_SDOclient_t *client, uint32_t dtUs, bool_t sendAbort,
                                     bool_t bufferPartial, CO_SDO_abortCode_t *abortCode,
                                     size_t *sizeTransferred, uint32_t *timerNextUs)
{
    (void)client;
    (void)sendAbort;
    (void)bufferPartial;
    (void)timerNextUs;
    if (fakeDownloadCalls < (sizeof(fakeDownloadDtUs) / sizeof(fakeDownloadDtUs[0]))) {
        fakeDownloadDtUs[fakeDownloadCalls] = dtUs;
    }
    fakeDownloadCalls++;
    if (fakeDownloadAbort != CO_SDO_AB_NONE) {
        *abortCode = fakeDownloadAbort;
        return CO_SDO_RT_endedWithServerAbort;
    }
    if (fakeDownloadCalls <= fakeDownloadWaitPasses) {
        return CO_SDO_RT_waitingResponse;
    }
    *abortCode = CO_SDO_AB_NONE;
    *sizeTransferred = fakeDownloadSize;
    return CO_SDO_RT_ok_communicationEnd;
}

void CO_SDOclientClose(CO_SDOclient_t *client)
{
    (void)client;
    fakeCloseCalls++;
}

CO_ReturnError_t CO_NMT_sendCommand(CO_NMT_t *nmt, CO_NMT_command_t command, uint8_t nodeId)
{
    (void)nmt;
    fakeNmtCalls++;
    fakeNmtCommand = command;
    fakeNmtNode = nodeId;
    return CO_ERROR_NO;
}

#if (((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO) != 0)
#define PROFILE_MASTER_TEST_DEFAULT_SDO_CLIENT_INDEX 1U
#define PROFILE_MASTER_TEST_INVALID_SDO_CLIENT_INDEX 2U
#else
#define PROFILE_MASTER_TEST_DEFAULT_SDO_CLIENT_INDEX 0U
#define PROFILE_MASTER_TEST_INVALID_SDO_CLIENT_INDEX 1U
#endif /* (((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO) != 0) */

static bool setupTransportInternal(CO_profile_master_RTT_t *runtime, CANopenNodeRTT *app, CO_t *co,
                                   CO_SDOclient_t *client, CO_NMT_t *nmt, bool serviceMainline)
{
    CO_profile_master_RTT_config_t config = {
        .sdoClientIndex = PROFILE_MASTER_TEST_DEFAULT_SDO_CLIENT_INDEX,
        .sdoTimeoutMs = 250U,
    };
    static OD_t od;

    (void)memset(runtime, 0, sizeof(*runtime));
    (void)memset(app, 0, sizeof(*app));
    (void)memset(co, 0, sizeof(*co));
    resetLifecycleRegistration();
    co->SDOclient = client;
    co->NMT = nmt;

    TEST_ASSERT(CO_profileMasterRTT_attach(app, runtime, &config) == RT_EOK);
    TEST_ASSERT(CO_profileMasterRTT_transport(runtime) == &runtime->portableTransport);
    TEST_ASSERT(runtime->portableTransport.context == runtime && runtime->portableTransport.ops != NULL);
    TEST_ASSERT(registeredOps != NULL && registeredContext == runtime && registeredApp == app);
    app->canOpenStack = co;
    /* Match canopen_app_rtt_init(): bind/ready, create co_main, RuntimeInit(), then start co_main last. */
    TEST_ASSERT(registeredOps->communicationBind(app, co, &od, runtime) == RT_EOK);
    registeredOps->communicationReady(app, runtime);
    TEST_ASSERT(runtime->co == co && runtime->client == &co->SDOclient[config.sdoClientIndex]);
    TEST_ASSERT(runtime->communicationReady == RT_TRUE && runtime->ipcInitialized == RT_FALSE);
    app->mainThread = &mainThreadObject;
    currentThread = &callerThreadObject;
    TEST_ASSERT(registeredOps->runtimeInit(app, runtime) == RT_EOK);
    TEST_ASSERT(runtime->co == co && runtime->client == &co->SDOclient[config.sdoClientIndex]);
    TEST_ASSERT(runtime->communicationReady == RT_TRUE && runtime->mainlineReady == RT_FALSE);
    if (serviceMainline) {
        runOwnerPass();
        TEST_ASSERT(runtime->mainlineReady == RT_TRUE);
    }
    return true;
}

static bool setupTransport(CO_profile_master_RTT_t *runtime, CANopenNodeRTT *app, CO_t *co,
                           CO_SDOclient_t *client, CO_NMT_t *nmt)
{
    return setupTransportInternal(runtime, app, co, client, nmt, true);
}

static void teardownTransport(CO_profile_master_RTT_t *runtime, CANopenNodeRTT *app)
{
    registeredOps->communicationStop(app, runtime);
    registeredOps->communicationQuiesced(app, runtime);
    registeredOps->runtimeDeinit(app, runtime);
    app->mainThread = RT_NULL;
    app->canOpenStack = NULL;
}

static bool test_startup_and_teardown_windows_reject_without_mainline_service(void)
{
    CO_profile_master_RTT_t runtime;
    CANopenNodeRTT app;
    CO_t co;
    CO_SDOclient_t client;
    CO_NMT_t nmt;
    CO_profile_transfer_result_t result;

    resetFakeTransfer();
    TEST_ASSERT(setupTransportInternal(&runtime, &app, &co, &client, &nmt, false));
    TEST_ASSERT(runtime.mainlineReady == RT_FALSE);
    TEST_ASSERT(CO_profileMasterRTT_sdoUpload(&runtime, 7U, 0x6041U, 0U, 2U, &result) == -RT_EBUSY);
    TEST_ASSERT(runtime.phase == CO_PROFILE_MASTER_RTT_PHASE_IDLE);
    TEST_ASSERT(runtime.requestKind == CO_PROFILE_MASTER_RTT_REQUEST_NONE);
    TEST_ASSERT(fakeUploadCalls == 0U);

    runOwnerPass();
    TEST_ASSERT(runtime.mainlineReady == RT_TRUE);
    fakeUploadData[0] = 0x34U;
    fakeUploadData[1] = 0x12U;
    fakeUploadSize = 2U;
    fakeUploadIndicated = 2U;
    TEST_ASSERT(CO_profileMasterRTT_sdoUpload(&runtime, 7U, 0x6041U, 0U, 2U, &result) == RT_EOK);
    TEST_ASSERT(result.status == CO_PROFILE_TRANSFER_OK);

    registeredOps->communicationStop(&app, &runtime);
    TEST_ASSERT(runtime.communicationReady == RT_FALSE && runtime.mainlineReady == RT_FALSE);
    TEST_ASSERT(CO_profileMasterRTT_sdoUpload(&runtime, 7U, 0x6041U, 0U, 2U, &result) == -RT_EBUSY);
    TEST_ASSERT(runtime.phase == CO_PROFILE_MASTER_RTT_PHASE_IDLE);
    TEST_ASSERT(runtime.requestKind == CO_PROFILE_MASTER_RTT_REQUEST_NONE);
    registeredOps->communicationQuiesced(&app, &runtime);
    registeredOps->runtimeDeinit(&app, &runtime);
    app.mainThread = RT_NULL;
    app.canOpenStack = NULL;
    return true;
}

static bool test_upload_download_and_nmt(void)
{
    CO_profile_master_RTT_t runtime;
    CANopenNodeRTT app;
    CO_t co;
    CO_SDOclient_t client;
    CO_NMT_t nmt;
    CO_profile_transfer_result_t result;
    CO_profile_transport_t *portable;
    CO_profile_call_result_t call;
    uint8_t download[2] = {0x34U, 0x12U};

    resetFakeTransfer();
    TEST_ASSERT(setupTransport(&runtime, &app, &co, &client, &nmt));
    portable = CO_profileMasterRTT_transport(&runtime);
    TEST_ASSERT(portable != NULL);

    fakeUploadData[0] = 0x78U;
    fakeUploadData[1] = 0x56U;
    fakeUploadSize = 2U;
    fakeUploadIndicated = 2U;
    fakeUploadWaitPasses = 1U;
    call = CO_profileTransport_sdoUpload(portable, 7U, 0x6041U, 0U, 2U, &result);
    TEST_ASSERT(call.status == CO_PROFILE_CALL_OK && call.backendError == 0);
    TEST_ASSERT(result.status == CO_PROFILE_TRANSFER_OK && result.size == 2U);
    TEST_ASSERT(result.data[0] == 0x78U && result.data[1] == 0x56U);
    TEST_ASSERT(fakeSetupNode == 7U && fakeIndex == 0x6041U && fakeSubIndex == 0U);
    TEST_ASSERT(fakeTimeoutMs == 250U && fakeUploadCalls == 2U && fakeCloseCalls == 1U);

    fakeCloseCalls = 0U;
    fakeDownloadCalls = 0U;
    TEST_ASSERT(CO_profileMasterRTT_sdoDownload(&runtime, 7U, 0x6040U, 0U,
                                                 download, sizeof(download), &result) == RT_EOK);
    TEST_ASSERT(result.status == CO_PROFILE_TRANSFER_OK);
    TEST_ASSERT(fakeDownloadSize == sizeof(download));
    TEST_ASSERT(memcmp(fakeDownloadData, download, sizeof(download)) == 0);
    TEST_ASSERT(fakeCloseCalls == 1U);

    TEST_ASSERT(CO_profileMasterRTT_nmtCommand(&runtime, CO_NMT_ENTER_OPERATIONAL, 7U, &result) == RT_EOK);
    TEST_ASSERT(result.status == CO_PROFILE_TRANSFER_OK);
    TEST_ASSERT(fakeNmtCalls == 1U && fakeNmtCommand == CO_NMT_ENTER_OPERATIONAL && fakeNmtNode == 7U);
    TEST_ASSERT(CO_profileMasterRTT_nmtCommand(&runtime, (CO_NMT_command_t)0x55U, 7U, &result) == -RT_EINVAL);
    call = CO_profileTransport_nmtCommand(portable, (CO_profile_nmt_command_t)0x55U, 7U, &result);
    TEST_ASSERT(call.status == CO_PROFILE_CALL_INVALID_ARGUMENT && call.backendError == 0);
    TEST_ASSERT(fakeNmtCalls == 1U);

    teardownTransport(&runtime, &app);
    TEST_ASSERT(runtime.ipcInitialized == RT_FALSE);
    return true;
}

static bool test_sdo_elapsed_starts_at_initiate(void)
{
    CO_profile_master_RTT_t runtime;
    CANopenNodeRTT app;
    CO_t co;
    CO_SDOclient_t client;
    CO_NMT_t nmt;
    CO_profile_transfer_result_t result;
    uint8_t download[2] = {0xAAU, 0x55U};

    resetFakeTransfer();
    TEST_ASSERT(setupTransport(&runtime, &app, &co, &client, &nmt));
    /* The first callback interval predates SDO Initiate even when it exceeds the configured protocol timeout. */
    ownerPassDtUs = 300000U;
    fakeUploadData[0] = 0x34U;
    fakeUploadData[1] = 0x12U;
    fakeUploadSize = 2U;
    fakeUploadIndicated = 2U;
    fakeUploadWaitPasses = 1U;

    TEST_ASSERT(CO_profileMasterRTT_sdoUpload(&runtime, 7U, 0x6041U, 0U, 2U, &result) == RT_EOK);
    TEST_ASSERT(result.status == CO_PROFILE_TRANSFER_OK);
    TEST_ASSERT(fakeUploadCalls == 2U);
    TEST_ASSERT(fakeUploadDtUs[0] == 0U && fakeUploadDtUs[1] == 300000U);

    fakeDownloadCalls = 0U;
    (void)memset(fakeDownloadDtUs, 0, sizeof(fakeDownloadDtUs));
    fakeDownloadWaitPasses = 1U;
    TEST_ASSERT(CO_profileMasterRTT_sdoDownload(&runtime, 7U, 0x6040U, 0U, download,
                                                 sizeof(download), &result) == RT_EOK);
    TEST_ASSERT(result.status == CO_PROFILE_TRANSFER_OK);
    TEST_ASSERT(fakeDownloadCalls == 2U);
    TEST_ASSERT(fakeDownloadDtUs[0] == 0U && fakeDownloadDtUs[1] == 300000U);

    teardownTransport(&runtime, &app);
    return true;
}

static bool test_teardown_admission_drains_before_ipc_detach(void)
{
    CO_profile_master_RTT_t runtime;
    CANopenNodeRTT app;
    CO_t co;
    CO_SDOclient_t client;
    CO_NMT_t nmt;
    CO_profile_transfer_result_t result;
    unsigned mutexTakeCheckpoint;

    resetFakeTransfer();
    TEST_ASSERT(setupTransport(&runtime, &app, &co, &client, &nmt));
    registeredOps->communicationStop(&app, &runtime);
    registeredOps->communicationQuiesced(&app, &runtime);

    rt_atomic_store(&runtime.admissionState, -1);
    mutexTakeCheckpoint = fakeMutexTakeCalls;
    TEST_ASSERT(CO_profileMasterRTT_sdoUpload(&runtime, 7U, 0x6041U, 0U, 2U, &result) == -RT_EBUSY);
    TEST_ASSERT(fakeMutexTakeCalls == mutexTakeCheckpoint);

    /* Model one caller admitted before teardown but paused before it can finish using the IPC lifetime. */
    rt_atomic_store(&runtime.admissionState, 1);
    fakeAdmissionDrainRuntime = &runtime;
    registeredOps->runtimeDeinit(&app, &runtime);

    TEST_ASSERT(fakeAdmissionDrainWaitCalls == 1U);
    TEST_ASSERT(!fakeDetachBeforeAdmissionDrain);
    TEST_ASSERT(rt_atomic_load(&runtime.admissionState) < 0);
    TEST_ASSERT(runtime.ipcInitialized == RT_FALSE);
    TEST_ASSERT(fakeSemDetachCalls == 2U && fakeMutexDetachCalls == 2U);
    app.mainThread = RT_NULL;
    app.canOpenStack = NULL;
    return true;
}

static bool test_abort_timeout_and_size_contract(void)
{
    CO_profile_master_RTT_t runtime;
    CANopenNodeRTT app;
    CO_t co;
    CO_SDOclient_t client;
    CO_NMT_t nmt;
    CO_profile_transfer_result_t result;

    resetFakeTransfer();
    TEST_ASSERT(setupTransport(&runtime, &app, &co, &client, &nmt));

    fakeUploadAbort = CO_SDO_AB_NO_OBJECT;
    TEST_ASSERT(CO_profileMasterRTT_sdoUpload(&runtime, 8U, 0x6401U, 1U, 2U, &result) == RT_EOK);
    TEST_ASSERT(result.status == CO_PROFILE_TRANSFER_ABORT);
    TEST_ASSERT(result.abortCode == CO_SDO_AB_NO_OBJECT);

    resetFakeTransfer();
    fakeUploadAbort = CO_SDO_AB_TIMEOUT;
    TEST_ASSERT(CO_profileMasterRTT_sdoUpload(&runtime, 8U, 0x6401U, 1U, 2U, &result) == RT_EOK);
    TEST_ASSERT(result.status == CO_PROFILE_TRANSFER_TIMEOUT);
    TEST_ASSERT(result.abortCode == CO_SDO_AB_TIMEOUT);

    resetFakeTransfer();
    fakeUploadData[0] = 1U;
    fakeUploadData[1] = 2U;
    fakeUploadData[2] = 3U;
    fakeUploadSize = 3U;
    fakeUploadIndicated = 3U;
    TEST_ASSERT(CO_profileMasterRTT_sdoUpload(&runtime, 8U, 0x6401U, 1U, 2U, &result) == RT_EOK);
    TEST_ASSERT(result.status == CO_PROFILE_TRANSFER_LOCAL_ERROR);
    TEST_ASSERT(result.localError == -RT_EINVAL);

    teardownTransport(&runtime, &app);
    return true;
}

static bool test_reset_cancels_active_generation(void)
{
    CO_profile_master_RTT_t runtime;
    CANopenNodeRTT app;
    CO_t co;
    CO_SDOclient_t client;
    CO_NMT_t nmt;
    CO_profile_transfer_result_t result;

    resetFakeTransfer();
    TEST_ASSERT(setupTransport(&runtime, &app, &co, &client, &nmt));
    fakeUploadData[0] = 0x11U;
    fakeUploadSize = 1U;
    fakeUploadIndicated = 1U;
    fakeUploadWaitPasses = 8U;
    resetAfterOwnerPass = ownerPassCount + 1U;

    TEST_ASSERT(CO_profileMasterRTT_sdoUpload(&runtime, 11U, 0x6000U, 1U, 1U, &result) == RT_EOK);
    TEST_ASSERT(result.status == CO_PROFILE_TRANSFER_CANCELED);
    TEST_ASSERT(fakeCloseCalls == 1U);

    ownerResetStatus = CO_RESET_NOT;
    resetAfterOwnerPass = 0U;
    teardownTransport(&runtime, &app);
    return true;
}


static bool test_runtime_init_failure_rollback_and_bind_guard(void)
{
    CO_profile_master_RTT_t runtime;
    CANopenNodeRTT app;
    CO_t co;
    CO_SDOclient_t clients[2];
    CO_profile_master_RTT_config_t config = {
        .sdoClientIndex = PROFILE_MASTER_TEST_DEFAULT_SDO_CLIENT_INDEX,
        .sdoTimeoutMs = 250U,
    };
    OD_t od;

    resetFakeTransfer();
    (void)memset(&runtime, 0, sizeof(runtime));
    (void)memset(&app, 0, sizeof(app));
    resetLifecycleRegistration();
    co.SDOclient = clients;
    TEST_ASSERT(CO_profileMasterRTT_attach(&app, &runtime, &config) == RT_EOK);
    TEST_ASSERT(registeredOps->communicationBind(&app, &co, &od, &runtime) == RT_EOK);
    registeredOps->communicationReady(&app, &runtime);
    TEST_ASSERT(runtime.co == &co && runtime.client == &clients[config.sdoClientIndex] && runtime.communicationReady == RT_TRUE);
    fakeMutexInitFailAt = 2U;
    TEST_ASSERT(registeredOps->runtimeInit(&app, &runtime) == -RT_ERROR);
    TEST_ASSERT(runtime.ipcInitialized == RT_FALSE);
    TEST_ASSERT(fakeMutexDetachCalls == 1U);
    registeredOps->communicationStop(&app, &runtime);
    registeredOps->communicationQuiesced(&app, &runtime);
    TEST_ASSERT(runtime.co == NULL && runtime.client == NULL && runtime.communicationReady == RT_FALSE);

    resetFakeTransfer();
    (void)memset(&runtime, 0, sizeof(runtime));
    (void)memset(&app, 0, sizeof(app));
    resetLifecycleRegistration();
    co.SDOclient = clients;
    TEST_ASSERT(CO_profileMasterRTT_attach(&app, &runtime, &config) == RT_EOK);
    TEST_ASSERT(registeredOps->communicationBind(&app, &co, &od, &runtime) == RT_EOK);
    registeredOps->communicationReady(&app, &runtime);
    fakeSemInitFailAt = 1U;
    TEST_ASSERT(registeredOps->runtimeInit(&app, &runtime) == -RT_ERROR);
    TEST_ASSERT(runtime.ipcInitialized == RT_FALSE);
    TEST_ASSERT(fakeMutexDetachCalls == 2U);
    TEST_ASSERT(fakeSemDetachCalls == 0U);
    registeredOps->communicationStop(&app, &runtime);
    registeredOps->communicationQuiesced(&app, &runtime);
    TEST_ASSERT(runtime.co == NULL && runtime.client == NULL && runtime.communicationReady == RT_FALSE);

    resetFakeTransfer();
    (void)memset(&runtime, 0, sizeof(runtime));
    (void)memset(&app, 0, sizeof(app));
    resetLifecycleRegistration();
    co.SDOclient = clients;
    config.sdoClientIndex = PROFILE_MASTER_TEST_DEFAULT_SDO_CLIENT_INDEX;
    TEST_ASSERT(CO_profileMasterRTT_attach(&app, &runtime, &config) == RT_EOK);
    TEST_ASSERT(registeredOps->communicationBind(&app, &co, &od, &runtime) == RT_EOK);
    registeredOps->communicationReady(&app, &runtime);
    fakeSemInitFailAt = 2U;
    TEST_ASSERT(registeredOps->runtimeInit(&app, &runtime) == -RT_ERROR);
    TEST_ASSERT(runtime.ipcInitialized == RT_FALSE);
    TEST_ASSERT(fakeMutexDetachCalls == 2U);
    TEST_ASSERT(fakeSemDetachCalls == 1U);
    registeredOps->communicationStop(&app, &runtime);
    registeredOps->communicationQuiesced(&app, &runtime);
    TEST_ASSERT(runtime.co == NULL && runtime.client == NULL && runtime.communicationReady == RT_FALSE);

    resetFakeTransfer();
    (void)memset(&runtime, 0, sizeof(runtime));
    (void)memset(&app, 0, sizeof(app));
    (void)memset(&co, 0, sizeof(co));
    resetLifecycleRegistration();
    co.SDOclient = clients;
    config.sdoClientIndex = PROFILE_MASTER_TEST_INVALID_SDO_CLIENT_INDEX;
    TEST_ASSERT(CO_profileMasterRTT_attach(&app, &runtime, &config) == RT_EOK);
    TEST_ASSERT(registeredOps->communicationBind(&app, &co, &od, &runtime) == -RT_EINVAL);
    TEST_ASSERT(registeredOps->runtimeInit(&app, &runtime) == RT_EOK);
    TEST_ASSERT(runtime.client == NULL && runtime.co == NULL);
    TEST_ASSERT(runtime.communicationReady == RT_FALSE);
    registeredOps->runtimeDeinit(&app, &runtime);
    TEST_ASSERT(runtime.ipcInitialized == RT_FALSE);
    return true;
}

#if (((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO) != 0)
static bool test_gateway_sdo_client_zero_is_reserved(void)
{
    CO_profile_master_RTT_t runtime;
    CANopenNodeRTT app;
    CO_t co;
    CO_SDOclient_t clients[2];
    CO_profile_master_RTT_config_t config = {.sdoClientIndex = 0U, .sdoTimeoutMs = 250U};
    OD_t od;

    resetFakeTransfer();
    (void)memset(&runtime, 0, sizeof(runtime));
    (void)memset(&app, 0, sizeof(app));
    (void)memset(&co, 0, sizeof(co));
    resetLifecycleRegistration();
    co.SDOclient = clients;

    TEST_ASSERT(CO_profileMasterRTT_attach(&app, &runtime, &config) == RT_EOK);
    TEST_ASSERT(registeredOps->communicationBind(&app, &co, &od, &runtime) == -RT_EBUSY);
    TEST_ASSERT(registeredOps->runtimeInit(&app, &runtime) == RT_EOK);
    TEST_ASSERT(runtime.client == NULL && runtime.co == NULL);

    config.sdoClientIndex = 1U;
    runtime.config = config;
    TEST_ASSERT(registeredOps->communicationBind(&app, &co, &od, &runtime) == RT_EOK);
    TEST_ASSERT(runtime.client == &clients[1]);
    registeredOps->runtimeDeinit(&app, &runtime);
    return true;
}
#endif /* (((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO) != 0) */

static bool test_mainline_never_waits_for_request_mutex(void)
{
    CO_profile_master_RTT_t runtime;
    CANopenNodeRTT app;
    CO_t co;
    CO_SDOclient_t client;
    CO_NMT_t nmt;
    uint32_t timerNextUs = UINT32_MAX;

    resetFakeTransfer();
    TEST_ASSERT(setupTransport(&runtime, &app, &co, &client, &nmt));
    runtime.requestKind = CO_PROFILE_MASTER_RTT_REQUEST_NMT;
    runtime.phase = CO_PROFILE_MASTER_RTT_PHASE_PENDING;
    runtime.remoteNodeId = 3U;
    runtime.nmtCommand = CO_NMT_ENTER_PRE_OPERATIONAL;
    runtime.requestMutex.locked = RT_TRUE;

    registeredOps->mainlineProcess(&app, &runtime, 1000U, CO_RESET_NOT, &timerNextUs);
    TEST_ASSERT(runtime.phase == CO_PROFILE_MASTER_RTT_PHASE_PENDING);
    TEST_ASSERT(fakeNmtCalls == 0U);

    runtime.requestMutex.locked = RT_FALSE;
    registeredOps->mainlineProcess(&app, &runtime, 1000U, CO_RESET_NOT, &timerNextUs);
    TEST_ASSERT(runtime.phase == CO_PROFILE_MASTER_RTT_PHASE_IDLE);
    TEST_ASSERT(fakeNmtCalls == 1U);
    (void)rt_sem_control(&runtime.completionSem, RT_IPC_CMD_RESET, RT_NULL);

    teardownTransport(&runtime, &app);
    return true;
}

int main(void)
{
    unsigned passed = 0U;

    passed += test_startup_and_teardown_windows_reject_without_mainline_service() ? 1U : 0U;
    passed += test_upload_download_and_nmt() ? 1U : 0U;
    passed += test_sdo_elapsed_starts_at_initiate() ? 1U : 0U;
    passed += test_teardown_admission_drains_before_ipc_detach() ? 1U : 0U;
    passed += test_abort_timeout_and_size_contract() ? 1U : 0U;
    passed += test_reset_cancels_active_generation() ? 1U : 0U;
    passed += test_mainline_never_waits_for_request_mutex() ? 1U : 0U;
    passed += test_runtime_init_failure_rollback_and_bind_guard() ? 1U : 0U;
#if (((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO) != 0)
    passed += test_gateway_sdo_client_zero_is_reserved() ? 1U : 0U;
    if (passed != 9U) {
        fprintf(stderr, "PROFILE_MASTER_RTT_HOST_SUMMARY:%u/9\n", passed);
        return 1;
    }
    printf("PROFILE_MASTER_RTT_HOST_SUMMARY:9/9\n");
#else
    if (passed != 8U) {
        fprintf(stderr, "PROFILE_MASTER_RTT_HOST_SUMMARY:%u/8\n", passed);
        return 1;
    }
    printf("PROFILE_MASTER_RTT_HOST_SUMMARY:8/8\n");
#endif /* (((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO) != 0) */
    return 0;
}
