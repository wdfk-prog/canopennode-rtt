/**
 * @file CO_mainline_RTT.c
 * @brief Event-driven RT-Thread mainline scheduler for CANopenNode.
 */

#include "CO_mainline_RTT.h"

#include "CO_app_RTT.h"
#include "CO_time_RTT.h"
#include "OD.h"

#include <limits.h>

#define CO_RTT_MAINLINE_EVENT_WAKE                     ((rt_uint32_t)0x01U)
#define CO_RTT_MAINLINE_COMPAT_POLL_US                  1000U
/* Keep waits below half the wrapping timestamp domain so elapsed-time ordering remains unambiguous. */
#define CO_RTT_MAINLINE_MAX_WAIT_US                    (UINT32_MAX / 2U)

/** Merge one scheduler-owned relative deadline into the current accumulator. */
void co_rtt_mainline_deadline_min(uint32_t *timerNextUs, uint32_t candidateUs)
{
    if ((timerNextUs != NULL) && (candidateUs < *timerNextUs)) {
        *timerNextUs = candidateUs;
    }
}

/** Wake the mainline worker from a CANopenNode callback-pre hook. */
static void co_rtt_mainline_callback_pre(void *object)
{
    CO_RTT_mainlineWakeup((CANopenNodeRTT *)object);
}

#if ((CO_CONFIG_TIME) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0
/** Handle TIME callback-pre work owned by the event-driven scheduler. */
static void co_rtt_mainline_time_callback_pre(void *object)
{
    CANopenNodeRTT *app = (CANopenNodeRTT *)object;

    if (app == NULL) {
        return;
    }

#if defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC)
    CO_demo_time_on_receive(&app->demo.time);
#endif /* defined(PKG_CANOPENNODE_DEMO_TIME_DIAGNOSTIC) */
    CO_RTT_mainlineWakeup(app);
}
#endif /* ((CO_CONFIG_TIME) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0 */

/** Convert a microsecond delay to a bounded RT-Thread timeout without exceeding the deadline. */
static rt_int32_t co_rtt_mainline_us_to_ticks(uint32_t delayUs)
{
    uint64_t ticks = ((uint64_t)delayUs * RT_TICK_PER_SECOND) / 1000000ULL;

    if (ticks > (uint64_t)INT32_MAX) {
        ticks = (uint64_t)INT32_MAX;
    }

    return (rt_int32_t)ticks;
}

#if ((CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0
/** Return the configured number of SDO server objects. */
static uint8_t co_rtt_mainline_sdo_server_count(const CO_t *co)
{
#ifdef CO_MULTIPLE_OD
    return ((co != NULL) && (co->config != NULL)) ? co->config->CNT_SDO_SRV : 0U;
#else
    (void)co;
    return (uint8_t)OD_CNT_SDO_SRV;
#endif /* CO_MULTIPLE_OD */
}
#endif /* ((CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0 */

#if ((CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0
/** Return the configured number of SDO client objects. */
static uint8_t co_rtt_mainline_sdo_client_count(const CO_t *co)
{
#ifdef CO_MULTIPLE_OD
    return ((co != NULL) && (co->config != NULL)) ? co->config->CNT_SDO_CLI : 0U;
#else
    (void)co;
    return (uint8_t)OD_CNT_SDO_CLI;
#endif /* CO_MULTIPLE_OD */
}
#endif /* ((CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0 */

#if (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0) && defined(PKG_CANOPENNODE_LSS_PERSIST)
/** Merge the application-side CiA 305 bitrate activation deadline. */
static void co_rtt_mainline_update_lss_deadline(const CANopenNodeRTT *app, uint32_t nowMs,
                                                uint32_t *timerNextUs)
{
    int32_t remainingMs;
    uint64_t remainingUs;

    if ((app == NULL) || ((app->lssBitrateState != CO_APP_RTT_LSS_BITRATE_PRE_DELAY)
                          && (app->lssBitrateState != CO_APP_RTT_LSS_BITRATE_POST_DELAY))) {
        return;
    }

    remainingMs = (int32_t)(app->lssBitrateDeadlineMs - nowMs);
    if (remainingMs <= 0) {
        co_rtt_mainline_deadline_min(timerNextUs, 0U);
        return;
    }

    remainingUs = (uint64_t)(uint32_t)remainingMs * 1000ULL;
    co_rtt_mainline_deadline_min(timerNextUs,
                                 (remainingUs > UINT32_MAX) ? UINT32_MAX : (uint32_t)remainingUs);
}
#endif /* (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0) && defined(PKG_CANOPENNODE_LSS_PERSIST) */

#if (((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LSS) != 0) && (((CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER) != 0)
/**
 * @brief Preserve cyclic progress while Gateway ASCII owns an active LSS command.
 *
 * Gateway LSS calls CO_LSSmaster_*() as a cyclic state machine, but that path
 * does not propagate an LSS timerNext deadline through CO_GTWA_process(). Some
 * multi-step commands also need another cycle after one LSS transfer completes.
 * Keep the legacy cadence only for an active Gateway LSS state; the LSS master
 * callback-pre still wakes earlier when a CAN response arrives.
 */
static void co_rtt_mainline_update_gateway_lss_deadline(const CO_t *co, uint32_t *timerNextUs)
{
    const CO_GTWA_t *gtwa;

    if ((co == NULL) || (co->gtwa == NULL) || (co->LSSmaster == NULL)) {
        return;
    }

    gtwa = co->gtwa;
    if ((gtwa->state >= CO_GTWA_ST_LSS_SWITCH_GLOB) && (gtwa->state <= CO_GTWA_ST_LSS_ALLNODES)) {
        co_rtt_mainline_deadline_min(timerNextUs, CO_RTT_MAINLINE_COMPAT_POLL_US);
    }
}
#endif /* (((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LSS) != 0) && (((CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER) != 0) */

#if ((CO_CONFIG_TIME) & CO_CONFIG_TIME_PRODUCER) != 0
/** Merge the TIME producer deadline, which upstream CO_TIME_process() does not expose through timerNext. */
static void co_rtt_mainline_update_time_producer_deadline(const CO_t *co, uint32_t *timerNextUs)
{
    CO_NMT_internalState_t nmtState;
    uint64_t remainingUs;

    if ((co == NULL) || co->nodeIdUnconfigured || (co->NMT == NULL) || (co->TIME == NULL)
        || !co->TIME->isProducer || (co->TIME->producerInterval_ms == 0U)) {
        return;
    }

    nmtState = CO_NMT_getInternalState(co->NMT);
    if ((nmtState != CO_NMT_PRE_OPERATIONAL) && (nmtState != CO_NMT_OPERATIONAL)) {
        return;
    }

    if (co->TIME->producerTimer_ms >= co->TIME->producerInterval_ms) {
        co_rtt_mainline_deadline_min(timerNextUs, 0U);
        return;
    }

    remainingUs = (uint64_t)(co->TIME->producerInterval_ms - co->TIME->producerTimer_ms) * 1000ULL;
    if (remainingUs <= (uint64_t)co->TIME->residual_us) {
        co_rtt_mainline_deadline_min(timerNextUs, 0U);
    } else {
        remainingUs -= (uint64_t)co->TIME->residual_us;
        co_rtt_mainline_deadline_min(timerNextUs,
                                     (remainingUs > UINT32_MAX) ? UINT32_MAX : (uint32_t)remainingUs);
    }
}
#endif /* ((CO_CONFIG_TIME) & CO_CONFIG_TIME_PRODUCER) != 0 */

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0
/** Preserve cyclic processing only when at least one automatic storage entry exists. */
static void co_rtt_mainline_update_storage_deadline(const CO_storage_t *storage, uint32_t *timerNextUs)
{
    uint8_t i;

    if ((storage == NULL) || !storage->enabled || (storage->entries == NULL)) {
        return;
    }

    for (i = 0U; i < storage->entriesCount; i++) {
        if ((storage->entries[i].attr & (uint8_t)CO_storage_auto) != 0U) {
            co_rtt_mainline_deadline_min(timerNextUs, CO_RTT_MAINLINE_COMPAT_POLL_US);
            return;
        }
    }
}
#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */

rt_err_t CO_RTT_mainlineInit(CANopenNodeRTT *app)
{
    rt_err_t ret;

    if (app == NULL) {
        return -RT_EINVAL;
    }

    app->mainline.initialized = RT_FALSE;

    ret = rt_event_init(&app->mainline.wakeEvent, "co_evt", RT_IPC_FLAG_FIFO);
    if (ret == RT_EOK) {
        app->mainline.initialized = RT_TRUE;
    }
    return ret;
}

void CO_RTT_mainlineDeinit(CANopenNodeRTT *app)
{
    if ((app == NULL) || (app->mainline.initialized != RT_TRUE)) {
        return;
    }

    app->mainline.initialized = RT_FALSE;
    (void)rt_event_detach(&app->mainline.wakeEvent);
}

void CO_RTT_mainlineResetWakeups(CANopenNodeRTT *app)
{
    rt_uint32_t received = 0U;

    if ((app == NULL) || (app->mainline.initialized != RT_TRUE)) {
        return;
    }

    (void)rt_event_recv(&app->mainline.wakeEvent, CO_RTT_MAINLINE_EVENT_WAKE,
                        RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, 0, &received);
}

rt_err_t CO_RTT_mainlineBindCallbacks(CANopenNodeRTT *app, CO_t *co)
{
    if ((app == NULL) || (co == NULL) || (app->mainline.initialized != RT_TRUE)) {
        return -RT_EINVAL;
    }

#if ((CO_CONFIG_NMT) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0
    if (co->NMT != NULL) {
        CO_NMT_initCallbackPre(co->NMT, app, co_rtt_mainline_callback_pre);
    }
#endif /* ((CO_CONFIG_NMT) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0 */

#if ((CO_CONFIG_EM) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0
    if (co->em != NULL) {
        CO_EM_initCallbackPre(co->em, app, co_rtt_mainline_callback_pre);
    }
#endif /* ((CO_CONFIG_EM) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0 */

#if ((CO_CONFIG_HB_CONS) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0
    if (co->HBcons != NULL) {
        CO_HBconsumer_initCallbackPre(co->HBcons, app, co_rtt_mainline_callback_pre);
    }
#endif /* ((CO_CONFIG_HB_CONS) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0 */

#if ((CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0
    if (co->SDOserver != NULL) {
        const uint8_t count = co_rtt_mainline_sdo_server_count(co);
        uint8_t i;

        for (i = 0U; i < count; i++) {
            CO_SDOserver_initCallbackPre(&co->SDOserver[i], app, co_rtt_mainline_callback_pre);
        }
    }
#endif /* ((CO_CONFIG_SDO_SRV) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0 */

#if ((CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0
    if (co->SDOclient != NULL) {
        const uint8_t count = co_rtt_mainline_sdo_client_count(co);
        uint8_t i;

        for (i = 0U; i < count; i++) {
            CO_SDOclient_initCallbackPre(&co->SDOclient[i], app, co_rtt_mainline_callback_pre);
        }
    }
#endif /* ((CO_CONFIG_SDO_CLI) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0 */

#if ((CO_CONFIG_TIME) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0
    if (co->TIME != NULL) {
        CO_TIME_initCallbackPre(co->TIME, app, co_rtt_mainline_time_callback_pre);
    }
#endif /* ((CO_CONFIG_TIME) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0 */

#if ((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0 && ((CO_CONFIG_LSS) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0
    if (co->LSSslave != NULL) {
        CO_LSSslave_initCallbackPre(co->LSSslave, app, co_rtt_mainline_callback_pre);
    }
#endif /* ((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0 && ((CO_CONFIG_LSS) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0 */

#if ((CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER) != 0 && ((CO_CONFIG_LSS) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0
    if (co->LSSmaster != NULL) {
        CO_LSSmaster_initCallbackPre(co->LSSmaster, app, co_rtt_mainline_callback_pre);
    }
#endif /* ((CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER) != 0 && ((CO_CONFIG_LSS) & CO_CONFIG_FLAG_CALLBACK_PRE) != 0 */

    return RT_EOK;
}

void CO_RTT_mainlineWakeup(CANopenNodeRTT *app)
{
    if ((app != NULL) && (app->mainline.initialized == RT_TRUE)) {
        (void)rt_event_send(&app->mainline.wakeEvent, CO_RTT_MAINLINE_EVENT_WAKE);
    }
}

void CO_RTT_mainlineUpdateDeadline(const CANopenNodeRTT *app, const CO_t *co, uint32_t nowMs,
                                   CO_NMT_reset_cmd_t resetStatus, uint32_t *timerNextUs)
{
    if ((app == NULL) || (co == NULL) || (timerNextUs == NULL) || (resetStatus != CO_RESET_NOT)) {
        return;
    }

    (void)nowMs;

#if (((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LSS) != 0) && (((CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER) != 0)
    co_rtt_mainline_update_gateway_lss_deadline(co, timerNextUs);
#endif /* (((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LSS) != 0) && (((CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER) != 0) */

#if (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0) && defined(PKG_CANOPENNODE_LSS_PERSIST)
    co_rtt_mainline_update_lss_deadline(app, nowMs, timerNextUs);
#endif /* (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0) && defined(PKG_CANOPENNODE_LSS_PERSIST) */

#if ((CO_CONFIG_TIME) & CO_CONFIG_TIME_PRODUCER) != 0
    co_rtt_mainline_update_time_producer_deadline(co, timerNextUs);
#endif /* ((CO_CONFIG_TIME) & CO_CONFIG_TIME_PRODUCER) != 0 */

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0
    co_rtt_mainline_update_storage_deadline(&app->storage, timerNextUs);
#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */

#if (CO_CONFIG_NODE_GUARDING != 0) \
    || defined(PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC) \
    || defined(PKG_CANOPENNODE_DEMO_SRDO_DIAGNOSTIC) \
    || (((CO_CONFIG_TRACE) & CO_CONFIG_TRACE_ENABLE) != 0)
    /*
     * These features still require cyclic mainline processing and cannot rely
     * exclusively on callback-pre wakeups or native timerNext deadlines.
     */
    co_rtt_mainline_deadline_min(timerNextUs, CO_RTT_MAINLINE_COMPAT_POLL_US);
#endif /* Node Guarding || GFC diagnostic || SRDO diagnostic || Trace */
}

void CO_RTT_mainlineWait(CANopenNodeRTT *app, uint32_t deadlineBaseUs, uint32_t timerNextUs)
{
    uint32_t waitUs;
    uint32_t elapsedUs;
    uint32_t nowUs;
    rt_uint32_t received = 0U;
    rt_int32_t timeoutTicks;

    if ((app == NULL) || (app->mainline.initialized != RT_TRUE)) {
        return;
    }

    if (timerNextUs == 0U) {
        return;
    }

    waitUs = timerNextUs;
    if (waitUs > CO_RTT_MAINLINE_MAX_WAIT_US) {
        waitUs = CO_RTT_MAINLINE_MAX_WAIT_US;
    }

    nowUs = CO_RTT_timeNowUs();
    elapsedUs = CO_RTT_timeElapsedUs(nowUs, deadlineBaseUs);
    if (elapsedUs >= waitUs) {
        return;
    }

    /*
     * RT-Thread event timeout has tick granularity. Round down so the scheduler
     * never sleeps beyond the CANopenNode timerNext deadline. A sub-tick
     * remainder cannot be represented safely, so process again immediately.
     */
    timeoutTicks = co_rtt_mainline_us_to_ticks(waitUs - elapsedUs);
    if (timeoutTicks <= 0) {
        return;
    }

    (void)rt_event_recv(&app->mainline.wakeEvent, CO_RTT_MAINLINE_EVENT_WAKE,
                        RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR, timeoutTicks, &received);
}
