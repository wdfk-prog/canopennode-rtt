/**
 * @file cia402-rtt-scheduling-host-test.c
 * @brief Host contract checks for shared versus dedicated CiA 402 worker selection.
 */

#include <stdio.h>
#include <string.h>

#include "CO_402_device_RTT.h"
#include "CO_app_RTT.h"

#define TEST_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "CIA402_RTT_SCHED_FAIL:%s:%d:%s\n", __func__, __LINE__, #expr); \
            return 1; \
        } \
    } while (0)

static const CO_RTT_lifecycle_ops_t *registeredOps;
#if defined(PKG_CANOPENNODE_PROFILE_RTT_SHARED_WORKER)
static unsigned sharedRequestCount;
#endif /* defined(PKG_CANOPENNODE_PROFILE_RTT_SHARED_WORKER) */
static unsigned semReleaseCount;
static struct rt_thread fakeThread;

rt_err_t CO_RTT_lifecycleRegisterEx(CANopenNodeRTT *app, const CO_RTT_lifecycle_ops_t *ops, void *context,
                                    CO_RTT_lifecycle_context_release_t release)
{
    (void)release;
    if (app == NULL || ops == NULL || context == NULL || registeredOps != NULL) {
        return -RT_EBUSY;
    }
    registeredOps = ops;
    return RT_EOK;
}

#if defined(PKG_CANOPENNODE_PROFILE_RTT_SHARED_WORKER)
rt_err_t CO_RTT_lifecycleRequestDeferredProcess(CANopenNodeRTT *app)
{
    if (app == NULL) {
        return -RT_EINVAL;
    }
    sharedRequestCount++;
    return RT_EOK;
}
#endif /* defined(PKG_CANOPENNODE_PROFILE_RTT_SHARED_WORKER) */

rt_err_t rt_sem_init(struct rt_semaphore *sem, const char *name, unsigned value, unsigned flag)
{
    (void)name;
    (void)flag;
    sem->value = value;
    sem->initialized = RT_TRUE;
    return RT_EOK;
}

rt_err_t rt_sem_detach(struct rt_semaphore *sem)
{
    sem->value = 0U;
    sem->initialized = RT_FALSE;
    return RT_EOK;
}

rt_err_t rt_sem_take(struct rt_semaphore *sem, int timeout)
{
    (void)timeout;
    if (sem == NULL || sem->initialized != RT_TRUE || sem->value == 0U) {
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
    return RT_EOK;
}

rt_err_t rt_mutex_take(struct rt_mutex *mutex, int timeout)
{
    (void)timeout;
    if (mutex == NULL) {
        return -RT_EINVAL;
    }
    mutex->locked = RT_TRUE;
    return RT_EOK;
}

rt_err_t rt_mutex_release(struct rt_mutex *mutex)
{
    if (mutex == NULL) {
        return -RT_EINVAL;
    }
    mutex->locked = RT_FALSE;
    return RT_EOK;
}

rt_thread_t rt_thread_create(const char *name, void (*entry)(void *parameter), void *parameter,
                             unsigned stackSize, unsigned priority, unsigned tick)
{
    (void)name;
    (void)stackSize;
    (void)priority;
    (void)tick;
    fakeThread.entry = entry;
    fakeThread.parameter = parameter;
    return &fakeThread;
}

rt_err_t rt_thread_startup(rt_thread_t thread)
{
    return thread != RT_NULL ? RT_EOK : -RT_EINVAL;
}

rt_err_t rt_thread_delete(rt_thread_t thread)
{
    return thread != RT_NULL ? RT_EOK : -RT_EINVAL;
}

rt_thread_t rt_thread_self(void)
{
    return RT_NULL;
}

int main(void)
{
    CANopenNodeRTT app = {0};
    CO_402_device_axis_t axes[1] = {0};
    CO_402_device_axis_config_t configs[1] = {0};
    CO_402_device_RTT_t runtime = {0};
    CO_402_device_RTT_config_t config = {
        .axes = axes,
        .configs = configs,
        .axisCount = 1U,
    };

    TEST_ASSERT(CO_402_device_RTT_attach(&app, &runtime, &config) == RT_EOK);
    TEST_ASSERT(registeredOps != NULL);
#if defined(PKG_CANOPENNODE_CIA402_DEVICE_RTT_DEDICATED_WORKER)
    TEST_ASSERT(registeredOps->deferredProcess == NULL);
    TEST_ASSERT(registeredOps->realtimeTick != NULL);
    TEST_ASSERT(registeredOps->resetWakeups != NULL);
    TEST_ASSERT(CO_402_device_RTT_requestProcess(&runtime) == -RT_EBUSY);
    runtime.semInitialized = RT_TRUE;
    runtime.cia402Sem.initialized = RT_TRUE;
    TEST_ASSERT(CO_402_device_RTT_requestProcess(&runtime) == RT_EOK);
    TEST_ASSERT(semReleaseCount == 1U);
    printf("CIA402_RTT_SCHED_PASS:dedicated\n");
#else
    TEST_ASSERT(registeredOps->deferredProcess != NULL);
    TEST_ASSERT(registeredOps->realtimeTick == NULL);
    TEST_ASSERT(registeredOps->resetWakeups == NULL);
    TEST_ASSERT(CO_402_device_RTT_requestProcess(&runtime) == RT_EOK);
    TEST_ASSERT(sharedRequestCount == 1U);
    printf("CIA402_RTT_SCHED_PASS:shared\n");
#endif /* defined(PKG_CANOPENNODE_CIA402_DEVICE_RTT_DEDICATED_WORKER) */
    return 0;
}
