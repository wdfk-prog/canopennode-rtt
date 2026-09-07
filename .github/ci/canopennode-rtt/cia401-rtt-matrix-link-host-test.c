/**
 * @file cia401-rtt-matrix-link-host-test.c
 * @brief Minimal link harness for supported CiA 401 RT-Thread feature combinations.
 */

#include <stddef.h>
#include <stdlib.h>

#include "CO_401_device_RTT.h"
#include "CO_app_RTT.h"

static const CO_RTT_lifecycle_ops_t *registeredOps;

static const CO_401_io_if_t ioIf = {
    .readDigital8 = NULL,
    .writeDigital8 = NULL,
    .readAnalog16 = NULL,
    .writeAnalog16 = NULL,
};

rt_err_t CO_RTT_lifecycleRegisterEx(CANopenNodeRTT *app, const CO_RTT_lifecycle_ops_t *ops, void *context,
                                    CO_RTT_lifecycle_context_release_t release)
{
    (void)release;
    if (app == NULL || ops == NULL || context == NULL) {
        return -RT_EINVAL;
    }
    registeredOps = ops;
    return RT_EOK;
}

rt_err_t rt_sem_init(struct rt_semaphore *sem, const char *name, unsigned value, unsigned flag)
{
    (void)name;
    (void)flag;
    if (sem == NULL) {
        return -RT_EINVAL;
    }
    sem->value = value;
    sem->initialized = RT_TRUE;
    return RT_EOK;
}

rt_err_t rt_sem_detach(struct rt_semaphore *sem)
{
    if (sem == NULL) {
        return -RT_EINVAL;
    }
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
    return RT_EOK;
}

rt_err_t rt_mutex_release(struct rt_mutex *mutex)
{
    return mutex != NULL ? RT_EOK : -RT_EINVAL;
}

rt_thread_t rt_thread_create(const char *name, void (*entry)(void *parameter), void *parameter,
                             unsigned stackSize, unsigned priority, unsigned tick)
{
    static struct rt_thread thread;

    (void)name;
    (void)stackSize;
    (void)priority;
    (void)tick;
    thread.entry = entry;
    thread.parameter = parameter;
    return &thread;
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

#if defined(PKG_CANOPENNODE_RTT_CAN_TX_SUCCESS_OBSERVER)
void CO_RTT_CANsetTxSuccessCallback(CO_CANmodule_t *CANmodule, void *object,
                                    CO_RTT_CANtxSuccessCallback_t callback)
{
    (void)CANmodule;
    (void)object;
    (void)callback;
}
#endif /* PKG_CANOPENNODE_RTT_CAN_TX_SUCCESS_OBSERVER */

void CO_error(CO_EM_t *em, bool_t setError, const uint8_t errorBit, uint16_t errorCode, uint32_t infoCode)
{
    (void)em;
    (void)setError;
    (void)errorBit;
    (void)errorCode;
    (void)infoCode;
}

int main(void)
{
    CANopenNodeRTT app = {0};
    CO_401_device_RTT_t runtime = {0};
    CO_401_device_RTT_config_t config = {
        .device = {
            .io = &ioIf,
        },
    };

    if (CO_401_device_RTT_attach(&app, &runtime, &config) != RT_EOK) {
        return 1;
    }
    if (registeredOps == NULL || registeredOps->nmtStateChanged == NULL) {
        return 2;
    }
    return 0;
}
