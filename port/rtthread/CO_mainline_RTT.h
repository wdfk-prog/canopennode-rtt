/**
 * @file CO_mainline_RTT.h
 * @brief Event-driven RT-Thread mainline scheduler for CANopenNode.
 * @details This module is compiled only when PKG_CANOPENNODE_GLOBAL_TIMERNEXT is enabled.
 */

#ifndef CO_MAINLINE_RTT_H_
#define CO_MAINLINE_RTT_H_

#include <rtthread.h>
#include <stdint.h>

#include "CANopen.h"

#if !defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
#error "CO_mainline_RTT requires PKG_CANOPENNODE_GLOBAL_TIMERNEXT"
#endif /* !defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */

#if !defined(PKG_CANOPENNODE_GLOBAL_CALLBACK_PRE)
#error "PKG_CANOPENNODE_GLOBAL_TIMERNEXT requires PKG_CANOPENNODE_GLOBAL_CALLBACK_PRE"
#endif /* !defined(PKG_CANOPENNODE_GLOBAL_CALLBACK_PRE) */

#if !defined(RT_USING_EVENT)
#error "PKG_CANOPENNODE_GLOBAL_TIMERNEXT requires RT_USING_EVENT"
#endif /* !defined(RT_USING_EVENT) */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct CANopenNodeRTT;
typedef struct CANopenNodeRTT CANopenNodeRTT;

/** Runtime state owned by the event-driven mainline scheduler. */
typedef struct {
    struct rt_event wakeEvent; /**< Coalescing wake event for callback-pre and runtime producers. */
    rt_bool_t initialized; /**< True after the RT-Thread event object has been initialized. */
} CO_RTT_mainline_t;

/**
 * @brief Initialize the event-driven mainline scheduler.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
rt_err_t CO_RTT_mainlineInit(CANopenNodeRTT *app);

/**
 * @brief Detach resources owned by the event-driven mainline scheduler.
 *
 * @param app CANopenNode RT-Thread application instance.
 */
void CO_RTT_mainlineDeinit(CANopenNodeRTT *app);

/**
 * @brief Clear a stale mainline wake notification after old CAN RX is quiesced.
 *
 * @param app CANopenNode RT-Thread application instance.
 */
void CO_RTT_mainlineResetWakeups(CANopenNodeRTT *app);

/**
 * @brief Register callback-pre wake hooks on all supported mainline objects.
 *
 * This function is called after communication objects are initialized and
 * before the CAN module enters normal mode. In timerNext mode the scheduler owns
 * the supported CANopenNode callback-pre slots, including TIME. Existing
 * callback-pre registrations are replaced and are neither retained nor invoked.
 * The optional TIME diagnostic is notified directly by the scheduler-owned TIME
 * callback, so it does not require a second callback-pre owner.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @param co Current initialized CANopenNode object.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
rt_err_t CO_RTT_mainlineBindCallbacks(CANopenNodeRTT *app, CO_t *co);

/**
 * @brief Signal that asynchronous mainline work is available.
 *
 * The event is level-like for this use: repeated sends coalesce into one wake
 * bit. No protocol work, blocking operation, or logging is performed here.
 *
 * @param app CANopenNode RT-Thread application instance.
 */
void CO_RTT_mainlineWakeup(CANopenNodeRTT *app);

/**
 * @brief Merge wrapper-owned deadlines into the current mainline deadline.
 *
 * This keeps scheduler-specific compatibility deadlines out of CO_app_RTT.c.
 * It covers wrapper state whose next wakeup is not propagated through upstream
 * CO_process(), such as runtime LSS timing, TIME producer timing, storage auto
 * processing, and legacy polling-only objects.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @param co Current initialized CANopenNode object.
 * @param nowMs Current RT-Thread time in milliseconds.
 * @param resetStatus Reset request returned by the current CO_process() call.
 * @param timerNextUs Mainline deadline accumulator initialized by the caller.
 */
void CO_RTT_mainlineUpdateDeadline(const CANopenNodeRTT *app, const CO_t *co, uint32_t nowMs,
                                   CO_NMT_reset_cmd_t resetStatus, uint32_t *timerNextUs);

/**
 * @brief Wait until the current mainline deadline or an asynchronous wake event.
 *
 * The function subtracts time already spent after the deadline was calculated
 * and rounds the remaining delay down to RT-Thread tick granularity so the wait
 * never extends beyond @p timerNextUs. A zero or positive sub-tick remainder
 * returns immediately instead of injecting an extra scheduling delay.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @param deadlineBaseUs Timestamp at which @p timerNextUs became relative.
 * @param timerNextUs Maximum delay requested by CANopenNode/wrapper logic.
 */
void CO_RTT_mainlineWait(CANopenNodeRTT *app, uint32_t deadlineBaseUs, uint32_t timerNextUs);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_MAINLINE_RTT_H_ */
