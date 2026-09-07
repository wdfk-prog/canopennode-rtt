#ifndef CIA401_RTT_HOST_STUB_CO_APP_RTT_H
#define CIA401_RTT_HOST_STUB_CO_APP_RTT_H

#include <rtthread.h>
#include "CANopen.h"

struct CANopenNodeRTT {
    struct rt_mutex lifecycleMutex;
    CO_t *canOpenStack;
    rt_thread_t mainThread;
    rt_thread_t rtThread;
    rt_timer_t rtTimer;
};

#endif /* CIA401_RTT_HOST_STUB_CO_APP_RTT_H */
