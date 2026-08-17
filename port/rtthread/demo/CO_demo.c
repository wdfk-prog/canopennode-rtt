/**
 * @file CO_demo.c
 * @brief Optional CANopenNode RT-Thread demo/test dispatcher implementation.
 */

#include "CO_demo.h"

#if !CO_DEMO_ENABLED
#error "CO_demo.c requires at least one enabled demo/test module"
#endif /* !CO_DEMO_ENABLED */

void CO_demo_init(CO_demo_t *demo)
{
    if (demo == NULL) {
        return;
    }

#if defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC)
    CO_demo_time_init(&demo->time);
#endif /* defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC)
    CO_demo_emcy_consumer_init(&demo->emcyConsumer);
#endif /* defined(PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST)
    CO_demo_nmt_master_init(&demo->nmtMaster);
#endif /* defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) */
}

bool_t CO_demo_bind(CO_demo_t *demo, CO_t *co)
{
    bool_t bound = true;

    if ((demo == NULL) || (co == NULL)) {
        return false;
    }

#if defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC)
    CO_demo_time_bind(&demo->time, co);
#endif /* defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC)
    if (!CO_demo_emcy_consumer_bind(&demo->emcyConsumer, co)) {
        bound = false;
    }
#endif /* defined(PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST)
    CO_demo_nmt_master_bind(&demo->nmtMaster, co);
#endif /* defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) */

    return bound;
}

void CO_demo_process(CO_demo_t *demo, CO_t *co, uint8_t localNodeId, uint32_t nowMs,
                     CO_NMT_reset_cmd_t resetStatus)
{
    if ((demo == NULL) || (co == NULL)) {
        return;
    }

#if defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC)
    CO_demo_time_process(&demo->time, co);
#endif /* defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC)
    CO_demo_emcy_consumer_process(&demo->emcyConsumer);
#endif /* defined(PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST)
    CO_demo_nmt_master_process(&demo->nmtMaster, co, localNodeId, nowMs, resetStatus);
#else
    (void)localNodeId;
    (void)nowMs;
    (void)resetStatus;
#endif /* defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) */
}

void CO_demo_reset(CO_demo_t *demo)
{
    if (demo == NULL) {
        return;
    }

#if defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC)
    CO_demo_time_reset(&demo->time);
#endif /* defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC)
    CO_demo_emcy_consumer_reset(&demo->emcyConsumer);
#endif /* defined(PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST)
    CO_demo_nmt_master_reset(&demo->nmtMaster);
#endif /* defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) */
}

void CO_demo_deinit(CO_demo_t *demo)
{
    if (demo == NULL) {
        return;
    }

#if defined(PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC)
    CO_demo_emcy_consumer_deinit(&demo->emcyConsumer);
#endif /* defined(PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC) */
}
