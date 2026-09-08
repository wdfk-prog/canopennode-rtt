/**
 * @file profile-shared-worker-host-test.c
 * @brief Host-only scheduling contract checks for the common RT-Thread Profile worker.
 */

#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "CO_app_RTT.h"

#define TEST_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "PROFILE_SHARED_WORKER_FAIL:%s:%d:%s\n", __func__, __LINE__, #expr); \
            return false; \
        } \
    } while (0)

typedef struct {
    char id;
    unsigned deferredCount;
    unsigned tickCount;
    unsigned resetCount;
    unsigned deinitCount;
} fake_profile_t;

static struct rt_thread fakeThread;
static unsigned threadCreateCount;
static unsigned threadStartCount;
static unsigned threadDeleteCount;
static unsigned semReleaseCount;
static unsigned semResetCount;
static unsigned semDetachCount;
static unsigned mutexTakeCount;
static unsigned mutexReleaseCount;
static char deferredOrder[8];
static unsigned deferredOrderCount;
static char teardownOrder[8];
static unsigned teardownOrderCount;
static jmp_buf workerExit;
static bool workerExitArmed;
static bool callbackLockObserved;
static bool failSemInitOnce;
static bool failSemReleaseOnce;
static bool failThreadCreateOnce;
static bool failThreadStartOnce;

static void resetHarness(void)
{
    (void)memset(&fakeThread, 0, sizeof(fakeThread));
    threadCreateCount = 0U;
    threadStartCount = 0U;
    threadDeleteCount = 0U;
    semReleaseCount = 0U;
    semResetCount = 0U;
    semDetachCount = 0U;
    mutexTakeCount = 0U;
    mutexReleaseCount = 0U;
    deferredOrderCount = 0U;
    teardownOrderCount = 0U;
    workerExitArmed = false;
    callbackLockObserved = true;
    failSemInitOnce = false;
    failSemReleaseOnce = false;
    failThreadCreateOnce = false;
    failThreadStartOnce = false;
}

static rt_err_t bindOk(CANopenNodeRTT *app, CO_t *co, OD_t *od, void *context)
{
    (void)app;
    (void)co;
    (void)od;
    (void)context;
    return RT_EOK;
}

static void deferred(CANopenNodeRTT *app, void *context)
{
    fake_profile_t *profile = (fake_profile_t *)context;

    if (app->lifecycleMutex.locked != RT_TRUE) {
        callbackLockObserved = false;
    }
    profile->deferredCount++;
    deferredOrder[deferredOrderCount++] = profile->id;
}

static void tick(CANopenNodeRTT *app, void *context)
{
    (void)app;
    ((fake_profile_t *)context)->tickCount++;
}

static void resetWake(CANopenNodeRTT *app, void *context)
{
    (void)app;
    ((fake_profile_t *)context)->resetCount++;
}

static void deinit(CANopenNodeRTT *app, void *context)
{
    fake_profile_t *profile = (fake_profile_t *)context;

    (void)app;
    profile->deinitCount++;
    teardownOrder[teardownOrderCount++] = profile->id;
}

static const CO_RTT_lifecycle_ops_t sharedOps = {
    .communicationBind = bindOk,
    .runtimeDeinit = deinit,
    .deferredProcess = deferred,
};

static const CO_RTT_lifecycle_ops_t dedicatedLikeOps = {
    .communicationBind = bindOk,
    .realtimeTick = tick,
    .resetWakeups = resetWake,
    .runtimeDeinit = deinit,
};

rt_err_t rt_sem_init(struct rt_semaphore *sem, const char *name, unsigned value, unsigned flag)
{
    (void)name;
    (void)flag;
    if (failSemInitOnce) {
        failSemInitOnce = false;
        return -RT_ENOMEM;
    }
    sem->value = value;
    sem->initialized = RT_TRUE;
    return RT_EOK;
}

rt_err_t rt_sem_detach(struct rt_semaphore *sem)
{
    sem->value = 0U;
    sem->initialized = RT_FALSE;
    semDetachCount++;
    return RT_EOK;
}

rt_err_t rt_sem_take(struct rt_semaphore *sem, int timeout)
{
    (void)timeout;
    if (sem == NULL || sem->initialized != RT_TRUE) {
        return -RT_ERROR;
    }
    if (sem->value == 0U) {
        if (workerExitArmed) {
            workerExitArmed = false;
            longjmp(workerExit, 1);
        }
        return -RT_ERROR;
    }
    sem->value--;
    return RT_EOK;
}

rt_err_t rt_sem_release(struct rt_semaphore *sem)
{
    if (sem == NULL || sem->initialized != RT_TRUE) {
        return -RT_ERROR;
    }
    if (failSemReleaseOnce) {
        failSemReleaseOnce = false;
        return -RT_ERROR;
    }
    sem->value++;
    semReleaseCount++;
    return RT_EOK;
}

rt_err_t rt_sem_control(struct rt_semaphore *sem, int cmd, void *arg)
{
    (void)arg;
    if (sem == NULL || sem->initialized != RT_TRUE || cmd != RT_IPC_CMD_RESET) {
        return -RT_ERROR;
    }
    sem->value = 0U;
    semResetCount++;
    return RT_EOK;
}

rt_err_t rt_mutex_init(struct rt_mutex *mutex, const char *name, unsigned flag)
{
    (void)name;
    (void)flag;
    mutex->locked = RT_FALSE;
    return RT_EOK;
}

rt_err_t rt_mutex_detach(struct rt_mutex *mutex)
{
    mutex->locked = RT_FALSE;
    return RT_EOK;
}

rt_err_t rt_mutex_take(struct rt_mutex *mutex, int timeout)
{
    (void)timeout;
    if (mutex == NULL || mutex->locked == RT_TRUE) {
        return -RT_EBUSY;
    }
    mutex->locked = RT_TRUE;
    mutexTakeCount++;
    return RT_EOK;
}

rt_err_t rt_mutex_release(struct rt_mutex *mutex)
{
    if (mutex == NULL || mutex->locked != RT_TRUE) {
        return -RT_EINVAL;
    }
    mutex->locked = RT_FALSE;
    mutexReleaseCount++;
    return RT_EOK;
}

rt_thread_t rt_thread_create(const char *name, void (*entry)(void *parameter), void *parameter,
                             unsigned stackSize, unsigned priority, unsigned tickValue)
{
    (void)stackSize;
    (void)priority;
    (void)tickValue;
    if (name == NULL || strcmp(name, "co_prof") != 0) {
        return RT_NULL;
    }
    if (failThreadCreateOnce) {
        failThreadCreateOnce = false;
        return RT_NULL;
    }
    fakeThread.entry = entry;
    fakeThread.parameter = parameter;
    threadCreateCount++;
    return &fakeThread;
}

rt_err_t rt_thread_startup(rt_thread_t thread)
{
    if (thread == RT_NULL) {
        return -RT_EINVAL;
    }
    if (failThreadStartOnce) {
        failThreadStartOnce = false;
        return -RT_ERROR;
    }
    threadStartCount++;
    return RT_EOK;
}

rt_err_t rt_thread_delete(rt_thread_t thread)
{
    if (thread == RT_NULL) {
        return -RT_EINVAL;
    }
    threadDeleteCount++;
    teardownOrder[teardownOrderCount++] = 'W';
    return RT_EOK;
}

rt_thread_t rt_thread_self(void)
{
    return RT_NULL;
}

static bool testSharedDispatchAndCoalescing(void)
{
    CANopenNodeRTT app = {0};
    CO_t co = {0};
    OD_t od = {0};
    fake_profile_t first = {.id = 'A'};
    fake_profile_t dedicated = {.id = 'D'};
    fake_profile_t second = {.id = 'B'};

    resetHarness();
    TEST_ASSERT(CO_RTT_lifecycleRequestDeferredProcess(NULL) == -RT_EINVAL);
    TEST_ASSERT(CO_RTT_lifecycleRequestDeferredProcess(&app) == -RT_EBUSY);
    TEST_ASSERT(CO_RTT_lifecycleRegister(&app, &sharedOps, &first) == RT_EOK);
    TEST_ASSERT(CO_RTT_lifecycleRegister(&app, &dedicatedLikeOps, &dedicated) == RT_EOK);
    TEST_ASSERT(CO_RTT_lifecycleRegister(&app, &sharedOps, &second) == RT_EOK);
    TEST_ASSERT(CO_RTT_lifecycleBindCommunication(&app, &co, &od) == RT_EOK);
    TEST_ASSERT(CO_RTT_lifecycleRuntimeInit(&app) == RT_EOK);
    TEST_ASSERT(threadCreateCount == 1U);
    TEST_ASSERT(app.lifecycle.sharedWorkerSemInitialized == RT_TRUE);
    TEST_ASSERT(CO_RTT_lifecycleRuntimeStart(&app) == RT_EOK);
    TEST_ASSERT(threadStartCount == 1U);

    CO_RTT_lifecycleRealtimeTick(&app);
    CO_RTT_lifecycleRealtimeTick(&app);
    TEST_ASSERT(dedicated.tickCount == 2U);
    TEST_ASSERT(semReleaseCount == 1U);
    TEST_ASSERT(app.lifecycle.sharedWorkerSem.value == 1U);

    workerExitArmed = true;
    if (setjmp(workerExit) == 0) {
        fakeThread.entry(fakeThread.parameter);
        TEST_ASSERT(false);
    }
    TEST_ASSERT(first.deferredCount == 1U);
    TEST_ASSERT(second.deferredCount == 1U);
    TEST_ASSERT(dedicated.deferredCount == 0U);
    TEST_ASSERT(deferredOrderCount == 2U);
    TEST_ASSERT(deferredOrder[0] == 'A' && deferredOrder[1] == 'B');
    TEST_ASSERT(mutexTakeCount == 1U && mutexReleaseCount == 1U);
    TEST_ASSERT(callbackLockObserved);
    TEST_ASSERT(app.lifecycleMutex.locked == RT_FALSE);

    CO_RTT_lifecycleRealtimeTick(&app);
    TEST_ASSERT(app.lifecycle.sharedWorkerSem.value == 1U);
    CO_RTT_lifecycleResetWakeups(&app);
    TEST_ASSERT(app.lifecycle.sharedWorkerSem.value == 0U);
    TEST_ASSERT(app.lifecycle.sharedWorkerWakePending == 0);
    TEST_ASSERT(dedicated.resetCount == 1U);
    TEST_ASSERT(semResetCount == 1U);

    CO_RTT_lifecycleRuntimeDeinit(&app);
    TEST_ASSERT(threadDeleteCount == 1U);
    TEST_ASSERT(teardownOrderCount == 4U);
    TEST_ASSERT(teardownOrder[0] == 'W');
    TEST_ASSERT(teardownOrder[1] == 'B');
    TEST_ASSERT(teardownOrder[2] == 'D');
    TEST_ASSERT(teardownOrder[3] == 'A');
    return true;
}

static bool testSemInitFailureIsReported(void)
{
    CANopenNodeRTT app = {0};
    CO_t co = {0};
    OD_t od = {0};
    fake_profile_t profile = {.id = 'A'};

    resetHarness();
    TEST_ASSERT(CO_RTT_lifecycleRegister(&app, &sharedOps, &profile) == RT_EOK);
    TEST_ASSERT(CO_RTT_lifecycleBindCommunication(&app, &co, &od) == RT_EOK);
    failSemInitOnce = true;
    TEST_ASSERT(CO_RTT_lifecycleRuntimeInit(&app) == -RT_ENOMEM);
    TEST_ASSERT(app.lifecycle.sharedWorkerSemInitialized == RT_FALSE);
    TEST_ASSERT(app.lifecycle.sharedWorkerThread == RT_NULL);
    TEST_ASSERT(semDetachCount == 0U);
    CO_RTT_lifecycleCommunicationQuiesced(&app);
    CO_RTT_lifecycleRuntimeDeinit(&app);
    TEST_ASSERT(profile.deinitCount == 1U);
    return true;
}

static bool testThreadCreateFailureRollsBackSemaphore(void)
{
    CANopenNodeRTT app = {0};
    CO_t co = {0};
    OD_t od = {0};
    fake_profile_t profile = {.id = 'A'};

    resetHarness();
    TEST_ASSERT(CO_RTT_lifecycleRegister(&app, &sharedOps, &profile) == RT_EOK);
    TEST_ASSERT(CO_RTT_lifecycleBindCommunication(&app, &co, &od) == RT_EOK);
    failThreadCreateOnce = true;
    TEST_ASSERT(CO_RTT_lifecycleRuntimeInit(&app) == -RT_ENOMEM);
    TEST_ASSERT(app.lifecycle.sharedWorkerSemInitialized == RT_FALSE);
    TEST_ASSERT(app.lifecycle.sharedWorkerThread == RT_NULL);
    TEST_ASSERT(semDetachCount == 1U);
    CO_RTT_lifecycleCommunicationQuiesced(&app);
    CO_RTT_lifecycleRuntimeDeinit(&app);
    TEST_ASSERT(semDetachCount == 1U);
    TEST_ASSERT(profile.deinitCount == 1U);
    return true;
}

static bool testThreadStartFailureIsReportedAndCleaned(void)
{
    CANopenNodeRTT app = {0};
    CO_t co = {0};
    OD_t od = {0};
    fake_profile_t profile = {.id = 'A'};

    resetHarness();
    TEST_ASSERT(CO_RTT_lifecycleRegister(&app, &sharedOps, &profile) == RT_EOK);
    TEST_ASSERT(CO_RTT_lifecycleBindCommunication(&app, &co, &od) == RT_EOK);
    TEST_ASSERT(CO_RTT_lifecycleRuntimeInit(&app) == RT_EOK);
    failThreadStartOnce = true;
    TEST_ASSERT(CO_RTT_lifecycleRuntimeStart(&app) == -RT_ERROR);
    TEST_ASSERT(threadStartCount == 0U);
    CO_RTT_lifecycleCommunicationQuiesced(&app);
    CO_RTT_lifecycleRuntimeDeinit(&app);
    TEST_ASSERT(threadDeleteCount == 1U);
    TEST_ASSERT(semDetachCount == 1U);
    TEST_ASSERT(profile.deinitCount == 1U);
    return true;
}

static bool testWakeReleaseFailureClearsPendingForRetry(void)
{
    CANopenNodeRTT app = {0};
    CO_t co = {0};
    OD_t od = {0};
    fake_profile_t profile = {.id = 'A'};

    resetHarness();
    TEST_ASSERT(CO_RTT_lifecycleRegister(&app, &sharedOps, &profile) == RT_EOK);
    TEST_ASSERT(CO_RTT_lifecycleBindCommunication(&app, &co, &od) == RT_EOK);
    TEST_ASSERT(CO_RTT_lifecycleRuntimeInit(&app) == RT_EOK);
    TEST_ASSERT(CO_RTT_lifecycleRuntimeStart(&app) == RT_EOK);

    failSemReleaseOnce = true;
    TEST_ASSERT(CO_RTT_lifecycleRequestDeferredProcess(&app) == -RT_ERROR);
    TEST_ASSERT(app.lifecycle.sharedWorkerWakePending == 0);
    TEST_ASSERT(app.lifecycle.sharedWorkerSem.value == 0U);
    TEST_ASSERT(semReleaseCount == 0U);
    TEST_ASSERT(CO_RTT_lifecycleRequestDeferredProcess(&app) == RT_EOK);
    TEST_ASSERT(app.lifecycle.sharedWorkerWakePending != 0);
    TEST_ASSERT(app.lifecycle.sharedWorkerSem.value == 1U);
    TEST_ASSERT(semReleaseCount == 1U);

    CO_RTT_lifecycleResetWakeups(&app);
    CO_RTT_lifecycleCommunicationQuiesced(&app);
    CO_RTT_lifecycleRuntimeDeinit(&app);
    return true;
}

static bool testNoDeferredProfileCreatesNoWorker(void)
{
    CANopenNodeRTT app = {0};
    CO_t co = {0};
    OD_t od = {0};
    fake_profile_t dedicated = {.id = 'D'};

    resetHarness();
    TEST_ASSERT(CO_RTT_lifecycleRegister(&app, &dedicatedLikeOps, &dedicated) == RT_EOK);
    TEST_ASSERT(CO_RTT_lifecycleBindCommunication(&app, &co, &od) == RT_EOK);
    TEST_ASSERT(CO_RTT_lifecycleRuntimeInit(&app) == RT_EOK);
    TEST_ASSERT(threadCreateCount == 0U);
    TEST_ASSERT(app.lifecycle.sharedWorkerThread == RT_NULL);
    TEST_ASSERT(CO_RTT_lifecycleRuntimeStart(&app) == RT_EOK);
    TEST_ASSERT(threadStartCount == 0U);
    CO_RTT_lifecycleRuntimeDeinit(&app);
    TEST_ASSERT(threadDeleteCount == 0U);
    return true;
}

int main(void)
{
    unsigned passed = 0U;

    if (testSharedDispatchAndCoalescing()) {
        passed++;
    }
    if (testSemInitFailureIsReported()) {
        passed++;
    }
    if (testThreadCreateFailureRollsBackSemaphore()) {
        passed++;
    }
    if (testThreadStartFailureIsReportedAndCleaned()) {
        passed++;
    }
    if (testWakeReleaseFailureClearsPendingForRetry()) {
        passed++;
    }
    if (testNoDeferredProfileCreatesNoWorker()) {
        passed++;
    }
    printf("PROFILE_SHARED_WORKER_PASS:%u/6\n", passed);
    return passed == 6U ? 0 : 1;
}
