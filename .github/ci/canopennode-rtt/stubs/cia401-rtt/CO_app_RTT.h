#ifndef CIA401_RTT_HOST_STUB_CO_APP_RTT_H
#define CIA401_RTT_HOST_STUB_CO_APP_RTT_H

#include <rtthread.h>
#include "CANopen.h"
#if defined(PKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSIONS)
#include "CO_lifecycle_RTT.h"
#endif

struct CANopenNodeRTT {
    const char *canName;
    struct rt_mutex lifecycleMutex;
    CO_t *canOpenStack;
    rt_thread_t mainThread;
    rt_thread_t rtThread;
    rt_timer_t rtTimer;
#if defined(PKG_CANOPENNODE_RTT_LIFECYCLE_EXTENSIONS)
    CO_RTT_lifecycle_t lifecycle;
#endif
};

#endif /* CIA401_RTT_HOST_STUB_CO_APP_RTT_H */
