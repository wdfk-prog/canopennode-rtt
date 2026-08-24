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
#if defined(PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC)
    CO_demo_gfc_init(&demo->gfc);
#endif /* defined(PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_SDO_BLOCK_TEST)
    CO_demo_sdo_block_init(&demo->sdoBlock);
#endif /* defined(PKG_CANOPENNODE_DEMO_SDO_BLOCK_TEST) */
#if defined(PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST)
    CO_demo_sdo_client_init(&demo->sdoClient);
#endif /* defined(PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST) */
#if defined(PKG_CANOPENNODE_DEMO_STORAGE_DIAGNOSTIC)
    CO_demo_storage_init(&demo->storage);
#endif /* defined(PKG_CANOPENNODE_DEMO_STORAGE_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST)
    CO_demo_nmt_master_init(&demo->nmtMaster);
#endif /* defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) */
}

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0
void CO_demo_on_storage_init(CO_demo_t *demo, CO_storage_t *storage, CO_storage_entry_t *entries,
                             uint8_t entriesCount, CO_ReturnError_t initResult, uint32_t initError)
{
    if (demo == NULL) {
        return;
    }

#if defined(PKG_CANOPENNODE_DEMO_STORAGE_DIAGNOSTIC)
    CO_demo_storage_on_backend_init(&demo->storage, storage, entries, entriesCount, initResult, initError);
#else
    (void)storage;
    (void)entries;
    (void)entriesCount;
    (void)initResult;
    (void)initError;
#endif /* defined(PKG_CANOPENNODE_DEMO_STORAGE_DIAGNOSTIC) */
}
#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */

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
#if defined(PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC)
    if (!CO_demo_gfc_bind(&demo->gfc, co)) {
        bound = false;
    }
#endif /* defined(PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_SDO_BLOCK_TEST)
    if (!CO_demo_sdo_block_bind(&demo->sdoBlock, co)) {
        bound = false;
    }
#endif /* defined(PKG_CANOPENNODE_DEMO_SDO_BLOCK_TEST) */
#if defined(PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST)
    if (!CO_demo_sdo_client_bind(&demo->sdoClient, co)) {
        bound = false;
    }
#endif /* defined(PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST) */
#if defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST)
    CO_demo_nmt_master_bind(&demo->nmtMaster, co);
#endif /* defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) */

    return bound;
}

void CO_demo_process(CO_demo_t *demo, CO_t *co, uint8_t localNodeId, uint32_t nowMs,
                     uint32_t timeDifferenceUs, CO_NMT_reset_cmd_t resetStatus)
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
#if defined(PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC)
    CO_demo_gfc_process(&demo->gfc, co);
#endif /* defined(PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST)
    CO_demo_sdo_client_process(&demo->sdoClient, co, localNodeId, timeDifferenceUs, resetStatus);
#else
    (void)timeDifferenceUs;
#endif /* defined(PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST) */
#if defined(PKG_CANOPENNODE_DEMO_STORAGE_DIAGNOSTIC)
    CO_demo_storage_process(&demo->storage);
#endif /* defined(PKG_CANOPENNODE_DEMO_STORAGE_DIAGNOSTIC) */
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
#if defined(PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC)
    CO_demo_gfc_reset(&demo->gfc);
#endif /* defined(PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST)
    CO_demo_sdo_client_reset(&demo->sdoClient);
#endif /* defined(PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST) */
#if defined(PKG_CANOPENNODE_DEMO_STORAGE_DIAGNOSTIC)
    CO_demo_storage_reset(&demo->storage);
#endif /* defined(PKG_CANOPENNODE_DEMO_STORAGE_DIAGNOSTIC) */
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
