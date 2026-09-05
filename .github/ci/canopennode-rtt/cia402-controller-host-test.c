/**
 * @file cia402-controller-host-test.c
 * @brief Host-only behavioral checks for the transport-agnostic CiA 402 Controller PDS API.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "CO_402_controller.h"

#define TEST_ASSERT(expr)                                                                  \
    do {                                                                                   \
        if (!(expr)) {                                                                     \
            fprintf(stderr, "CIA402_CONTROLLER_HOST_FAIL:%s:%d:%s\n", __func__, __LINE__, \
                    #expr);                                                                \
            return false;                                                                  \
        }                                                                                  \
    } while (0)

static CO_402_controller_config_t configWithTimeouts(uint32_t transition_us, uint32_t feedback_us)
{
    CO_402_controller_config_t config;

    config.transitionTimeout_us = transition_us;
    config.feedbackTimeout_us = feedback_us;
    return config;
}

static CO_402_controller_feedback_t feedbackForState(CO_402_state_t state)
{
    CO_402_controller_feedback_t feedback;

    feedback.statuswordValid = true;
    feedback.statusword = CO_402_statuswordForState(state);
    return feedback;
}

static CO_402_controller_result_t processState(CO_402_controller_axis_t *axis,
                                                CO_402_state_t state,
                                                uint32_t dt_us,
                                                CO_402_controller_controlword_update_t *update)
{
    CO_402_controller_feedback_t feedback = feedbackForState(state);
    return CO_402_controller_process(axis, &feedback, dt_us, update);
}

static bool expectCommand(const CO_402_controller_controlword_update_t *update, uint16_t value)
{
    TEST_ASSERT(update->valid);
    TEST_ASSERT(update->mask == CO_402_CONTROLLER_PDS_CONTROLWORD_MASK);
    TEST_ASSERT(update->value == value);
    return true;
}

static bool test_enable_sequence(void)
{
    CO_402_controller_axis_t axis;
    CO_402_controller_controlword_update_t update;
    CO_402_controller_config_t config = configWithTimeouts(10000U, 10000U);

    TEST_ASSERT(CO_402_controller_init(&axis, &config));
    TEST_ASSERT(CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));

    TEST_ASSERT(processState(&axis, CO_402_STATE_SWITCH_ON_DISABLED, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0006U));

    TEST_ASSERT(processState(&axis, CO_402_STATE_READY_TO_SWITCH_ON, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0007U));

    TEST_ASSERT(processState(&axis, CO_402_STATE_SWITCHED_ON, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x000FU));

    TEST_ASSERT(processState(&axis, CO_402_STATE_OPERATION_ENABLED, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_TARGET_REACHED);
    TEST_ASSERT(!update.valid);
    TEST_ASSERT(CO_402_controller_getRemoteState(&axis) == CO_402_STATE_OPERATION_ENABLED);
    return true;
}

static bool test_lower_targets(void)
{
    CO_402_controller_axis_t axis;
    CO_402_controller_controlword_update_t update;
    CO_402_controller_config_t config = configWithTimeouts(10000U, 10000U);

    TEST_ASSERT(CO_402_controller_init(&axis, &config));
    TEST_ASSERT(processState(&axis, CO_402_STATE_OPERATION_ENABLED, 0U, &update)
                == CO_402_CONTROLLER_RESULT_IDLE);

    TEST_ASSERT(CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_SWITCHED_ON));
    TEST_ASSERT(processState(&axis, CO_402_STATE_OPERATION_ENABLED, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0007U));
    TEST_ASSERT(processState(&axis, CO_402_STATE_SWITCHED_ON, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_TARGET_REACHED);
    TEST_ASSERT(!update.valid);

    TEST_ASSERT(CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_READY_TO_SWITCH_ON));
    TEST_ASSERT(processState(&axis, CO_402_STATE_SWITCHED_ON, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0006U));
    TEST_ASSERT(processState(&axis, CO_402_STATE_READY_TO_SWITCH_ON, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_TARGET_REACHED);
    TEST_ASSERT(!update.valid);

    TEST_ASSERT(CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_SWITCH_ON_DISABLED));
    TEST_ASSERT(processState(&axis, CO_402_STATE_READY_TO_SWITCH_ON, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0000U));
    TEST_ASSERT(processState(&axis, CO_402_STATE_SWITCH_ON_DISABLED, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_TARGET_REACHED);
    TEST_ASSERT(!update.valid);
    return true;
}

static bool test_not_ready_and_transition_timeout(void)
{
    CO_402_controller_axis_t axis;
    CO_402_controller_controlword_update_t update;
    CO_402_controller_config_t config = configWithTimeouts(3000U, 10000U);

    TEST_ASSERT(CO_402_controller_init(&axis, &config));
    TEST_ASSERT(CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));

    TEST_ASSERT(processState(&axis, CO_402_STATE_NOT_READY_TO_SWITCH_ON, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(!update.valid);
    TEST_ASSERT(processState(&axis, CO_402_STATE_NOT_READY_TO_SWITCH_ON, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(processState(&axis, CO_402_STATE_NOT_READY_TO_SWITCH_ON, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(processState(&axis, CO_402_STATE_NOT_READY_TO_SWITCH_ON, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_TRANSITION_TIMEOUT);
    TEST_ASSERT(!update.valid);
    return true;
}

static bool test_first_state_sample_can_emit_command(void)
{
    CO_402_controller_axis_t axis;
    CO_402_controller_controlword_update_t update;
    CO_402_controller_config_t config = configWithTimeouts(1000U, 10000U);

    TEST_ASSERT(CO_402_controller_init(&axis, &config));
    TEST_ASSERT(CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));
    TEST_ASSERT(processState(&axis, CO_402_STATE_SWITCH_ON_DISABLED, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0006U));
    TEST_ASSERT(processState(&axis, CO_402_STATE_SWITCH_ON_DISABLED, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_TRANSITION_TIMEOUT);
    TEST_ASSERT(!update.valid);
    return true;
}

static bool test_repeated_same_target_preserves_timeout(void)
{
    CO_402_controller_axis_t axis;
    CO_402_controller_controlword_update_t update;
    CO_402_controller_config_t config = configWithTimeouts(3000U, 10000U);

    TEST_ASSERT(CO_402_controller_init(&axis, &config));
    TEST_ASSERT(CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));
    TEST_ASSERT(processState(&axis, CO_402_STATE_SWITCH_ON_DISABLED, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0006U));

    TEST_ASSERT(CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));
    TEST_ASSERT(processState(&axis, CO_402_STATE_SWITCH_ON_DISABLED, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));
    TEST_ASSERT(processState(&axis, CO_402_STATE_SWITCH_ON_DISABLED, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));
    TEST_ASSERT(processState(&axis, CO_402_STATE_SWITCH_ON_DISABLED, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_TRANSITION_TIMEOUT);
    TEST_ASSERT(!update.valid);

    TEST_ASSERT(CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_SWITCHED_ON));
    TEST_ASSERT(processState(&axis, CO_402_STATE_SWITCH_ON_DISABLED, 2000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0006U));
    return true;
}

static bool test_fault_requires_explicit_reset(void)
{
    CO_402_controller_axis_t axis;
    CO_402_controller_controlword_update_t update;
    CO_402_controller_config_t config = configWithTimeouts(10000U, 10000U);

    TEST_ASSERT(CO_402_controller_init(&axis, &config));
    TEST_ASSERT(CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));
    TEST_ASSERT(processState(&axis, CO_402_STATE_FAULT_REACTION_ACTIVE, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_WAIT_FAULT_REACTION);
    TEST_ASSERT(!axis.targetValid);
    TEST_ASSERT(!update.valid);

    TEST_ASSERT(processState(&axis, CO_402_STATE_FAULT, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_REMOTE_FAULT);
    TEST_ASSERT(!update.valid);
    TEST_ASSERT(!CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));
    TEST_ASSERT(CO_402_controller_requestFaultReset(&axis));
    TEST_ASSERT(!CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));

    TEST_ASSERT(processState(&axis, CO_402_STATE_FAULT, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0080U));
    TEST_ASSERT(axis.faultResetPulseHigh);
    TEST_ASSERT(!CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));

    TEST_ASSERT(processState(&axis, CO_402_STATE_SWITCH_ON_DISABLED, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IDLE);
    TEST_ASSERT(expectCommand(&update, 0x0000U));
    TEST_ASSERT(!axis.faultResetPulseHigh);
    TEST_ASSERT(!axis.targetValid);
    TEST_ASSERT(CO_402_controller_getRemoteState(&axis) == CO_402_STATE_SWITCH_ON_DISABLED);
    TEST_ASSERT(CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));
    return true;
}


static bool test_fault_reset_low_survives_reset(void)
{
    CO_402_controller_axis_t axis;
    CO_402_controller_controlword_update_t update;
    CO_402_controller_feedback_t feedback;
    CO_402_controller_config_t config = configWithTimeouts(10000U, 10000U);
    uint16_t controlword = 0U;

    TEST_ASSERT(CO_402_controller_init(&axis, &config));
    TEST_ASSERT(processState(&axis, CO_402_STATE_FAULT, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_REMOTE_FAULT);
    TEST_ASSERT(CO_402_controller_requestFaultReset(&axis));
    TEST_ASSERT(processState(&axis, CO_402_STATE_FAULT, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0080U));
    controlword = CO_402_controller_applyControlwordUpdate(controlword, &update);
    TEST_ASSERT((controlword & 0x0080U) != 0U);

    CO_402_controller_reset(&axis);
    TEST_ASSERT(axis.faultResetPulseHigh);
    TEST_ASSERT(axis.config.transitionTimeout_us == config.transitionTimeout_us);
    TEST_ASSERT(axis.config.feedbackTimeout_us == config.feedbackTimeout_us);

    memset(&feedback, 0, sizeof(feedback));
    TEST_ASSERT(CO_402_controller_process(&axis, &feedback, 0U, &update)
                == CO_402_CONTROLLER_RESULT_IDLE);
    TEST_ASSERT(expectCommand(&update, 0x0000U));
    controlword = CO_402_controller_applyControlwordUpdate(controlword, &update);
    TEST_ASSERT((controlword & 0x0080U) == 0U);
    TEST_ASSERT(!axis.faultResetPulseHigh);

    TEST_ASSERT(processState(&axis, CO_402_STATE_FAULT, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_REMOTE_FAULT);
    TEST_ASSERT(CO_402_controller_requestFaultReset(&axis));
    TEST_ASSERT(processState(&axis, CO_402_STATE_FAULT, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0080U));
    return true;
}


static bool test_fault_reset_low_survives_unknown_status(void)
{
    CO_402_controller_axis_t axis;
    CO_402_controller_controlword_update_t update;
    CO_402_controller_feedback_t feedback;
    CO_402_controller_config_t config = configWithTimeouts(10000U, 10000U);

    TEST_ASSERT(CO_402_controller_init(&axis, &config));
    TEST_ASSERT(processState(&axis, CO_402_STATE_FAULT, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_REMOTE_FAULT);
    TEST_ASSERT(CO_402_controller_requestFaultReset(&axis));
    TEST_ASSERT(processState(&axis, CO_402_STATE_FAULT, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0080U));

    feedback.statuswordValid = true;
    feedback.statusword = 0x0011U;
    TEST_ASSERT(CO_402_controller_process(&axis, &feedback, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_STATUS_UNKNOWN);
    TEST_ASSERT(expectCommand(&update, 0x0000U));
    TEST_ASSERT(!axis.faultResetPulseHigh);

    TEST_ASSERT(processState(&axis, CO_402_STATE_FAULT, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_REMOTE_FAULT);
    TEST_ASSERT(CO_402_controller_requestFaultReset(&axis));
    TEST_ASSERT(processState(&axis, CO_402_STATE_FAULT, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0080U));
    return true;
}

static bool test_fault_reset_low_survives_timeout_and_fault(void)
{
    CO_402_controller_axis_t axis;
    CO_402_controller_controlword_update_t update;
    CO_402_controller_feedback_t feedback;
    CO_402_controller_config_t config = configWithTimeouts(10000U, 1000U);

    TEST_ASSERT(CO_402_controller_init(&axis, &config));
    TEST_ASSERT(processState(&axis, CO_402_STATE_FAULT, 0U, &update)
                == CO_402_CONTROLLER_RESULT_REMOTE_FAULT);
    TEST_ASSERT(CO_402_controller_requestFaultReset(&axis));
    TEST_ASSERT(processState(&axis, CO_402_STATE_FAULT, 0U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0080U));

    memset(&feedback, 0, sizeof(feedback));
    TEST_ASSERT(CO_402_controller_process(&axis, &feedback, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_FEEDBACK_TIMEOUT);
    TEST_ASSERT(expectCommand(&update, 0x0000U));
    TEST_ASSERT(!axis.faultResetPulseHigh);

    TEST_ASSERT(processState(&axis, CO_402_STATE_FAULT, 0U, &update)
                == CO_402_CONTROLLER_RESULT_REMOTE_FAULT);
    TEST_ASSERT(CO_402_controller_requestFaultReset(&axis));
    TEST_ASSERT(processState(&axis, CO_402_STATE_FAULT, 0U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0080U));
    TEST_ASSERT(processState(&axis, CO_402_STATE_FAULT, 0U, &update)
                == CO_402_CONTROLLER_RESULT_REMOTE_FAULT);
    TEST_ASSERT(expectCommand(&update, 0x0000U));
    TEST_ASSERT(!axis.faultResetPulseHigh);
    return true;
}

static bool test_quick_stop_policy(void)
{
    CO_402_controller_axis_t axis;
    CO_402_controller_controlword_update_t update;
    CO_402_controller_config_t config = configWithTimeouts(10000U, 10000U);

    TEST_ASSERT(CO_402_controller_init(&axis, &config));
    TEST_ASSERT(processState(&axis, CO_402_STATE_OPERATION_ENABLED, 0U, &update)
                == CO_402_CONTROLLER_RESULT_IDLE);
    TEST_ASSERT(CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_QUICK_STOP_ACTIVE));
    TEST_ASSERT(processState(&axis, CO_402_STATE_OPERATION_ENABLED, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0002U));
    TEST_ASSERT(processState(&axis, CO_402_STATE_QUICK_STOP_ACTIVE, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_TARGET_REACHED);

    TEST_ASSERT(CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));
    TEST_ASSERT(processState(&axis, CO_402_STATE_QUICK_STOP_ACTIVE, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_POLICY_BLOCKED);
    TEST_ASSERT(!update.valid);

    TEST_ASSERT(CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_SWITCH_ON_DISABLED));
    TEST_ASSERT(processState(&axis, CO_402_STATE_QUICK_STOP_ACTIVE, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0000U));
    return true;
}

static bool test_feedback_timeout_and_unknown_status(void)
{
    CO_402_controller_axis_t axis;
    CO_402_controller_controlword_update_t update;
    CO_402_controller_feedback_t feedback;
    CO_402_controller_config_t config = configWithTimeouts(10000U, 2000U);

    TEST_ASSERT(CO_402_controller_init(&axis, &config));
    TEST_ASSERT(CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));
    TEST_ASSERT(processState(&axis, CO_402_STATE_SWITCH_ON_DISABLED, 0U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);

    memset(&feedback, 0, sizeof(feedback));
    TEST_ASSERT(CO_402_controller_process(&axis, &feedback, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(CO_402_controller_process(&axis, &feedback, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_FEEDBACK_TIMEOUT);
    TEST_ASSERT(!update.valid);

    feedback.statuswordValid = true;
    feedback.statusword = 0x0011U;
    TEST_ASSERT(CO_402_controller_process(&axis, &feedback, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_STATUS_UNKNOWN);
    TEST_ASSERT(CO_402_controller_getRemoteState(&axis) == CO_402_STATE_UNKNOWN);
    TEST_ASSERT(!update.valid);

    feedback.statuswordValid = false;
    TEST_ASSERT(CO_402_controller_process(&axis, &feedback, 500U, &update)
                == CO_402_CONTROLLER_RESULT_STATUS_UNKNOWN);
    TEST_ASSERT(!update.valid);

    TEST_ASSERT(processState(&axis, CO_402_STATE_READY_TO_SWITCH_ON, 0U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0007U));
    return true;
}

static bool test_controlword_mask_preserves_mode_bits(void)
{
    CO_402_controller_axis_t axis;
    CO_402_controller_controlword_update_t update;
    CO_402_controller_config_t config = configWithTimeouts(10000U, 10000U);
    uint16_t currentControlword = 0x0170U;
    uint16_t merged;

    TEST_ASSERT(CO_402_controller_init(&axis, &config));
    TEST_ASSERT(CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));
    TEST_ASSERT(processState(&axis, CO_402_STATE_SWITCH_ON_DISABLED, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0006U));

    merged = CO_402_controller_applyControlwordUpdate(currentControlword, &update);
    TEST_ASSERT((merged & 0x0170U) == 0x0170U);
    TEST_ASSERT((merged & CO_402_CONTROLLER_PDS_CONTROLWORD_MASK) == 0x0006U);

    update.valid = false;
    TEST_ASSERT(CO_402_controller_applyControlwordUpdate(currentControlword, &update)
                == currentControlword);
    return true;
}

static bool test_multi_axis_isolation_and_reset(void)
{
    CO_402_controller_axis_t axis[3];
    CO_402_controller_controlword_update_t update;
    CO_402_controller_config_t config = configWithTimeouts(10000U, 10000U);
    size_t i;

    for (i = 0U; i < 3U; i++) {
        TEST_ASSERT(CO_402_controller_init(&axis[i], &config));
    }

    TEST_ASSERT(CO_402_controller_setTarget(&axis[0], CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));
    TEST_ASSERT(processState(&axis[0], CO_402_STATE_OPERATION_ENABLED, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_TARGET_REACHED);
    TEST_ASSERT(processState(&axis[1], CO_402_STATE_FAULT, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_REMOTE_FAULT);
    TEST_ASSERT(processState(&axis[2], CO_402_STATE_QUICK_STOP_ACTIVE, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_IDLE);

    TEST_ASSERT(CO_402_controller_requestFaultReset(&axis[1]));
    TEST_ASSERT(!axis[0].faultResetRequested);
    TEST_ASSERT(!axis[2].faultResetRequested);
    TEST_ASSERT(axis[0].targetValid);
    TEST_ASSERT(!axis[1].targetValid);

    CO_402_controller_reset(&axis[0]);
    TEST_ASSERT(axis[0].config.transitionTimeout_us == config.transitionTimeout_us);
    TEST_ASSERT(axis[0].config.feedbackTimeout_us == config.feedbackTimeout_us);
    TEST_ASSERT(CO_402_controller_getRemoteState(&axis[0]) == CO_402_STATE_UNKNOWN);
    TEST_ASSERT(CO_402_controller_getResult(&axis[0]) == CO_402_CONTROLLER_RESULT_IDLE);
    return true;
}


static bool test_public_helpers_and_disabled_timeouts(void)
{
    CO_402_controller_axis_t axis;
    CO_402_controller_controlword_update_t update;
    CO_402_controller_feedback_t feedback;
    CO_402_controller_config_t config = configWithTimeouts(0U, 0U);
    uint16_t controlword = 0x0150U;
    unsigned int i;

    TEST_ASSERT(CO_402_controller_init(&axis, &config));
    TEST_ASSERT(CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));
    TEST_ASSERT(processState(&axis, CO_402_STATE_SWITCH_ON_DISABLED, 0U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0006U));

    memset(&feedback, 0, sizeof(feedback));
    for (i = 0U; i < 4U; i++) {
        TEST_ASSERT(CO_402_controller_process(&axis, &feedback, UINT32_MAX, &update)
                    == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    }

    CO_402_controller_clearTarget(&axis);
    TEST_ASSERT(!axis.targetValid);
    TEST_ASSERT(CO_402_controller_getResult(&axis) == CO_402_CONTROLLER_RESULT_IDLE);
    TEST_ASSERT(CO_402_controller_process(&axis, &feedback, UINT32_MAX, &update)
                == CO_402_CONTROLLER_RESULT_IDLE);
    TEST_ASSERT(!update.valid);

    TEST_ASSERT(CO_402_controller_getRemoteState(NULL) == CO_402_STATE_UNKNOWN);
    TEST_ASSERT(CO_402_controller_getResult(NULL) == CO_402_CONTROLLER_RESULT_STATUS_UNKNOWN);
    TEST_ASSERT(CO_402_controller_applyControlwordUpdate(controlword, NULL) == controlword);
    CO_402_controller_clearTarget(NULL);
    CO_402_controller_reset(NULL);
    return true;
}

static bool test_invalid_public_requests(void)
{
    CO_402_controller_axis_t axis;
    CO_402_controller_controlword_update_t update;
    CO_402_controller_feedback_t feedback = feedbackForState(CO_402_STATE_SWITCH_ON_DISABLED);
    CO_402_controller_config_t config = configWithTimeouts(0U, 0U);
    CO_402_controller_target_t targetBefore;
    CO_402_controller_result_t resultBefore;
    uint32_t transitionElapsedBefore;

    TEST_ASSERT(!CO_402_controller_init(NULL, &config));
    TEST_ASSERT(!CO_402_controller_init(&axis, NULL));
    TEST_ASSERT(CO_402_controller_init(&axis, &config));
    TEST_ASSERT(!CO_402_controller_setTarget(&axis, (CO_402_controller_target_t)99));
    TEST_ASSERT(!axis.targetValid);

    TEST_ASSERT(CO_402_controller_setTarget(&axis, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));
    TEST_ASSERT(processState(&axis, CO_402_STATE_SWITCH_ON_DISABLED, 0U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(expectCommand(&update, 0x0006U));
    TEST_ASSERT(processState(&axis, CO_402_STATE_SWITCH_ON_DISABLED, 750U, &update)
                == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(axis.transitionElapsed_us == 750U);

    targetBefore = axis.target;
    resultBefore = axis.result;
    transitionElapsedBefore = axis.transitionElapsed_us;

    TEST_ASSERT(!CO_402_controller_setTarget(&axis, (CO_402_controller_target_t)-1));
    TEST_ASSERT(axis.targetValid);
    TEST_ASSERT(axis.target == targetBefore);
    TEST_ASSERT(axis.result == resultBefore);
    TEST_ASSERT(axis.transitionElapsed_us == transitionElapsedBefore);

    TEST_ASSERT(!CO_402_controller_setTarget(
        &axis,
        (CO_402_controller_target_t)(CO_402_CONTROLLER_TARGET_QUICK_STOP_ACTIVE + 1)));
    TEST_ASSERT(axis.targetValid);
    TEST_ASSERT(axis.target == targetBefore);
    TEST_ASSERT(axis.result == resultBefore);
    TEST_ASSERT(axis.transitionElapsed_us == transitionElapsedBefore);

    CO_402_controller_clearTarget(&axis);
    TEST_ASSERT(!axis.targetValid);
    TEST_ASSERT(!CO_402_controller_requestFaultReset(&axis));
    TEST_ASSERT(!axis.faultResetRequested);

    update.valid = true;
    update.value = UINT16_MAX;
    update.mask = UINT16_MAX;
    TEST_ASSERT(CO_402_controller_process(NULL, &feedback, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_STATUS_UNKNOWN);
    TEST_ASSERT(!update.valid);
    TEST_ASSERT(CO_402_controller_getResult(&axis) == CO_402_CONTROLLER_RESULT_IDLE);

    update.valid = true;
    TEST_ASSERT(CO_402_controller_process(&axis, NULL, 1000U, &update)
                == CO_402_CONTROLLER_RESULT_STATUS_UNKNOWN);
    TEST_ASSERT(!update.valid);
    TEST_ASSERT(CO_402_controller_getResult(&axis) == CO_402_CONTROLLER_RESULT_STATUS_UNKNOWN);

    TEST_ASSERT(CO_402_controller_process(&axis, &feedback, 1000U, NULL)
                == CO_402_CONTROLLER_RESULT_STATUS_UNKNOWN);
    TEST_ASSERT(CO_402_controller_getResult(&axis) == CO_402_CONTROLLER_RESULT_STATUS_UNKNOWN);
    return true;
}

struct test_case {
    const char *name;
    bool (*run)(void);
};

int main(void)
{
    static const struct test_case cases[] = {
        {"enable-sequence", test_enable_sequence},
        {"lower-targets", test_lower_targets},
        {"not-ready-transition-timeout", test_not_ready_and_transition_timeout},
        {"first-state-sample-command", test_first_state_sample_can_emit_command},
        {"repeated-same-target-timeout", test_repeated_same_target_preserves_timeout},
        {"explicit-fault-reset", test_fault_requires_explicit_reset},
        {"fault-reset-low-survives-reset", test_fault_reset_low_survives_reset},
        {"fault-reset-low-on-unknown-status", test_fault_reset_low_survives_unknown_status},
        {"fault-reset-low-on-timeout-fault", test_fault_reset_low_survives_timeout_and_fault},
        {"quick-stop-policy", test_quick_stop_policy},
        {"feedback-timeout-unknown-status", test_feedback_timeout_and_unknown_status},
        {"controlword-mask-preservation", test_controlword_mask_preserves_mode_bits},
        {"multi-axis-isolation-reset", test_multi_axis_isolation_and_reset},
        {"public-helpers-disabled-timeouts", test_public_helpers_and_disabled_timeouts},
        {"invalid-public-requests", test_invalid_public_requests},
    };
    size_t i;

    for (i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++) {
        if (!cases[i].run()) {
            return 1;
        }
        printf("CIA402_CONTROLLER_HOST_CASE_PASS:%s\n", cases[i].name);
    }

    printf("CIA402_CONTROLLER_HOST_PASS:%u\n", (unsigned int)(sizeof(cases) / sizeof(cases[0])));
    return 0;
}
