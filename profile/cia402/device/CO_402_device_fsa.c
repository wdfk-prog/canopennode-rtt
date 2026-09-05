/**
 * @file CO_402_device_fsa.c
 * @brief Non-blocking CiA 402 Device PDS state supervisor.
 */

#include "CO_402_device_fsa.h"
#include "CO_402_device_internal.h"
#include "CO_402_log.h"

#define CO_402_CONTROLWORD_FAULT_RESET 0x0080U

/* Commit one symbolic PDS state and report actual state changes through the optional log hook. */
static void setState(CO_402_device_axis_t *axis, CO_402_state_t targetState)
{
    if (axis->state != targetState) {
        CO_402_LOG_I("CiA402 state changed: axis=%u from=%u to=%u", (unsigned int)axis->logicalDevice,
                     (unsigned int)axis->state, (unsigned int)targetState);
        axis->state = targetState;
    }
}

/* Safety actions can transfer DriveIF ownership only at supervisor callback boundaries. */
typedef enum {
    CO_402_SAFETY_PRIORITY_NONE = 0,
    CO_402_SAFETY_PRIORITY_QUICK_STOP,
    CO_402_SAFETY_PRIORITY_DISABLE_VOLTAGE
} CO_402_safety_priority_t;

/*
 * Retire the software ownership token before a higher-priority safety callback starts.
 *
 * No two callbacks execute concurrently. The incoming safety callback is responsible
 * for synchronously superseding any physical action left BUSY by the retired owner.
 */
static void releaseOwnerForSafetyTransfer(CO_402_device_axis_t *axis)
{
    axis->pdsTransitionOperation = NULL;
    axis->pdsTransitionTargetState = CO_402_STATE_UNKNOWN;
    axis->faultResetInProgress = false;
    axis->pendingExitMode = CO_402_MODE_NONE;
    axis->pendingEnterMode = CO_402_MODE_NONE;
}

/* Fault reaction is the highest-priority safety owner and cannot wait behind BUSY work. */
static void setFaultReaction(CO_402_device_axis_t *axis)
{
    releaseOwnerForSafetyTransfer(axis);
    setState(axis, CO_402_STATE_FAULT_REACTION_ACTIVE);
}

/* Invoke one DriveIF transition callback through the axis-owned drive object. */
static CO_402_drive_result_t callDrive(CO_402_device_axis_t *axis, CO_402_drive_result_t (*operation)(void *))
{
    if (operation == NULL) {
        CO_402_LOG_E("CiA402 DriveIF callback missing: axis=%u state=%u", (unsigned int)axis->logicalDevice,
                     (unsigned int)axis->state);
        return CO_402_DRIVE_ERROR;
    }

    return operation(axis->driveObject);
}

/*
 * Continue the callback latched by the first accepted PDS command.
 *
 * While BUSY, the callback pointer is the ownership token: ordinary Controlword
 * commands, operation-mode callbacks and the synchronous fast path must not
 * overtake it. Only the explicit safety-transfer path may retire this token.
 */
static void continuePdsTransition(CO_402_device_axis_t *axis)
{
    CO_402_drive_result_t result;
    CO_402_state_t targetState;

    if (axis->pdsTransitionOperation == NULL) {
        return;
    }

    targetState = axis->pdsTransitionTargetState;
    result = callDrive(axis, axis->pdsTransitionOperation);
    if (result == CO_402_DRIVE_BUSY) {
        return;
    }

    axis->pdsTransitionOperation = NULL;
    axis->pdsTransitionTargetState = CO_402_STATE_UNKNOWN;
    if (result == CO_402_DRIVE_DONE) {
        setState(axis, targetState);
        return;
    }

    CO_402_LOG_E("CiA402 drive transition failed: axis=%u state=%u", (unsigned int)axis->logicalDevice,
                 (unsigned int)axis->state);
    setFaultReaction(axis);
}

/* Latch one PDS owner before its first callback; only a stricter safety request may replace it while BUSY. */
static void startPdsTransition(CO_402_device_axis_t *axis,
                               CO_402_drive_result_t (*operation)(void *),
                               CO_402_state_t targetState)
{
    axis->pdsTransitionOperation = operation;
    axis->pdsTransitionTargetState = targetState;
    continuePdsTransition(axis);
}

static void processFaultReactionActive(CO_402_device_axis_t *axis);

/* Classify the latched PDS target, not callback identity: products may reuse one function for multiple actions. */
static CO_402_safety_priority_t activePdsSafetyPriority(const CO_402_device_axis_t *axis)
{
    if (axis->pdsTransitionTargetState == CO_402_STATE_SWITCH_ON_DISABLED) {
        return CO_402_SAFETY_PRIORITY_DISABLE_VOLTAGE;
    }
    if (axis->pdsTransitionTargetState == CO_402_STATE_QUICK_STOP_ACTIVE) {
        return CO_402_SAFETY_PRIORITY_QUICK_STOP;
    }
    return CO_402_SAFETY_PRIORITY_NONE;
}

/*
 * Transfer one BUSY owner to a stricter Controlword safety request.
 *
 * Ordinary state/mode requests remain serialized behind the active owner. Quick-stop
 * may also interrupt an in-progress Enable operation whose target is Operation enabled.
 */
static bool preemptOwnerForSafetyCommand(CO_402_device_axis_t *axis, CO_402_command_t command)
{
    CO_402_drive_result_t (*operation)(void *) = NULL;
    CO_402_state_t targetState = CO_402_STATE_UNKNOWN;
    CO_402_safety_priority_t requestedPriority = CO_402_SAFETY_PRIORITY_NONE;
    CO_402_safety_priority_t activePriority;
    bool ownerActive;

    if (command == CO_402_COMMAND_DISABLE_VOLTAGE) {
        operation = axis->drive->disableVoltage;
        targetState = CO_402_STATE_SWITCH_ON_DISABLED;
        requestedPriority = CO_402_SAFETY_PRIORITY_DISABLE_VOLTAGE;
    } else if (command == CO_402_COMMAND_QUICK_STOP
               && (axis->state == CO_402_STATE_OPERATION_ENABLED
                   || axis->pdsTransitionTargetState == CO_402_STATE_OPERATION_ENABLED)) {
        operation = axis->drive->quickStop;
        targetState = CO_402_STATE_QUICK_STOP_ACTIVE;
        requestedPriority = CO_402_SAFETY_PRIORITY_QUICK_STOP;
    } else {
        return false;
    }

    /* Fault Reset is preemptible only by fault reaction, never by a normal Controlword state command. */
    ownerActive = axis->pdsTransitionOperation != NULL || CO_402_device_modeTransitionPending(axis);
    if (!ownerActive) {
        return false;
    }

    activePriority = activePdsSafetyPriority(axis);
    if (activePriority >= requestedPriority) {
        return false;
    }

    CO_402_LOG_I("CiA402 safety owner transfer: axis=%u command=%u",
                 (unsigned int)axis->logicalDevice, (unsigned int)command);
    releaseOwnerForSafetyTransfer(axis);
    startPdsTransition(axis, operation, targetState);
    if (axis->state == CO_402_STATE_FAULT_REACTION_ACTIVE) {
        /* A failed takeover cannot leave the retired physical action uncontrolled until the next supervisor tick. */
        processFaultReactionActive(axis);
    }
    return true;
}

/* Handle commands accepted while the axis is Switch on disabled. */
static void processSwitchOnDisabled(CO_402_device_axis_t *axis, CO_402_command_t command)
{
    if (command == CO_402_COMMAND_SHUTDOWN) {
        startPdsTransition(axis, axis->drive->shutdown, CO_402_STATE_READY_TO_SWITCH_ON);
    }
}

/* Handle commands accepted while the axis is Ready to switch on. */
static void processReadyToSwitchOn(CO_402_device_axis_t *axis, CO_402_command_t command)
{
    if (command == CO_402_COMMAND_DISABLE_VOLTAGE) {
        startPdsTransition(axis, axis->drive->disableVoltage, CO_402_STATE_SWITCH_ON_DISABLED);
    } else if (command == CO_402_COMMAND_SWITCH_ON_OR_DISABLE_OPERATION) {
        startPdsTransition(axis, axis->drive->switchOn, CO_402_STATE_SWITCHED_ON);
    }
}

/* Handle commands accepted while the axis is Switched on. */
static void processSwitchedOn(CO_402_device_axis_t *axis, CO_402_command_t command)
{
    if (command == CO_402_COMMAND_DISABLE_VOLTAGE) {
        startPdsTransition(axis, axis->drive->disableVoltage, CO_402_STATE_SWITCH_ON_DISABLED);
    } else if (command == CO_402_COMMAND_SHUTDOWN) {
        startPdsTransition(axis, axis->drive->shutdown, CO_402_STATE_READY_TO_SWITCH_ON);
    } else if (command == CO_402_COMMAND_ENABLE_OPERATION) {
        startPdsTransition(axis, axis->drive->enableOperation, CO_402_STATE_OPERATION_ENABLED);
    }
}

/* Handle commands accepted while the axis is Operation enabled. */
static void processOperationEnabled(CO_402_device_axis_t *axis, CO_402_command_t command)
{
    if (command == CO_402_COMMAND_QUICK_STOP) {
        startPdsTransition(axis, axis->drive->quickStop, CO_402_STATE_QUICK_STOP_ACTIVE);
    } else if (command == CO_402_COMMAND_DISABLE_VOLTAGE) {
        startPdsTransition(axis, axis->drive->disableVoltage, CO_402_STATE_SWITCH_ON_DISABLED);
    } else if (command == CO_402_COMMAND_SHUTDOWN) {
        startPdsTransition(axis, axis->drive->shutdown, CO_402_STATE_READY_TO_SWITCH_ON);
    } else if (command == CO_402_COMMAND_SWITCH_ON_OR_DISABLE_OPERATION) {
        startPdsTransition(axis, axis->drive->disableOperation, CO_402_STATE_SWITCHED_ON);
    }
}

/*
 * Handle commands accepted while the axis is Quick stop active.
 *
 * The current Device core does not bind the Quick stop option code, so it cannot select a normative
 * automatic recovery policy. Only Disable voltage is accepted here until the
 * corresponding OD and product recovery policy are explicitly available.
 */
static void processQuickStopActive(CO_402_device_axis_t *axis, CO_402_command_t command)
{
    if (command == CO_402_COMMAND_DISABLE_VOLTAGE) {
        startPdsTransition(axis, axis->drive->disableVoltage, CO_402_STATE_SWITCH_ON_DISABLED);
    }
}

/* Execute the non-blocking drive fault reaction until the axis reaches Fault. */
static void processFaultReactionActive(CO_402_device_axis_t *axis)
{
    CO_402_drive_result_t result = callDrive(axis, axis->drive->faultReaction);

    if (result == CO_402_DRIVE_DONE || result == CO_402_DRIVE_ERROR) {
        if (result == CO_402_DRIVE_ERROR) {
            CO_402_LOG_E("CiA402 fault reaction failed closed: axis=%u", (unsigned int)axis->logicalDevice);
        }
        /* A failed fault-reaction callback must fail closed in Fault rather than retry an unsafe action forever. */
        setState(axis, CO_402_STATE_FAULT);
    }
}

/*
 * Run one edge-triggered Fault Reset transaction.
 *
 * A 0->1 edge starts the transaction only while the axis is Fault. Once accepted,
 * BUSY keeps the transaction latched so the non-blocking DriveIF can finish across
 * later supervisor cycles without requiring another Controlword edge.
 */
static void processFault(CO_402_device_axis_t *axis, bool faultResetRisingEdge)
{
    CO_402_drive_result_t result;

    if (!axis->faultResetInProgress) {
        if (!faultResetRisingEdge) {
            return;
        }

        axis->faultResetInProgress = true;
        CO_402_LOG_I("CiA402 fault reset edge accepted: axis=%u", (unsigned int)axis->logicalDevice);
    }

    result = callDrive(axis, axis->drive->faultReset);
    if (result == CO_402_DRIVE_BUSY) {
        return;
    }

    axis->faultResetInProgress = false;
    if (result == CO_402_DRIVE_DONE) {
        CO_402_LOG_I("CiA402 fault reset completed: axis=%u", (unsigned int)axis->logicalDevice);
        setState(axis, CO_402_STATE_SWITCH_ON_DISABLED);
        return;
    }

    CO_402_LOG_E("CiA402 fault reset failed: axis=%u", (unsigned int)axis->logicalDevice);
    setFaultReaction(axis);
}

void CO_402_device_axisControlwordReadFailed(CO_402_device_axis_t *axis)
{
    CO_402_state_t stateBefore;

    if (axis == NULL || axis->drive == NULL) {
        return;
    }

    CO_402_LOG_E("CiA402 Controlword read failed: axis=%u", (unsigned int)axis->logicalDevice);
    stateBefore = axis->state;

    /* Once Fault is reached, an unreadable Controlword cannot authorize reset; remain failed closed. */
    if (stateBefore == CO_402_STATE_FAULT && !axis->faultResetInProgress
        && axis->pdsTransitionOperation == NULL && !CO_402_device_modeTransitionPending(axis)) {
        return;
    }

    /* Fault reaction supersedes any BUSY PDS, Fault Reset, or mode owner at this callback boundary. */
    setFaultReaction(axis);
    processFaultReactionActive(axis);
}

void CO_402_device_axisProcess(CO_402_device_axis_t *axis)
{
    uint16_t controlword = 0U;
    CO_402_command_t command;
    bool faultResetBit;
    bool faultResetRisingEdge;

    /* OD binding and DriveIF ownership must be complete before the FSA can run. */
    if (axis == NULL || axis->drive == NULL || axis->od.controlword == NULL || axis->od.statusword == NULL) {
        return;
    }

    /* A failed Controlword snapshot transfers any in-flight owner directly to fault reaction. */
    if (OD_get_u16(axis->od.controlword, 0U, &controlword, true) != ODR_OK) {
        CO_402_device_axisControlwordReadFailed(axis);
        return;
    }

    /*
     * Fault Reset is an observed edge, not a level command. Track bit 7 on every
     * successful snapshot so an edge seen before Fault cannot be replayed after
     * a later fault. Normal state commands are decoded with bit 7 ignored.
     */
    faultResetBit = (controlword & CO_402_CONTROLWORD_FAULT_RESET) != 0U;
    faultResetRisingEdge = faultResetBit && !axis->faultResetBitPrevious;
    axis->faultResetBitPrevious = faultResetBit;
    command = CO_402_decodeControlword((uint16_t)(controlword & (uint16_t)~CO_402_CONTROLWORD_FAULT_RESET));

    if (faultResetRisingEdge && axis->state != CO_402_STATE_FAULT) {
        CO_402_LOG_D("CiA402 fault reset edge ignored outside Fault: axis=%u state=%u",
                     (unsigned int)axis->logicalDevice, (unsigned int)axis->state);
    }

    /*
     * BUSY mode/PDS/Fault-Reset callbacks keep exclusive ownership for ordinary
     * requests. A stricter Quick-stop or Disable-voltage request transfers that
     * ownership before the old callback is polled again.
     */
    if (preemptOwnerForSafetyCommand(axis, command)) {
        return;
    }
    if (CO_402_device_modeTransitionPending(axis)) {
        return;
    }
    if (axis->pdsTransitionOperation != NULL) {
        continuePdsTransition(axis);
        return;
    }

    /* Exactly one state handler runs per supervisor cycle, preserving asynchronous DriveIF sequencing. */
    switch (axis->state) {
        case CO_402_STATE_NOT_READY_TO_SWITCH_ON:
            /* The Pure-C core has completed its own initialization once OD binding succeeded. */
            setState(axis, CO_402_STATE_SWITCH_ON_DISABLED);
            break;
        case CO_402_STATE_SWITCH_ON_DISABLED:
            processSwitchOnDisabled(axis, command);
            break;
        case CO_402_STATE_READY_TO_SWITCH_ON:
            processReadyToSwitchOn(axis, command);
            break;
        case CO_402_STATE_SWITCHED_ON:
            processSwitchedOn(axis, command);
            break;
        case CO_402_STATE_OPERATION_ENABLED:
            processOperationEnabled(axis, command);
            break;
        case CO_402_STATE_QUICK_STOP_ACTIVE:
            processQuickStopActive(axis, command);
            break;
        case CO_402_STATE_FAULT_REACTION_ACTIVE:
            processFaultReactionActive(axis);
            break;
        case CO_402_STATE_FAULT:
            processFault(axis, faultResetRisingEdge);
            break;
        case CO_402_STATE_UNKNOWN:
        default:
            /* Unknown state is not recoverable locally; enter the common fault-reaction path. */
            CO_402_LOG_E("CiA402 invalid PDS state: axis=%u state=%u", (unsigned int)axis->logicalDevice,
                         (unsigned int)axis->state);
            setFaultReaction(axis);
            break;
    }
}
