/**
 * @file CO_402_controller.c
 * @brief Pure-C CiA 402 Controller PDS sequencing implementation.
 */

#include <string.h>

#include "CO_402_controller.h"

static uint32_t saturatingAddU32(uint32_t value, uint32_t increment)
{
    if (UINT32_MAX - value < increment) {
        return UINT32_MAX;
    }
    return value + increment;
}

static bool targetValid(CO_402_controller_target_t target)
{
    return target >= CO_402_CONTROLLER_TARGET_SWITCH_ON_DISABLED
           && target <= CO_402_CONTROLLER_TARGET_QUICK_STOP_ACTIVE;
}

static CO_402_state_t targetState(CO_402_controller_target_t target)
{
    switch (target) {
        case CO_402_CONTROLLER_TARGET_SWITCH_ON_DISABLED:
            return CO_402_STATE_SWITCH_ON_DISABLED;
        case CO_402_CONTROLLER_TARGET_READY_TO_SWITCH_ON:
            return CO_402_STATE_READY_TO_SWITCH_ON;
        case CO_402_CONTROLLER_TARGET_SWITCHED_ON:
            return CO_402_STATE_SWITCHED_ON;
        case CO_402_CONTROLLER_TARGET_OPERATION_ENABLED:
            return CO_402_STATE_OPERATION_ENABLED;
        case CO_402_CONTROLLER_TARGET_QUICK_STOP_ACTIVE:
            return CO_402_STATE_QUICK_STOP_ACTIVE;
        default:
            return CO_402_STATE_UNKNOWN;
    }
}

static bool commandUpdate(CO_402_command_t command,
                          CO_402_controller_controlword_update_t *update)
{
    uint16_t value;

    switch (command) {
        case CO_402_COMMAND_DISABLE_VOLTAGE:
            value = 0x0000U;
            break;
        case CO_402_COMMAND_QUICK_STOP:
            value = 0x0002U;
            break;
        case CO_402_COMMAND_SHUTDOWN:
            value = 0x0006U;
            break;
        case CO_402_COMMAND_SWITCH_ON_OR_DISABLE_OPERATION:
            value = 0x0007U;
            break;
        case CO_402_COMMAND_ENABLE_OPERATION:
            value = 0x000FU;
            break;
        case CO_402_COMMAND_FAULT_RESET:
            value = 0x0080U;
            break;
        case CO_402_COMMAND_UNKNOWN:
        default:
            return false;
    }

    update->value = value;
    update->mask = CO_402_CONTROLLER_PDS_CONTROLWORD_MASK;
    update->valid = true;
    return true;
}

static CO_402_command_t commandForTarget(CO_402_state_t state,
                                         CO_402_controller_target_t target,
                                         bool *policyBlocked)
{
    *policyBlocked = false;

    switch (state) {
        case CO_402_STATE_NOT_READY_TO_SWITCH_ON:
            return CO_402_COMMAND_UNKNOWN;

        case CO_402_STATE_SWITCH_ON_DISABLED:
            if (target == CO_402_CONTROLLER_TARGET_READY_TO_SWITCH_ON
                || target == CO_402_CONTROLLER_TARGET_SWITCHED_ON
                || target == CO_402_CONTROLLER_TARGET_OPERATION_ENABLED) {
                return CO_402_COMMAND_SHUTDOWN;
            }
            break;

        case CO_402_STATE_READY_TO_SWITCH_ON:
            if (target == CO_402_CONTROLLER_TARGET_SWITCH_ON_DISABLED) {
                return CO_402_COMMAND_DISABLE_VOLTAGE;
            }
            if (target == CO_402_CONTROLLER_TARGET_SWITCHED_ON
                || target == CO_402_CONTROLLER_TARGET_OPERATION_ENABLED) {
                return CO_402_COMMAND_SWITCH_ON_OR_DISABLE_OPERATION;
            }
            break;

        case CO_402_STATE_SWITCHED_ON:
            if (target == CO_402_CONTROLLER_TARGET_SWITCH_ON_DISABLED) {
                return CO_402_COMMAND_DISABLE_VOLTAGE;
            }
            if (target == CO_402_CONTROLLER_TARGET_READY_TO_SWITCH_ON) {
                return CO_402_COMMAND_SHUTDOWN;
            }
            if (target == CO_402_CONTROLLER_TARGET_OPERATION_ENABLED) {
                return CO_402_COMMAND_ENABLE_OPERATION;
            }
            break;

        case CO_402_STATE_OPERATION_ENABLED:
            if (target == CO_402_CONTROLLER_TARGET_SWITCH_ON_DISABLED) {
                return CO_402_COMMAND_DISABLE_VOLTAGE;
            }
            if (target == CO_402_CONTROLLER_TARGET_READY_TO_SWITCH_ON) {
                return CO_402_COMMAND_SHUTDOWN;
            }
            if (target == CO_402_CONTROLLER_TARGET_SWITCHED_ON) {
                return CO_402_COMMAND_SWITCH_ON_OR_DISABLE_OPERATION;
            }
            if (target == CO_402_CONTROLLER_TARGET_QUICK_STOP_ACTIVE) {
                return CO_402_COMMAND_QUICK_STOP;
            }
            break;

        case CO_402_STATE_QUICK_STOP_ACTIVE:
            /* Recovery policy depends on the remote quick-stop option, which this transport-agnostic layer does not own. */
            if (target == CO_402_CONTROLLER_TARGET_SWITCH_ON_DISABLED) {
                return CO_402_COMMAND_DISABLE_VOLTAGE;
            }
            break;

        case CO_402_STATE_FAULT_REACTION_ACTIVE:
        case CO_402_STATE_FAULT:
        case CO_402_STATE_UNKNOWN:
        default:
            return CO_402_COMMAND_UNKNOWN;
    }

    *policyBlocked = true;
    return CO_402_COMMAND_UNKNOWN;
}

static void clearTargetRuntime(CO_402_controller_axis_t *axis)
{
    axis->targetValid = false;
    axis->transitionElapsed_us = 0U;
    axis->transitionFrom = CO_402_STATE_UNKNOWN;
    axis->pendingCommand = CO_402_COMMAND_UNKNOWN;
}

static CO_402_controller_result_t setResult(CO_402_controller_axis_t *axis,
                                             CO_402_controller_result_t result)
{
    axis->result = result;
    return result;
}

bool CO_402_controller_init(CO_402_controller_axis_t *axis,
                            const CO_402_controller_config_t *config)
{
    if (axis == NULL || config == NULL) {
        return false;
    }

    memset(axis, 0, sizeof(*axis));
    axis->config = *config;
    axis->remoteState = CO_402_STATE_UNKNOWN;
    axis->transitionFrom = CO_402_STATE_UNKNOWN;
    axis->pendingCommand = CO_402_COMMAND_UNKNOWN;
    axis->result = CO_402_CONTROLLER_RESULT_IDLE;
    return true;
}

void CO_402_controller_reset(CO_402_controller_axis_t *axis)
{
    CO_402_controller_config_t config;
    bool faultResetPulseHigh;

    if (axis == NULL) {
        return;
    }

    config = axis->config;
    faultResetPulseHigh = axis->faultResetPulseHigh;
    memset(axis, 0, sizeof(*axis));
    axis->config = config;
    axis->remoteState = CO_402_STATE_UNKNOWN;
    axis->transitionFrom = CO_402_STATE_UNKNOWN;
    axis->pendingCommand = CO_402_COMMAND_UNKNOWN;
    axis->result = CO_402_CONTROLLER_RESULT_IDLE;

    /* A previously published Fault Reset high still requires an explicit low update after runtime reset. */
    axis->faultResetPulseHigh = faultResetPulseHigh;
}

bool CO_402_controller_setTarget(CO_402_controller_axis_t *axis,
                                 CO_402_controller_target_t target)
{
    if (axis == NULL || !targetValid(target)) {
        return false;
    }
    if (axis->faultResetRequested || axis->faultResetPulseHigh
        || (axis->remoteStateValid
            && (axis->remoteState == CO_402_STATE_FAULT_REACTION_ACTIVE
                || axis->remoteState == CO_402_STATE_FAULT))) {
        return false;
    }

    /* Repeating the same target is not a timeout keepalive; preserve transition progress. */
    if (axis->targetValid && axis->target == target) {
        return true;
    }

    axis->target = target;
    axis->targetValid = true;
    axis->transitionElapsed_us = 0U;
    axis->transitionFrom = axis->remoteStateValid ? axis->remoteState : CO_402_STATE_UNKNOWN;
    axis->pendingCommand = CO_402_COMMAND_UNKNOWN;
    axis->result = CO_402_CONTROLLER_RESULT_IN_PROGRESS;
    return true;
}

void CO_402_controller_clearTarget(CO_402_controller_axis_t *axis)
{
    if (axis == NULL) {
        return;
    }

    clearTargetRuntime(axis);
    axis->result = CO_402_CONTROLLER_RESULT_IDLE;
}

bool CO_402_controller_requestFaultReset(CO_402_controller_axis_t *axis)
{
    if (axis == NULL || !axis->remoteStateValid || axis->remoteState != CO_402_STATE_FAULT
        || axis->faultResetRequested || axis->faultResetPulseHigh) {
        return false;
    }

    axis->faultResetRequested = true;
    return true;
}

CO_402_controller_result_t CO_402_controller_process(
    CO_402_controller_axis_t *axis,
    const CO_402_controller_feedback_t *feedback,
    uint32_t timeDifference_us,
    CO_402_controller_controlword_update_t *controlwordUpdate)
{
    CO_402_state_t decodedState;
    CO_402_state_t desiredState;
    CO_402_command_t command;
    bool policyBlocked;
    bool stateChanged = false;

    if (controlwordUpdate != NULL) {
        memset(controlwordUpdate, 0, sizeof(*controlwordUpdate));
    }
    if (axis == NULL || feedback == NULL || controlwordUpdate == NULL) {
        return axis != NULL ? setResult(axis, CO_402_CONTROLLER_RESULT_STATUS_UNKNOWN)
                            : CO_402_CONTROLLER_RESULT_STATUS_UNKNOWN;
    }

    if (feedback->statuswordValid) {
        axis->feedbackElapsed_us = 0U;
        axis->remoteStatusword = feedback->statusword;
        decodedState = CO_402_decodeStatusword(feedback->statusword);
        if (decodedState == CO_402_STATE_UNKNOWN) {
            axis->remoteState = CO_402_STATE_UNKNOWN;
            axis->remoteStateValid = false;
            axis->remoteStatuswordInvalid = true;
            axis->transitionElapsed_us = saturatingAddU32(axis->transitionElapsed_us, timeDifference_us);
        } else {
            stateChanged = !axis->remoteStateValid || axis->remoteState != decodedState;
            axis->remoteState = decodedState;
            axis->remoteStateValid = true;
            axis->remoteStatuswordInvalid = false;
            if (stateChanged) {
                axis->transitionElapsed_us = 0U;
                axis->transitionFrom = decodedState;
                axis->pendingCommand = CO_402_COMMAND_UNKNOWN;
            }
        }
    } else {
        axis->feedbackElapsed_us = saturatingAddU32(axis->feedbackElapsed_us, timeDifference_us);
    }

    /* Once Fault Reset is asserted, emit its explicit low update before reporting stale or unknown feedback. */
    if (axis->faultResetPulseHigh) {
        controlwordUpdate->value = 0x0000U;
        controlwordUpdate->mask = CO_402_CONTROLLER_PDS_CONTROLWORD_MASK;
        controlwordUpdate->valid = true;
        axis->faultResetPulseHigh = false;
        axis->pendingCommand = CO_402_COMMAND_DISABLE_VOLTAGE;

        if (axis->config.feedbackTimeout_us != CO_402_CONTROLLER_TIMEOUT_DISABLED
            && axis->feedbackElapsed_us >= axis->config.feedbackTimeout_us) {
            return setResult(axis, CO_402_CONTROLLER_RESULT_FEEDBACK_TIMEOUT);
        }
        if (axis->remoteStatuswordInvalid) {
            return setResult(axis, CO_402_CONTROLLER_RESULT_STATUS_UNKNOWN);
        }
        if (axis->remoteStateValid && axis->remoteState == CO_402_STATE_FAULT) {
            return setResult(axis, CO_402_CONTROLLER_RESULT_REMOTE_FAULT);
        }
        return setResult(axis, CO_402_CONTROLLER_RESULT_IDLE);
    }

    if (axis->config.feedbackTimeout_us != CO_402_CONTROLLER_TIMEOUT_DISABLED
        && axis->feedbackElapsed_us >= axis->config.feedbackTimeout_us) {
        return setResult(axis, CO_402_CONTROLLER_RESULT_FEEDBACK_TIMEOUT);
    }

    if (axis->remoteStatuswordInvalid) {
        return setResult(axis, CO_402_CONTROLLER_RESULT_STATUS_UNKNOWN);
    }

    if (!axis->remoteStateValid) {
        return setResult(axis, axis->targetValid ? CO_402_CONTROLLER_RESULT_IN_PROGRESS
                                                  : CO_402_CONTROLLER_RESULT_IDLE);
    }

    if (axis->remoteState == CO_402_STATE_FAULT_REACTION_ACTIVE) {
        /* Fault handling invalidates motion intent; the application must explicitly request motion again after recovery. */
        clearTargetRuntime(axis);
        axis->faultResetRequested = false;
        return setResult(axis, CO_402_CONTROLLER_RESULT_WAIT_FAULT_REACTION);
    }

    if (axis->remoteState == CO_402_STATE_FAULT) {
        clearTargetRuntime(axis);
        if (axis->faultResetRequested) {
            axis->faultResetRequested = false;
            axis->faultResetPulseHigh = true;
            axis->pendingCommand = CO_402_COMMAND_FAULT_RESET;
            (void)commandUpdate(CO_402_COMMAND_FAULT_RESET, controlwordUpdate);
            return setResult(axis, CO_402_CONTROLLER_RESULT_IN_PROGRESS);
        }
        return setResult(axis, CO_402_CONTROLLER_RESULT_REMOTE_FAULT);
    }

    axis->faultResetRequested = false;

    if (!axis->targetValid) {
        axis->transitionElapsed_us = 0U;
        axis->pendingCommand = CO_402_COMMAND_UNKNOWN;
        return setResult(axis, CO_402_CONTROLLER_RESULT_IDLE);
    }

    desiredState = targetState(axis->target);
    if (axis->remoteState == desiredState) {
        axis->transitionElapsed_us = 0U;
        axis->pendingCommand = CO_402_COMMAND_UNKNOWN;
        return setResult(axis, CO_402_CONTROLLER_RESULT_TARGET_REACHED);
    }

    command = commandForTarget(axis->remoteState, axis->target, &policyBlocked);
    if (policyBlocked) {
        axis->pendingCommand = CO_402_COMMAND_UNKNOWN;
        return setResult(axis, CO_402_CONTROLLER_RESULT_POLICY_BLOCKED);
    }

    if (!stateChanged) {
        axis->transitionElapsed_us = saturatingAddU32(axis->transitionElapsed_us, timeDifference_us);
    }
    if (axis->config.transitionTimeout_us != CO_402_CONTROLLER_TIMEOUT_DISABLED
        && axis->transitionElapsed_us >= axis->config.transitionTimeout_us) {
        axis->pendingCommand = CO_402_COMMAND_UNKNOWN;
        return setResult(axis, CO_402_CONTROLLER_RESULT_TRANSITION_TIMEOUT);
    }

    axis->pendingCommand = command;
    if (command != CO_402_COMMAND_UNKNOWN) {
        (void)commandUpdate(command, controlwordUpdate);
    }
    return setResult(axis, CO_402_CONTROLLER_RESULT_IN_PROGRESS);
}

CO_402_state_t CO_402_controller_getRemoteState(const CO_402_controller_axis_t *axis)
{
    if (axis == NULL || !axis->remoteStateValid) {
        return CO_402_STATE_UNKNOWN;
    }
    return axis->remoteState;
}

CO_402_controller_result_t CO_402_controller_getResult(const CO_402_controller_axis_t *axis)
{
    return axis != NULL ? axis->result : CO_402_CONTROLLER_RESULT_STATUS_UNKNOWN;
}

uint16_t CO_402_controller_applyControlwordUpdate(
    uint16_t currentControlword,
    const CO_402_controller_controlword_update_t *update)
{
    if (update == NULL || !update->valid) {
        return currentControlword;
    }

    return (uint16_t)((currentControlword & (uint16_t)(~update->mask))
                      | (update->value & update->mask));
}
