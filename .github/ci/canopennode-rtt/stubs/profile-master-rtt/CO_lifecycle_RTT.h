#ifndef PROFILE_MASTER_RTT_HOST_STUB_CO_LIFECYCLE_RTT_H
#define PROFILE_MASTER_RTT_HOST_STUB_CO_LIFECYCLE_RTT_H

#include <stdint.h>

#include <rtthread.h>
#include "CANopen.h"
#include "CO_app_RTT.h"

typedef struct {
    rt_err_t (*runtimeInit)(CANopenNodeRTT *app, void *context);
    rt_err_t (*runtimeStart)(CANopenNodeRTT *app, void *context);
    void (*communicationStop)(CANopenNodeRTT *app, void *context);
    void (*communicationQuiesced)(CANopenNodeRTT *app, void *context);
    rt_err_t (*communicationBind)(CANopenNodeRTT *app, CO_t *co, OD_t *od, void *context);
    void (*communicationReady)(CANopenNodeRTT *app, void *context);
    void (*realtimeTick)(CANopenNodeRTT *app, void *context);
    void (*resetWakeups)(CANopenNodeRTT *app, void *context);
    void (*runtimeDeinit)(CANopenNodeRTT *app, void *context);
    void (*synchronousProcess)(CANopenNodeRTT *app, void *context, uint32_t dtUs);
    void (*nmtStateChanged)(CANopenNodeRTT *app, void *context, int state);
    void (*deferredProcess)(CANopenNodeRTT *app, void *context);
    void (*mainlineProcess)(CANopenNodeRTT *app, void *context, uint32_t dtUs,
                            CO_NMT_reset_cmd_t resetStatus, uint32_t *timerNextUs);
} CO_RTT_lifecycle_ops_t;

rt_err_t CO_RTT_lifecycleRegister(CANopenNodeRTT *app, const CO_RTT_lifecycle_ops_t *ops, void *context);
rt_bool_t CO_RTT_lifecycleHasOps(const CANopenNodeRTT *app, const CO_RTT_lifecycle_ops_t *ops);

#endif /* PROFILE_MASTER_RTT_HOST_STUB_CO_LIFECYCLE_RTT_H */
