/**
 * @file CO_demo.h
 * @brief Optional CANopenNode RT-Thread demo/test dispatcher.
 */

#ifndef CO_DEMO_H_
#define CO_DEMO_H_

#include "CANopen.h"

#if defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC)
#include "CO_demo_time.h"
#endif /* defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST)
#include "CO_demo_nmt_master.h"
#endif /* defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Long-lived state for optional demo/test modules owned by one application instance. */
typedef struct {
#if defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC)
    CO_demo_time_t time; /**< TIME consumer diagnostic state. */
#endif /* defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC) */
#if defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST)
    CO_demo_nmt_master_t nmtMaster; /**< Automatic NMT master validation state. */
#endif /* defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) */
#if !defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC) && !defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST)
    uint8_t reserved; /**< Keeps the dispatcher state complete when no demo is enabled. */
#endif /* !defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC) && !defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) */
} CO_demo_t;

/**
 * @brief Initialize all enabled demo/test module states.
 *
 * @param demo Demo dispatcher state owned by the application instance.
 */
#if defined(PKG_CANOPENNODE_USING_DEMO_OD)
void CO_demo_init(CO_demo_t *demo);
#else
static inline void CO_demo_init(CO_demo_t *demo)
{
    (void)demo;
}
#endif /* defined(PKG_CANOPENNODE_USING_DEMO_OD) */

/**
 * @brief Bind enabled demo/test modules to a newly initialized CANopenNode stack.
 *
 * This hook is called after all communication objects are initialized and before
 * the CAN module enters normal mode, including after communication reset. This
 * keeps callbacks bound before receive processing can start.
 *
 * @param demo Demo dispatcher state owned by the application instance.
 * @param co Current CANopenNode object.
 */
#if defined(PKG_CANOPENNODE_USING_DEMO_OD)
void CO_demo_bind(CO_demo_t *demo, CO_t *co);
#else
static inline void CO_demo_bind(CO_demo_t *demo, CO_t *co)
{
    (void)demo;
    (void)co;
}
#endif /* defined(PKG_CANOPENNODE_USING_DEMO_OD) */

/**
 * @brief Process all enabled non-blocking demo/test modules from the mainline thread.
 *
 * @param demo Demo dispatcher state owned by the application instance.
 * @param co Current CANopenNode object.
 * @param localNodeId Active local CANopen Node-ID.
 * @param nowMs Current monotonic RT-Thread time in milliseconds.
 * @param resetStatus Reset request returned by the preceding CO_process() call.
 */
#if defined(PKG_CANOPENNODE_USING_DEMO_OD)
void CO_demo_process(CO_demo_t *demo, CO_t *co, uint8_t localNodeId, uint32_t nowMs,
                     CO_NMT_reset_cmd_t resetStatus);
#else
static inline void CO_demo_process(CO_demo_t *demo, CO_t *co, uint8_t localNodeId, uint32_t nowMs,
                                   CO_NMT_reset_cmd_t resetStatus)
{
    (void)demo;
    (void)co;
    (void)localNodeId;
    (void)nowMs;
    (void)resetStatus;
}
#endif /* defined(PKG_CANOPENNODE_USING_DEMO_OD) */

/**
 * @brief Reset enabled demo/test module state before local communication reset.
 *
 * @param demo Demo dispatcher state owned by the application instance.
 */
#if defined(PKG_CANOPENNODE_USING_DEMO_OD)
void CO_demo_reset(CO_demo_t *demo);
#else
static inline void CO_demo_reset(CO_demo_t *demo)
{
    (void)demo;
}
#endif /* defined(PKG_CANOPENNODE_USING_DEMO_OD) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_DEMO_H_ */
