#ifndef PROFILE_MASTER_RTT_HOST_STUB_RTTHREAD_H
#define PROFILE_MASTER_RTT_HOST_STUB_RTTHREAD_H

#include <stddef.h>
#include <stdint.h>

typedef int rt_err_t;
typedef int rt_bool_t;
typedef int rt_atomic_t;
typedef void *rt_timer_t;

typedef struct rt_thread {
    unsigned id;
} *rt_thread_t;

struct rt_mutex {
    rt_bool_t initialized;
    rt_bool_t locked;
};

struct rt_semaphore {
    rt_bool_t initialized;
    unsigned value;
};

#define RT_EOK 0
#define RT_ERROR 1
#define RT_EBUSY 16
#define RT_EINVAL 22
#define RT_ENOSYS 38
#define RT_EFULL 28
#define RT_TRUE 1
#define RT_FALSE 0
#define RT_NULL NULL
#define RT_WAITING_FOREVER (-1)
#define RT_IPC_FLAG_FIFO 0
#define RT_IPC_FLAG_PRIO 1
#define RT_IPC_CMD_RESET 1


static inline rt_atomic_t rt_atomic_load(const rt_atomic_t *value)
{
    return *value;
}

static inline void rt_atomic_store(rt_atomic_t *value, rt_atomic_t desired)
{
    *value = desired;
}

static inline rt_atomic_t rt_atomic_add(rt_atomic_t *value, rt_atomic_t increment)
{
    rt_atomic_t previous = *value;
    *value += increment;
    return previous;
}

static inline rt_atomic_t rt_atomic_sub(rt_atomic_t *value, rt_atomic_t decrement)
{
    rt_atomic_t previous = *value;
    *value -= decrement;
    return previous;
}

rt_err_t rt_mutex_init(struct rt_mutex *mutex, const char *name, unsigned flag);
rt_err_t rt_mutex_detach(struct rt_mutex *mutex);
rt_err_t rt_mutex_take(struct rt_mutex *mutex, int timeout);
rt_err_t rt_mutex_release(struct rt_mutex *mutex);
rt_err_t rt_sem_init(struct rt_semaphore *sem, const char *name, unsigned value, unsigned flag);
rt_err_t rt_sem_detach(struct rt_semaphore *sem);
rt_err_t rt_sem_take(struct rt_semaphore *sem, int timeout);
rt_err_t rt_sem_release(struct rt_semaphore *sem);
rt_err_t rt_sem_control(struct rt_semaphore *sem, int cmd, void *arg);
rt_thread_t rt_thread_self(void);

#endif /* PROFILE_MASTER_RTT_HOST_STUB_RTTHREAD_H */
