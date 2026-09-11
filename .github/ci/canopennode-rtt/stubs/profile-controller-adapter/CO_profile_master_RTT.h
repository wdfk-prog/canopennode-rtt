#ifndef PROFILE_CONTROLLER_ADAPTER_HOST_STUB_MASTER_RTT_H
#define PROFILE_CONTROLLER_ADAPTER_HOST_STUB_MASTER_RTT_H

#include <rtthread.h>

#include "CO_profile_transport.h"

typedef struct {
    CO_profile_transport_t portableTransport;
    rt_bool_t attached;
} CO_profile_master_RTT_t;

CO_profile_transport_t *CO_profileMasterRTT_transport(CO_profile_master_RTT_t *runtime);

#endif /* PROFILE_CONTROLLER_ADAPTER_HOST_STUB_MASTER_RTT_H */
