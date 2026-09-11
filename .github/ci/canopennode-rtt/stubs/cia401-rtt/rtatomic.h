#ifndef CIA401_RTT_HOST_STUB_RTATOMIC_H
#define CIA401_RTT_HOST_STUB_RTATOMIC_H

typedef int rt_atomic_t;

static inline void rt_atomic_store(rt_atomic_t *value, int desired)
{
    *value = desired;
}

static inline int rt_atomic_load(const rt_atomic_t *value)
{
    return *value;
}

static inline int rt_atomic_flag_test_and_set(rt_atomic_t *value)
{
    const int previous = *value;
    *value = 1;
    return previous;
}

static inline void rt_atomic_flag_clear(rt_atomic_t *value)
{
    *value = 0;
}

#endif /* CIA401_RTT_HOST_STUB_RTATOMIC_H */
