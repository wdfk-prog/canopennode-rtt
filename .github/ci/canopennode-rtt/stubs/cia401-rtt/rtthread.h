#ifndef CIA401_RTT_HOST_STUB_RTTHREAD_H
#define CIA401_RTT_HOST_STUB_RTTHREAD_H

#include <stddef.h>
#include <stdint.h>
#include <rtatomic.h>

typedef int rt_err_t;
typedef int rt_bool_t;
typedef intptr_t rt_base_t;
typedef uintptr_t rt_ubase_t;
typedef uint32_t rt_uint32_t;
typedef size_t rt_size_t;
typedef ptrdiff_t rt_ssize_t;
typedef void *rt_timer_t;

struct rt_device {
    unsigned unused;
};
typedef struct rt_device *rt_device_t;

struct rt_thread {
    void (*entry)(void *parameter);
    void *parameter;
};
typedef struct rt_thread *rt_thread_t;

struct rt_semaphore {
    unsigned value;
    rt_bool_t initialized;
};

struct rt_mutex {
    rt_bool_t locked;
};

#define RT_EOK 0
#define RT_ERROR 1
#define RT_EBUSY 16
#define RT_EINVAL 22
#define RT_ENOMEM 12
#define RT_ETIMEOUT 110
#define RT_TRUE 1
#define RT_FALSE 0
#define RT_NULL NULL
#define RT_WAITING_FOREVER (-1)
#define RT_IPC_FLAG_FIFO 0
#define RT_IPC_FLAG_PRIO 1
#define RT_IPC_CMD_RESET 1
#define RT_DEVICE_FLAG_INT_RX 0x01U
#define RT_DEVICE_FLAG_INT_TX 0x02U

rt_err_t rt_sem_init(struct rt_semaphore *sem, const char *name, unsigned value, unsigned flag);
rt_err_t rt_sem_detach(struct rt_semaphore *sem);
rt_err_t rt_sem_take(struct rt_semaphore *sem, int timeout);
rt_err_t rt_sem_release(struct rt_semaphore *sem);
rt_err_t rt_sem_control(struct rt_semaphore *sem, int cmd, void *arg);
rt_err_t rt_mutex_init(struct rt_mutex *mutex, const char *name, unsigned flag);
rt_err_t rt_mutex_detach(struct rt_mutex *mutex);
rt_err_t rt_mutex_take(struct rt_mutex *mutex, int timeout);
rt_err_t rt_mutex_release(struct rt_mutex *mutex);
rt_thread_t rt_thread_create(const char *name, void (*entry)(void *parameter), void *parameter,
                             unsigned stackSize, unsigned priority, unsigned tick);
rt_err_t rt_thread_startup(rt_thread_t thread);
rt_err_t rt_thread_delete(rt_thread_t thread);
rt_thread_t rt_thread_self(void);
void *rt_calloc(rt_size_t count, rt_size_t size);
void rt_free(void *ptr);
rt_device_t rt_device_find(const char *name);
rt_err_t rt_device_open(rt_device_t dev, unsigned oflag);
rt_err_t rt_device_close(rt_device_t dev);
rt_err_t rt_device_control(rt_device_t dev, int cmd, void *arg);
rt_size_t rt_device_read(rt_device_t dev, rt_size_t pos, void *buffer, rt_size_t size);
rt_ssize_t rt_device_write(rt_device_t dev, rt_size_t pos, const void *buffer, rt_size_t size);
rt_err_t rt_device_set_rx_indicate(rt_device_t dev, rt_err_t (*rx_ind)(rt_device_t dev, rt_size_t size));

#endif /* CIA401_RTT_HOST_STUB_RTTHREAD_H */
