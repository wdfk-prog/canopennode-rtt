/**
 * @file CO_demo_time.h
 * @brief TIME consumer diagnostic demo state and hooks.
 */

#ifndef CO_DEMO_TIME_H_
#define CO_DEMO_TIME_H_

#include "CANopen.h"

#if !defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC)
#error "CO_demo_time requires PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC"
#endif /* !defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC) */

#include <rtatomic.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Runtime state for the optional TIME consumer diagnostic demo. */
typedef struct {
    rt_atomic_t rxCount; /**< Valid DLC=6 TIME receptions observed by the diagnostic receive hook. */
} CO_demo_time_t;

/**
 * @brief Initialize TIME diagnostic runtime state.
 *
 * @param demo TIME diagnostic state.
 */
void CO_demo_time_init(CO_demo_time_t *demo);

/**
 * @brief Record one syntactically valid TIME reception.
 *
 * In timerNext mode the mainline scheduler owns the CANopenNode TIME callback-pre
 * slot and calls this hook directly. Legacy polling mode reaches the same hook
 * through the demo-owned callback-pre registration.
 *
 * @param demo TIME diagnostic state.
 */
void CO_demo_time_on_receive(CO_demo_time_t *demo);

/**
 * @brief Bind legacy TIME diagnostics or publish initial state for timerNext mode.
 *
 * When PKG_CANOPENNODE_GLOBAL_TIMERNEXT is enabled, callback-pre ownership belongs
 * to the mainline scheduler and this function does not register another callback.
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
