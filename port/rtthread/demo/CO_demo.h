/**
 * @file CO_demo.h
 * @brief Optional CANopenNode RT-Thread demo/test dispatcher.
 */

#ifndef CO_DEMO_H_
#define CO_DEMO_H_

#include "CANopen.h"

#if defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC) \
    || defined(PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC) \
    || defined(PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC) \
    || defined(PKG_CANOPENNODE_DEMO_SDO_BLOCK_TEST) \
    || defined(PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST) \
    || defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST)
#define CO_DEMO_ENABLED 1
#else
#define CO_DEMO_ENABLED 0
#endif

#if CO_DEMO_ENABLED
#if !defined(PKG_CANOPENNODE_USING_DEMO_OD)
#error "CANopenNode demo/test modules require PKG_CANOPENNODE_USING_DEMO_OD"
#endif /* !defined(PKG_CANOPENNODE_USING_DEMO_OD) */

#if defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC)
#include "CO_demo_time.h"
#endif /* defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC)
#include "CO_demo_emcy_consumer.h"
#endif /* defined(PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC)
#include "CO_demo_gfc.h"
#endif /* defined(PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_SDO_BLOCK_TEST)
#include "CO_demo_sdo_block.h"
#endif /* defined(PKG_CANOPENNODE_DEMO_SDO_BLOCK_TEST) */
#if defined(PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST)
#include "CO_demo_sdo_client.h"
#endif /* defined(PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST) */
#if defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST)
#include "CO_demo_nmt_master.h"
#endif /* defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Long-lived state for the enabled demo/test modules owned by one application instance. */
typedef struct {
#if defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC)
    CO_demo_time_t time; /**< TIME consumer diagnostic state. */
#endif /* defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC)
    CO_demo_emcy_consumer_t emcyConsumer; /**< EMCY consumer diagnostic state. */
#endif /* defined(PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC)
    CO_demo_gfc_t gfc; /**< GFC protocol diagnostic and producer state. */
#endif /* defined(PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_SDO_BLOCK_TEST)
    CO_demo_sdo_block_t sdoBlock; /**< Test-only variable-length SDO server DOMAIN state. */
#endif /* defined(PKG_CANOPENNODE_DEMO_SDO_BLOCK_TEST) */
#if defined(PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST)
    CO_demo_sdo_client_t sdoClient; /**< Test-only non-blocking SDO client state. */
#endif /* defined(PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST) */
#if defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST)
    CO_demo_nmt_master_t nmtMaster; /**< Automatic NMT master validation state. */
#endif /* defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) */
} CO_demo_t;

/**
 * @brief Initialize all enabled demo/test module states.
 *
 * @param demo Demo dispatcher state owned by the application instance.
 */
void CO_demo_init(CO_demo_t *demo);

/**
 * @brief Bind enabled demo/test modules to a newly initialized CANopenNode stack.
 *
 * This hook is called after all communication objects are initialized and before
 * the CAN module enters normal mode, including after communication reset. This
 * keeps callbacks bound before receive processing can start.
 *
 * @param demo Demo dispatcher state owned by the application instance.
 * @param co Current CANopenNode object.
 * @return true when all enabled demo callbacks are bound, otherwise false.
 */
bool_t CO_demo_bind(CO_demo_t *demo, CO_t *co);

/**
 * @brief Process all enabled non-blocking demo/test modules from the mainline thread.
 *
 * @param demo Demo dispatcher state owned by the application instance.
 * @param co Current CANopenNode object.
 * @param localNodeId Active local CANopen Node-ID.
 * @param nowMs Current monotonic RT-Thread time in milliseconds.
 * @param timeDifferenceUs Elapsed mainline time passed to CANopenNode in microseconds.
 * @param resetStatus Reset request returned by the preceding CO_process() call.
 */
void CO_demo_process(CO_demo_t *demo, CO_t *co, uint8_t localNodeId, uint32_t nowMs,
                     uint32_t timeDifferenceUs, CO_NMT_reset_cmd_t resetStatus);

/**
 * @brief Reset enabled demo/test module state before local communication reset.
 *
 * @param demo Demo dispatcher state owned by the application instance.
 */
void CO_demo_reset(CO_demo_t *demo);

/**
 * @brief Release resources owned by optional demo/test modules during init rollback.
 *
 * The runtime calls this only after CAN receive callbacks are stopped.
 *
 * @param demo Demo dispatcher state owned by the application instance.
 */
void CO_demo_deinit(CO_demo_t *demo);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* CO_DEMO_ENABLED */

#endif /* CO_DEMO_H_ */
