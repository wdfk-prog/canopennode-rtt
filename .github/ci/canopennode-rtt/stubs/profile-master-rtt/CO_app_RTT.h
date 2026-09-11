#ifndef PROFILE_MASTER_RTT_HOST_STUB_CO_APP_RTT_H
#define PROFILE_MASTER_RTT_HOST_STUB_CO_APP_RTT_H

#include <rtthread.h>
#include "CANopen.h"

typedef struct CANopenNodeRTT {
    CO_t *canOpenStack;
    rt_thread_t mainThread;
    rt_thread_t rtThread;
    rt_timer_t rtTimer;
} CANopenNodeRTT;

#endif /* PROFILE_MASTER_RTT_HOST_STUB_CO_APP_RTT_H */
