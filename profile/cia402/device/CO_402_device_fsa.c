/**
 * @file CO_402_device_fsa.c
 * @brief Non-blocking CiA 402 Device PDS state supervisor.
 */

#include "CO_402_device_fsa.h"
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

/*
 * Move an axis into the serialized fault-reaction state.
 *
 * Abort an accepted Fault Reset transaction here so an ERROR cannot be replayed
 * after fault reaction without observing a new Controlword bit-7 rising edge.
 */
static void setFaultReaction(CO_402_device_axis_t *axis)
{
    axis->faultResetInProgress = false;
    setState(axis, CO_402_STATE_FAULT_REACTION_ACTIVE);
}

/*
 * Apply one asynchronous DriveIF transition result to the PDS state.
 *
 * BUSY deliberately keeps the current state so the same transition can be
 * retried on the next supervisor cycle without blocking the caller.
 */
static bool applyTransition(CO_402_device_axis_t *axis, CO_402_drive_result_t result, CO_402_state_t targetState)
{
    if (result == CO_402_DRIVE_DONE) {
        setState(axis, targetState);
        return true;
    }
    if (result == CO_402_DRIVE_ERROR) {
        CO_402_LOG_E("CiA402 drive transition failed: axis=%u state=%u", (unsigned int)axis->logicalDevice,
                     (unsigned int)axis->state);
        /* Transition errors are owned by the PDS supervisor, which serializes fault reaction on later cycles. */
        setFaultReaction(axis);
    }

    return false;
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

/* Handle commands accepted while the axis is Switch on disabled. */
static void processSwitchOnDisabled(CO_402_device_axis_t *axis, CO_402_command_t command)
{
    if (command == CO_402_COMMAND_SHUTDOWN) {
        (void)applyTransition(axis, callDrive(axis, axis->drive->shutdown), CO_402_STATE_READY_TO_SWITCH_ON);
    }
}

/* Handle commands accepted while the axis is Ready to switch on. */
static void processReadyToSwitchOn(CO_402_device_axis_t *axis, CO_402_command_t command)
{
    if (command == CO_402_COMMAND_DISABLE_VOLTAGE) {
        (void)applyTransition(axis, callDrive(axis, axis->drive->disableVoltage), CO_402_STATE_SWITCH_ON_DISABLED);
    } else if (command == CO_402_COMMAND_SWITCH_ON_OR_DISABLE_OPERATION) {
        (void)applyTransition(axis, callDrive(axis, axis->drive->switchOn), CO_402_STATE_SWITCHED_ON);
    }
}

/* Handle commands accepted while the axis is Switched on. */
static void processSwitchedOn(CO_402_device_axis_t *axis, CO_402_command_t command)
{
    if (command == CO_402_COMMAND_DISABLE_VOLTAGE) {
        (void)applyTransition(axis, callDrive(axis, axis->drive->disableVoltage), CO_402_STATE_SWITCH_ON_DISABLED);
    } else if (command == CO_402_COMMAND_SHUTDOWN) {
        (void)applyTransition(axis, callDrive(axis, axis->drive->shutdown), CO_402_STATE_READY_TO_SWITCH_ON);
    } else if (command == CO_402_COMMAND_ENABLE_OPERATION) {
        (void)applyTransition(axis, callDrive(axis, axis->drive->enableOperation), CO_402_STATE_OPERATION_ENABLED);
    }
}

/* Handle commands accepted while the axis is Operation enabled. */
static void processOperationEnabled(CO_402_device_axis_t *axis, CO_402_command_t command)
{
    if (command == CO_402_COMMAND_QUICK_STOP) {
        (void)applyTransition(axis, callDrive(axis, axis->drive->quickStop), CO_402_STATE_QUICK_STOP_ACTIVE);
    } else if (command == CO_402_COMMAND_DISABLE_VOLTAGE) {
        (void)applyTransition(axis, callDrive(axis, axis->drive->disableVoltage), CO_402_STATE_SWITCH_ON_DISABLED);
    } else if (command == CO_402_COMMAND_SHUTDOWN) {
        (void)applyTransition(axis, callDrive(axis, axis->drive->shutdown), CO_402_STATE_READY_TO_SWITCH_ON);
    } else if (command == CO_402_COMMAND_SWITCH_ON_OR_DISABLE_OPERATION) {
        (void)applyTransition(axis, callDrive(axis, axis->drive->disableOperation), CO_402_STATE_SWITCHED_ON);
    }
}

/*
 * Handle commands accepted while the axis is Quick stop active.
 *
 * A3 does not bind the Quick stop option code, so it cannot select a normative
 * automatic recovery policy. Only Disable voltage is accepted here; later
 * stages may add recovery after the corresponding OD/policy contract exists.
 */
static void processQuickStopActive(CO_402_device_axis_t *axis, CO_402_command_t command)
{
    if (command == CO_402_COMMAND_DISABLE_VOLTAGE) {
        (void)applyTransition(axis, callDrive(axis, axis->drive->disableVoltage), CO_402_STATE_SWITCH_ON_DISABLED);
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

    /* A failed Controlword snapshot is treated as a local control-path failure and enters fault reaction. */
    if (OD_get_u16(axis->od.controlword, 0U, &controlword, true) != ODR_OK) {
        CO_402_LOG_E("CiA402 Controlword read failed: axis=%u", (unsigned int)axis->logicalDevice);
        setFaultReaction(axis);
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
