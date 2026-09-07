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

#endif /* CIA401_RTT_HOST_STUB_RTATOMIC_H */
