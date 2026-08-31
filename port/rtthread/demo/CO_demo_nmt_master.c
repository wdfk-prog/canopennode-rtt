/**
 * @file CO_demo_nmt_master.c
 * @brief Heartbeat-supervised automatic NMT master validation demo implementation.
 */

#include "CO_demo_nmt_master.h"

#include "OD.h"
#include "co_rtt_log.h"

#include <stddef.h>
#include <string.h>

#if defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) && (((CO_CONFIG_NMT) & CO_CONFIG_NMT_MASTER) == 0)
#error "PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST requires CO_CONFIG_NMT_MASTER"
#endif /* defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) && (((CO_CONFIG_NMT) & CO_CONFIG_NMT_MASTER) == 0) */

#if defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) \
    && (((CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_ENABLE) == 0)
#error "PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST requires CO_CONFIG_HB_CONS_ENABLE"
#endif /* defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) && (((CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_ENABLE) == 0) */

#if defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) \
    && (((CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_QUERY_FUNCT) == 0)
#error "PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST requires CO_CONFIG_HB_CONS_QUERY_FUNCT"
#endif /* defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) && (((CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_QUERY_FUNCT) == 0) */

#if defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) \
    && (((CO_CONFIG_HB_CONS) & CO_CONFIG_FLAG_OD_DYNAMIC) == 0)
#error "PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST requires CO_CONFIG_FLAG_OD_DYNAMIC"
#endif /* defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) && (((CO_CONFIG_HB_CONS) & CO_CONFIG_FLAG_OD_DYNAMIC) == 0) */

#if defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST)
/** Non-blocking phases of the heartbeat-supervised NMT master test. */
typedef enum {
    CO_DEMO_NMT_MASTER_WAIT_LOCAL_OPERATIONAL = 0,
    CO_DEMO_NMT_MASTER_WAIT_REMOTE_ONLINE,
    CO_DEMO_NMT_MASTER_WAIT_REMOTE_PREOP,
    CO_DEMO_NMT_MASTER_SEND_COMMAND,
    CO_DEMO_NMT_MASTER_WAIT_REMOTE_STATE,
} CO_demo_nmt_master_phase_t;

/** One NMT command and the heartbeat state expected after it is processed. */
typedef struct {
    CO_NMT_command_t command;
    CO_NMT_internalState_t expectedState;
    bool_t requireBootup;
    const char *name;
} CO_demo_nmt_master_step_t;

/** Ordered remote-node command/state sequence used by the automatic test. */
static const CO_demo_nmt_master_step_t CO_demo_nmt_master_steps[] = {
    {CO_NMT_ENTER_OPERATIONAL, CO_NMT_OPERATIONAL, false, "START"},
    {CO_NMT_ENTER_STOPPED, CO_NMT_STOPPED, false, "STOP"},
    {CO_NMT_ENTER_PRE_OPERATIONAL, CO_NMT_PRE_OPERATIONAL, false, "PREOP"},
    {CO_NMT_RESET_COMMUNICATION, CO_NMT_PRE_OPERATIONAL, true, "RESET_COMM"},
    {CO_NMT_RESET_NODE, CO_NMT_PRE_OPERATIONAL, true, "RESET_NODE"},
    {CO_NMT_ENTER_OPERATIONAL, CO_NMT_OPERATIONAL, false, "START"},
};

#define CO_DEMO_NMT_MASTER_STEP_COUNT \
    (sizeof(CO_demo_nmt_master_steps) / sizeof(CO_demo_nmt_master_steps[0]))

/**
 * @brief Check whether a timeout elapsed with uint32_t wraparound safety.
 *
 * @param nowMs Current monotonic time in milliseconds.
 * @param referenceMs Previous reference timestamp in milliseconds.
 * @param timeoutMs Required elapsed timeout in milliseconds.
 * @return true when at least @p timeoutMs elapsed; otherwise false.
 */
static bool_t CO_demo_nmt_master_timeout_elapsed(uint32_t nowMs, uint32_t referenceMs, uint32_t timeoutMs)
{
    return ((uint32_t)(nowMs - referenceMs) >= timeoutMs) ? true : false;
}

/**
 * @brief Find an existing or free heartbeat-consumer slot for the target node.
 *
 * @param hbcons Heartbeat consumer object.
 * @param targetNodeId Remote node to supervise.
 * @return Zero-based consumer index, or -1 when no slot is available.
 */
static int8_t CO_demo_nmt_master_find_hb_consumer(CO_HBconsumer_t *hbcons, uint8_t targetNodeId)
{
    int8_t idx = CO_HBconsumer_getIdxByNodeId(hbcons, targetNodeId);
    uint8_t i;

    if (idx >= 0) {
        return idx;
    }

    for (i = 0U; i < hbcons->numberOfMonitoredNodes; i++) {
        if (CO_HBconsumer_getState(hbcons, i) == CO_HBconsumer_UNCONFIGURED) {
            return (int8_t)i;
        }
    }

    return -1;
}

/**
 * @brief Read the current valid remote NMT state from heartbeat supervision.
 *
 * @param demo NMT master validation state.
 * @param co Current CANopenNode object.
 * @param nmtState Receives the remote NMT state when valid.
 * @return true when heartbeat is ACTIVE and @p nmtState is valid.
 */
static bool_t CO_demo_nmt_master_get_remote_state(const CO_demo_nmt_master_t *demo, CO_t *co,
                                                  CO_NMT_internalState_t *nmtState)
{
    if ((demo == NULL) || (co == NULL) || (co->HBcons == NULL) || (nmtState == NULL)
        || (demo->hbConsumerIdx < 0)) {
        return false;
    }

    if (CO_HBconsumer_getState(co->HBcons, (uint8_t)demo->hbConsumerIdx) != CO_HBconsumer_ACTIVE) {
        return false;
    }

    return (CO_HBconsumer_getNmtState(co->HBcons, (uint8_t)demo->hbConsumerIdx, nmtState) == 0) ? true : false;
}

/**
 * @brief Verify that the demo still owns the configured Heartbeat Consumer entry.
 *
 * External SDO writes may reconfigure OD 0x1016 while the test is running. The
 * cached consumer index is valid only while both the runtime node mapping and
 * the OD value still match the value installed by the demo.
 *
 * @param demo NMT master validation state.
 * @param co Current CANopenNode object.
 * @param targetNodeId Expected remote Node-ID.
 * @return true while the demo-owned 0x1016 entry is unchanged; otherwise false.
 */
static bool_t CO_demo_nmt_master_hb_consumer_owned(const CO_demo_nmt_master_t *demo, CO_t *co,
                                                   uint8_t targetNodeId)
{
    uint32_t currentConfig;
    ODR_t odRet;
    int8_t currentIdx;

    if ((demo == NULL) || (co == NULL) || (co->HBcons == NULL) || !demo->hbConfigured
        || !demo->hbOverrideActive || (demo->hbConsumerIdx < 0)) {
        return false;
    }

    currentIdx = CO_HBconsumer_getIdxByNodeId(co->HBcons, targetNodeId);
    if (currentIdx != demo->hbConsumerIdx) {
        CO_RTT_LOG_E("NMT master test heartbeat consumer ownership lost: target=%u expectedIdx=%d currentIdx=%d",
                     targetNodeId, demo->hbConsumerIdx, currentIdx);
        return false;
    }

    odRet = OD_get_u32(OD_ENTRY_H1016, (uint8_t)demo->hbConsumerIdx + 1U, &currentConfig, true);
    if (odRet != ODR_OK) {
        CO_RTT_LOG_E("NMT master test OD 0x1016 ownership read failed: sub=%u ret=%d",
                     (uint8_t)demo->hbConsumerIdx + 1U, odRet);
        return false;
    }
    if (currentConfig != demo->hbConsumerApplied) {
        CO_RTT_LOG_E("NMT master test heartbeat consumer changed externally: sub=%u expected=0x%08x current=0x%08x",
                     (uint8_t)demo->hbConsumerIdx + 1U, (unsigned int)demo->hbConsumerApplied,
                     (unsigned int)currentConfig);
        return false;
    }

    return true;
}

/**
 * @brief Restore the heartbeat-consumer OD entry replaced by the demo test.
 *
 * @param demo NMT master validation state.
 * @return true when no override is active or the original 0x1016 value is restored.
 */
static bool_t CO_demo_nmt_master_restore_hb_consumer(CO_demo_nmt_master_t *demo)
{
    uint32_t currentConfig;
    ODR_t odRet;

    if ((demo == NULL) || !demo->hbOverrideActive || (demo->hbConsumerIdx < 0)) {
        return true;
    }

    odRet = OD_get_u32(OD_ENTRY_H1016, (uint8_t)demo->hbConsumerIdx + 1U, &currentConfig, true);
    if (odRet != ODR_OK) {
        CO_RTT_LOG_E("NMT master test OD 0x1016 restore read failed: sub=%u ret=%d",
                     (uint8_t)demo->hbConsumerIdx + 1U, odRet);
        return false;
    }

    if (currentConfig != demo->hbConsumerApplied) {
        CO_RTT_LOG_W("NMT master test OD 0x1016 ownership lost; leaving external value unchanged: "
                     "sub=%u expected=0x%08x current=0x%08x",
                     (uint8_t)demo->hbConsumerIdx + 1U, (unsigned int)demo->hbConsumerApplied,
                     (unsigned int)currentConfig);
        demo->hbOverrideActive = false;
        demo->hbConfigured = false;
        demo->hbConsumerIdx = -1;
        demo->hbConsumerOriginal = 0U;
        demo->hbConsumerApplied = 0U;
        return false;
    }

    odRet = OD_set_u32(OD_ENTRY_H1016, (uint8_t)demo->hbConsumerIdx + 1U, demo->hbConsumerOriginal, false);
    if (odRet != ODR_OK) {
        CO_RTT_LOG_E("NMT master test OD 0x1016 restore failed: sub=%u ret=%d",
                     (uint8_t)demo->hbConsumerIdx + 1U, odRet);
        return false;
    }

    demo->hbOverrideActive = false;
    demo->hbConfigured = false;
    demo->hbConsumerIdx = -1;
    demo->hbConsumerOriginal = 0U;
    demo->hbConsumerApplied = 0U;
    return true;
}
#endif /* defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) */

void CO_demo_nmt_master_init(CO_demo_nmt_master_t *demo)
{
    if (demo != NULL) {
        (void)memset(demo, 0, sizeof(*demo));
        demo->hbConsumerIdx = -1;
#if defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST)
        demo->phase = (uint8_t)CO_DEMO_NMT_MASTER_WAIT_LOCAL_OPERATIONAL;
#endif /* defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) */
    }
}

void CO_demo_nmt_master_bind(CO_demo_nmt_master_t *demo, CO_t *co)
{
#if defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST)
    int8_t idx;
    ODR_t odRet;
    uint32_t consumerConfig;
    uint32_t consumerOriginal;
    const uint8_t targetNodeId = (uint8_t)PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST_TARGET_NODE_ID;
    const uint16_t hbTimeoutMs = (uint16_t)PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST_HB_TIMEOUT_MS;

    if ((demo == NULL) || (co == NULL) || (co->HBcons == NULL)) {
        if (demo != NULL) {
            demo->failed = true;
        }
        CO_RTT_LOG_E("NMT master test heartbeat consumer is unavailable");
        return;
    }

    idx = CO_demo_nmt_master_find_hb_consumer(co->HBcons, targetNodeId);
    if (idx < 0) {
        CO_RTT_LOG_E("NMT master test has no free OD 0x1016 entry: target=%u", targetNodeId);
        demo->failed = true;
        return;
    }

    odRet = OD_get_u32(OD_ENTRY_H1016, (uint8_t)idx + 1U, &consumerOriginal, true);
    if (odRet != ODR_OK) {
        CO_RTT_LOG_E("NMT master test OD 0x1016 read failed: sub=%u target=%u ret=%d",
                     (uint8_t)idx + 1U, targetNodeId, odRet);
        demo->failed = true;
        return;
    }

    consumerConfig = ((uint32_t)targetNodeId << 16) | (uint32_t)hbTimeoutMs;
    odRet = OD_set_u32(OD_ENTRY_H1016, (uint8_t)idx + 1U, consumerConfig, false);
    if (odRet != ODR_OK) {
        CO_RTT_LOG_E("NMT master test OD 0x1016 configuration failed: sub=%u target=%u ret=%d",
                     (uint8_t)idx + 1U, targetNodeId, odRet);
        demo->failed = true;
        return;
    }

    demo->hbConsumerIdx = idx;
    demo->hbConsumerOriginal = consumerOriginal;
    demo->hbConsumerApplied = consumerConfig;
    demo->hbConfigured = true;
    demo->hbOverrideActive = true;
    CO_RTT_LOG_I("NMT master test heartbeat supervision configured: target=%u sub=%u timeoutMs=%u",
                 targetNodeId, (uint8_t)idx + 1U, (unsigned int)hbTimeoutMs);
#else
    (void)demo;
    (void)co;
#endif /* defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) */
}

void CO_demo_nmt_master_process(CO_demo_nmt_master_t *demo, CO_t *co, uint8_t localNodeId,
                                uint32_t nowMs, CO_NMT_reset_cmd_t resetStatus)
{
#if defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST)
    CO_ReturnError_t err;
    CO_HBconsumer_state_t hbState;
    CO_NMT_internalState_t remoteNmtState;
    const CO_demo_nmt_master_step_t *step;
    const uint8_t targetNodeId = (uint8_t)PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST_TARGET_NODE_ID;
    const uint32_t stateTimeoutMs = (uint32_t)PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST_STATE_TIMEOUT_MS;

    if ((demo == NULL) || (co == NULL) || (co->NMT == NULL) || (co->HBcons == NULL) || (co->CANmodule == NULL)) {
        return;
    }

    /* Restore the demo-owned OD override before a local reset destroys the
     * current CANopenNode stack. Handle reset before the terminal-state check
     * so a previous cleanup failure gets one more restore attempt. */
    if (resetStatus != CO_RESET_NOT) {
        if (demo->hbOverrideActive) {
            goto cleanup;
        }
        return;
    }

    if ((demo->finished == true) || (demo->failed == true)) {
        return;
    }
    if (!co->CANmodule->CANnormal) {
        return;
    }

    if (!demo->hbConfigured || (demo->hbConsumerIdx < 0)) {
        CO_RTT_LOG_E("NMT master test heartbeat supervision is not configured");
        goto fail;
    }

    if (!CO_demo_nmt_master_hb_consumer_owned(demo, co, targetNodeId)) {
        goto fail;
    }

    if (targetNodeId == localNodeId) {
        CO_RTT_LOG_E("NMT master test target conflicts with local node: node=%u", targetNodeId);
        goto fail;
    }

    switch ((CO_demo_nmt_master_phase_t)demo->phase) {
    case CO_DEMO_NMT_MASTER_WAIT_LOCAL_OPERATIONAL:
        if (CO_NMT_getInternalState(co->NMT) != CO_NMT_OPERATIONAL) {
            return;
        }
        demo->phase = (uint8_t)CO_DEMO_NMT_MASTER_WAIT_REMOTE_ONLINE;
        CO_RTT_LOG_I("NMT master test waiting for remote heartbeat: target=%u", targetNodeId);
        return;

    case CO_DEMO_NMT_MASTER_WAIT_REMOTE_ONLINE:
        if (!CO_demo_nmt_master_get_remote_state(demo, co, &remoteNmtState)) {
            return;
        }
        if (remoteNmtState == CO_NMT_PRE_OPERATIONAL) {
            demo->phase = (uint8_t)CO_DEMO_NMT_MASTER_SEND_COMMAND;
            CO_RTT_LOG_I("NMT master test remote ready: target=%u state=%d", targetNodeId,
                         (int)remoteNmtState);
            return;
        }

        err = CO_NMT_sendCommand(co->NMT, CO_NMT_ENTER_PRE_OPERATIONAL, targetNodeId);
        if (err != CO_ERROR_NO) {
            CO_RTT_LOG_E("NMT master test PRE-OP preparation TX failed: target=%u state=%d err=%d",
                         targetNodeId, (int)remoteNmtState, err);
            goto fail;
        }
        demo->referenceTimeMs = nowMs;
        demo->resetObserved = false;
        demo->phase = (uint8_t)CO_DEMO_NMT_MASTER_WAIT_REMOTE_PREOP;
        CO_RTT_LOG_I("NMT master test normalizing remote to PRE-OP: target=%u state=%d", targetNodeId,
                     (int)remoteNmtState);
        return;

    case CO_DEMO_NMT_MASTER_WAIT_REMOTE_PREOP:
        if (CO_demo_nmt_master_timeout_elapsed(nowMs, demo->referenceTimeMs, stateTimeoutMs)) {
            CO_RTT_LOG_E("NMT master test PRE-OP normalization timeout: target=%u", targetNodeId);
            goto fail;
        }
        if (!CO_demo_nmt_master_get_remote_state(demo, co, &remoteNmtState)
            || (remoteNmtState != CO_NMT_PRE_OPERATIONAL)) {
            return;
        }

        if (demo->resetObserved && (demo->step < CO_DEMO_NMT_MASTER_STEP_COUNT)
            && CO_demo_nmt_master_steps[demo->step].requireBootup) {
            step = &CO_demo_nmt_master_steps[demo->step];
            CO_RTT_LOG_I("NMT master test state passed after normalization: step=%u command=%s target=%u state=%d",
                         demo->step + 1U, step->name, targetNodeId, (int)remoteNmtState);
            demo->step++;
            demo->resetObserved = false;
        } else {
            CO_RTT_LOG_I("NMT master test remote ready after normalization: target=%u state=%d", targetNodeId,
                         (int)remoteNmtState);
        }
        demo->phase = (uint8_t)CO_DEMO_NMT_MASTER_SEND_COMMAND;
        return;

    case CO_DEMO_NMT_MASTER_SEND_COMMAND:
        if (demo->step >= CO_DEMO_NMT_MASTER_STEP_COUNT) {
            goto complete;
        }

        step = &CO_demo_nmt_master_steps[demo->step];
        if (!CO_demo_nmt_master_get_remote_state(demo, co, &remoteNmtState)) {
            CO_RTT_LOG_E("NMT master test remote heartbeat lost before TX: step=%u command=%s target=%u",
                         demo->step + 1U, step->name, targetNodeId);
            goto fail;
        }
        if (!step->requireBootup && (remoteNmtState == step->expectedState)) {
            CO_RTT_LOG_E("NMT master test invalid pre-command state: step=%u command=%s target=%u state=%d",
                         demo->step + 1U, step->name, targetNodeId, (int)remoteNmtState);
            goto fail;
        }
        err = CO_NMT_sendCommand(co->NMT, step->command, targetNodeId);
        if (err != CO_ERROR_NO) {
            CO_RTT_LOG_E("NMT master test TX failed: step=%u command=%s target=%u err=%d", demo->step + 1U,
                         step->name, targetNodeId, err);
            goto fail;
        }

        demo->referenceTimeMs = nowMs;
        demo->resetObserved = false;
        demo->phase = (uint8_t)CO_DEMO_NMT_MASTER_WAIT_REMOTE_STATE;
        CO_RTT_LOG_I("NMT master test TX: step=%u command=%s target=%u", demo->step + 1U, step->name,
                     targetNodeId);
        return;

    case CO_DEMO_NMT_MASTER_WAIT_REMOTE_STATE:
        if (demo->step >= CO_DEMO_NMT_MASTER_STEP_COUNT) {
            CO_RTT_LOG_E("NMT master test invalid step while waiting: step=%u", demo->step);
            goto fail;
        }

        step = &CO_demo_nmt_master_steps[demo->step];
        if (CO_demo_nmt_master_timeout_elapsed(nowMs, demo->referenceTimeMs, stateTimeoutMs)) {
            CO_RTT_LOG_E("NMT master test state timeout: step=%u command=%s target=%u expected=%d",
                         demo->step + 1U, step->name, targetNodeId, (int)step->expectedState);
            goto fail;
        }

        hbState = CO_HBconsumer_getState(co->HBcons, (uint8_t)demo->hbConsumerIdx);
        if (step->requireBootup && !demo->resetObserved) {
            if (hbState == CO_HBconsumer_UNKNOWN) {
                demo->resetObserved = true;
                CO_RTT_LOG_I("NMT master test remote boot-up observed: step=%u target=%u", demo->step + 1U,
                             targetNodeId);
            }
            return;
        }

        if (!CO_demo_nmt_master_get_remote_state(demo, co, &remoteNmtState)) {
            return;
        }
        if (remoteNmtState != step->expectedState) {
            if (step->requireBootup && demo->resetObserved) {
                err = CO_NMT_sendCommand(co->NMT, CO_NMT_ENTER_PRE_OPERATIONAL, targetNodeId);
                if (err != CO_ERROR_NO) {
                    CO_RTT_LOG_E("NMT master test post-reset PRE-OP preparation TX failed: "
                                 "step=%u target=%u state=%d err=%d",
                                 demo->step + 1U, targetNodeId, (int)remoteNmtState, err);
                    goto fail;
                }
                demo->referenceTimeMs = nowMs;
                demo->phase = (uint8_t)CO_DEMO_NMT_MASTER_WAIT_REMOTE_PREOP;
                CO_RTT_LOG_I("NMT master test normalizing post-reset state: step=%u target=%u state=%d",
                             demo->step + 1U, targetNodeId, (int)remoteNmtState);
            }
            return;
        }

        CO_RTT_LOG_I("NMT master test state passed: step=%u command=%s target=%u state=%d", demo->step + 1U,
                     step->name, targetNodeId, (int)remoteNmtState);
        demo->step++;
        demo->phase = (uint8_t)CO_DEMO_NMT_MASTER_SEND_COMMAND;
        return;

    default:
        CO_RTT_LOG_E("NMT master test invalid phase: phase=%u", demo->phase);
        goto fail;
    }

complete:
    demo->finished = true;
    goto cleanup;

fail:
    demo->failed = true;

cleanup:
    if (!CO_demo_nmt_master_restore_hb_consumer(demo) && (resetStatus == CO_RESET_NOT)) {
        demo->finished = false;
        demo->failed = true;
    }
    if (demo->finished) {
        CO_RTT_LOG_I("NMT master test completed: target=%u", targetNodeId);
    }
    return;
#else
    (void)demo;
    (void)co;
    (void)localNodeId;
    (void)nowMs;
    (void)resetStatus;
#endif /* defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) */
}

#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
void CO_demo_nmt_master_update_timer_next(const CO_demo_nmt_master_t *demo, uint32_t nowMs,
                                          uint32_t *timerNextUs)
{
#if defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST)
    uint32_t elapsedMs;
    uint32_t remainingMs;
    uint64_t remainingUs;

    if ((demo == NULL) || (timerNextUs == NULL) || demo->finished || demo->failed) {
        return;
    }

    if (demo->phase == (uint8_t)CO_DEMO_NMT_MASTER_SEND_COMMAND) {
        *timerNextUs = 0U;
        return;
    }

    if ((demo->phase != (uint8_t)CO_DEMO_NMT_MASTER_WAIT_REMOTE_PREOP)
        && (demo->phase != (uint8_t)CO_DEMO_NMT_MASTER_WAIT_REMOTE_STATE)) {
        return;
    }

    elapsedMs = (uint32_t)(nowMs - demo->referenceTimeMs);
    if (elapsedMs >= (uint32_t)PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST_STATE_TIMEOUT_MS) {
        *timerNextUs = 0U;
        return;
    }

    remainingMs = (uint32_t)PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST_STATE_TIMEOUT_MS - elapsedMs;
    remainingUs = (uint64_t)remainingMs * 1000ULL;
    if (remainingUs > UINT32_MAX) {
        remainingUs = UINT32_MAX;
    }
    if ((uint32_t)remainingUs < *timerNextUs) {
        *timerNextUs = (uint32_t)remainingUs;
    }
#else
    (void)demo;
    (void)nowMs;
    (void)timerNextUs;
#endif /* defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) */
}
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */

void CO_demo_nmt_master_reset(CO_demo_nmt_master_t *demo)
{
#if defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST)
    if ((demo != NULL) && demo->hbOverrideActive) {
        (void)CO_demo_nmt_master_restore_hb_consumer(demo);
    }
#endif /* defined(PKG_CANOPENNODE_DEMO_NMT_MASTER_TEST) */
    CO_demo_nmt_master_init(demo);
}
