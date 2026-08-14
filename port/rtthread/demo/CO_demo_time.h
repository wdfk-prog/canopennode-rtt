/**
 * @file CO_demo_time.h
 * @brief TIME consumer diagnostic demo state and hooks.
 */

#ifndef CO_DEMO_TIME_H_
#define CO_DEMO_TIME_H_

#include "CANopen.h"

#if defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC)
#include <rtatomic.h>
#endif /* defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC) */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Runtime state for the optional TIME consumer diagnostic demo. */
typedef struct {
#if defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC)
    rt_atomic_t rxCount; /**< Valid DLC=6 TIME receptions observed by callback-pre. */
#else
    uint8_t reserved; /**< Keeps the type complete when the demo is disabled. */
#endif /* defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC) */
} CO_demo_time_t;

/**
 * @brief Initialize TIME diagnostic runtime state.
 *
 * @param demo TIME diagnostic state.
 */
void CO_demo_time_init(CO_demo_time_t *demo);

/**
 * @brief Bind TIME diagnostic callbacks to a newly initialized CANopenNode stack.
 *
 * @param demo TIME diagnostic state.
 * @param co Current CANopenNode object.
 */
void CO_demo_time_bind(CO_demo_time_t *demo, CO_t *co);

/**
 * @brief Publish the current applied TIME values to the demo diagnostic OD.
 *
 * @param demo TIME diagnostic state.
 * @param co Current CANopenNode object.
 */
void CO_demo_time_process(CO_demo_time_t *demo, const CO_t *co);

/**
 * @brief Handle a CANopen application communication reset.
 *
 * The receive counter intentionally survives communication reset to preserve the
 * existing diagnostic behavior. A newly initialized CANopenNode stack is bound
 * again through CO_demo_time_bind().
 *
 * @param demo TIME diagnostic state.
 */
void CO_demo_time_reset(CO_demo_time_t *demo);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_DEMO_TIME_H_ */
