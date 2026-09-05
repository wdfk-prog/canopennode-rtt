/**
 * @file CO_402_device_RTT_demo.c
 * @brief Software-only CiA 402 Device factory for RT-Thread protocol validation.
 */

#define LOG_TAG "canopen.402.demo"
#define LOG_LVL LOG_LVL_DBG

#include "CO_402_device_RTT.h"
#if CO_402_CONFIG_DIAGNOSTICS
#include "301/CO_Emergency.h"
#endif /* CO_402_CONFIG_DIAGNOSTICS */
#include "co_rtt_log.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#if (PKG_CANOPENNODE_CIA402_DEMO_AXIS_COUNT < 1) || (PKG_CANOPENNODE_CIA402_DEMO_AXIS_COUNT > 3)
#error "PKG_CANOPENNODE_CIA402_DEMO_AXIS_COUNT must be in the generated demo OD range 1..3"
#endif /* (PKG_CANOPENNODE_CIA402_DEMO_AXIS_COUNT < 1) || (PKG_CANOPENNODE_CIA402_DEMO_AXIS_COUNT > 3) */

/* Software-only state that substitutes for one physical motor/drive in the package demo. */
typedef struct {
    uint8_t axis;
    CO_402_mode_t mode;
    int32_t position;
    int32_t velocity;
    int16_t torque;
    int32_t ppTarget;
    bool ppActive;
    bool ppHalted;
    bool hmActive;
    bool hmHalted;
    bool pvCommandValid;
    CO_402_profile_velocity_command_t lastPvCommand;
#if CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST
    CO_402_sync_feedback_t syncFeedback;
    bool syncCommandValid;
#endif /* CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST */
} CO_402_demo_axis_t;

/* Product-owned software feedback intentionally survives CANopen Communication Reset like a physical drive. */
static CO_402_demo_axis_t CO_402_demoAxes[] = {
    {.axis = 0U},
    {.axis = 1U},
    {.axis = 2U},
};

static const char *CO_402_demoModeName(CO_402_mode_t mode)
{
    switch (mode) {
        case CO_402_MODE_PROFILE_POSITION:
            return "PP";
        case CO_402_MODE_PROFILE_VELOCITY:
            return "PV";
        case CO_402_MODE_HOMING:
            return "HM";
        case CO_402_MODE_CYCLIC_SYNC_POSITION:
            return "CSP";
        case CO_402_MODE_CYCLIC_SYNC_VELOCITY:
            return "CSV";
        case CO_402_MODE_CYCLIC_SYNC_TORQUE:
            return "CST";
        case CO_402_MODE_NONE:
            return "NONE";
        default:
            return "OTHER";
    }
}

static void CO_402_demoStopMotion(CO_402_demo_axis_t *axis)
{
    axis->velocity = 0;
    axis->ppActive = false;
    axis->ppHalted = false;
    axis->hmActive = false;
    axis->hmHalted = false;
    axis->torque = 0;
#if CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST
    axis->syncCommandValid = false;
#endif /* CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST */
}

static CO_402_drive_result_t CO_402_demoPdsDone(void *object, const char *action, bool stopMotion)
{
    CO_402_demo_axis_t *axis = (CO_402_demo_axis_t *)object;

    if (axis == NULL) {
        return CO_402_DRIVE_ERROR;
    }
    if (stopMotion) {
        CO_402_demoStopMotion(axis);
    }

    CO_RTT_LOG_I("axis=%u PDS %s", (unsigned int)axis->axis, action);
    return CO_402_DRIVE_DONE;
}

static CO_402_drive_result_t CO_402_demoShutdown(void *object)
{
    return CO_402_demoPdsDone(object, "shutdown", true);
}

static CO_402_drive_result_t CO_402_demoSwitchOn(void *object)
{
    return CO_402_demoPdsDone(object, "switch-on", false);
}

static CO_402_drive_result_t CO_402_demoEnableOperation(void *object)
{
    return CO_402_demoPdsDone(object, "enable-operation", false);
}

static CO_402_drive_result_t CO_402_demoDisableOperation(void *object)
{
    return CO_402_demoPdsDone(object, "disable-operation", true);
}

static CO_402_drive_result_t CO_402_demoQuickStop(void *object)
{
    return CO_402_demoPdsDone(object, "quick-stop", true);
}

static CO_402_drive_result_t CO_402_demoFaultReaction(void *object)
{
    return CO_402_demoPdsDone(object, "fault-reaction", true);
}

static CO_402_drive_result_t CO_402_demoFaultReset(void *object)
{
    return CO_402_demoPdsDone(object, "fault-reset", true);
}

static CO_402_drive_result_t CO_402_demoDisableVoltage(void *object)
{
    return CO_402_demoPdsDone(object, "disable-voltage", true);
}

static int32_t CO_402_demoGetPosition(void *object)
{
    CO_402_demo_axis_t *axis = (CO_402_demo_axis_t *)object;
    return axis != NULL ? axis->position : 0;
}

static int32_t CO_402_demoGetVelocity(void *object)
{
    CO_402_demo_axis_t *axis = (CO_402_demo_axis_t *)object;
    return axis != NULL ? axis->velocity : 0;
}

static int16_t CO_402_demoGetTorque(void *object)
{
    CO_402_demo_axis_t *axis = (CO_402_demo_axis_t *)object;
    return axis != NULL ? axis->torque : 0;
}

static CO_402_drive_result_t CO_402_demoModeEnter(void *object, CO_402_mode_t mode)
{
    CO_402_demo_axis_t *axis = (CO_402_demo_axis_t *)object;

    if (axis == NULL) {
        return CO_402_DRIVE_ERROR;
    }

    axis->mode = mode;
    axis->pvCommandValid = false;
#if CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST
    axis->syncCommandValid = false;
#endif /* CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST */
    CO_RTT_LOG_I("axis=%u mode-enter %s(%d)", (unsigned int)axis->axis,
                 CO_402_demoModeName(mode), (int)mode);
    return CO_402_DRIVE_DONE;
}

static CO_402_drive_result_t CO_402_demoModeExit(void *object, CO_402_mode_t mode)
{
    CO_402_demo_axis_t *axis = (CO_402_demo_axis_t *)object;

    if (axis == NULL) {
        return CO_402_DRIVE_ERROR;
    }

    CO_402_demoStopMotion(axis);
    axis->mode = CO_402_MODE_NONE;
    axis->pvCommandValid = false;
    CO_RTT_LOG_I("axis=%u mode-exit %s(%d)", (unsigned int)axis->axis,
                 CO_402_demoModeName(mode), (int)mode);
    return CO_402_DRIVE_DONE;
}

static bool CO_402_demoResolvePositionTarget(CO_402_demo_axis_t *axis,
                                             const CO_402_profile_position_command_t *command,
                                             int32_t *resolvedTarget)
{
    int64_t target;

    if (axis == NULL || command == NULL || resolvedTarget == NULL) {
        return false;
    }

    target = command->relative ? ((int64_t)axis->position + (int64_t)command->targetPosition)
                               : (int64_t)command->targetPosition;
    if (target < INT32_MIN || target > INT32_MAX) {
        return false;
    }

    *resolvedTarget = (int32_t)target;
    return true;
}

static int32_t CO_402_demoProfileVelocityFeedback(uint32_t magnitude, int32_t current, int32_t target)
{
    int32_t velocity = magnitude > (uint32_t)INT32_MAX ? INT32_MAX : (int32_t)magnitude;

    if (target < current) {
        velocity = -velocity;
    } else if (target == current) {
        velocity = 0;
    }

    return velocity;
}

static CO_402_drive_result_t CO_402_demoProfilePosition(
    void *object, const CO_402_profile_position_command_t *command)
{
    CO_402_demo_axis_t *axis = (CO_402_demo_axis_t *)object;

    if (axis == NULL || command == NULL) {
        return CO_402_DRIVE_ERROR;
    }

    if (command->newSetPoint) {
        if (!CO_402_demoResolvePositionTarget(axis, command, &axis->ppTarget)) {
            CO_RTT_LOG_E("axis=%u PP target overflow: current=%ld request=%ld relative=%u",
                         (unsigned int)axis->axis, (long)axis->position,
                         (long)command->targetPosition, command->relative ? 1U : 0U);
            return CO_402_DRIVE_ERROR;
        }

        axis->ppActive = true;
        axis->ppHalted = false;
        axis->velocity = command->halt
            ? 0
            : CO_402_demoProfileVelocityFeedback(command->profileVelocity, axis->position, axis->ppTarget);
        CO_RTT_LOG_I("axis=%u PP accept target=%ld resolved=%ld vel=%lu acc=%lu dec=%lu rel=%u imm=%u halt=%u",
                     (unsigned int)axis->axis, (long)command->targetPosition, (long)axis->ppTarget,
                     (unsigned long)command->profileVelocity, (unsigned long)command->profileAcceleration,
                     (unsigned long)command->profileDeceleration, command->relative ? 1U : 0U,
                     command->changeImmediately ? 1U : 0U, command->halt ? 1U : 0U);
    }

    if (!axis->ppActive) {
        return CO_402_DRIVE_DONE;
    }

    if (command->halt) {
        axis->velocity = 0;
        if (!axis->ppHalted) {
            axis->ppHalted = true;
            CO_RTT_LOG_I("axis=%u PP halted at position=%ld", (unsigned int)axis->axis,
                         (long)axis->position);
        }
        return CO_402_DRIVE_BUSY;
    }

    if (command->newSetPoint) {
        /* Keep one BUSY supervisor interval visible so the protocol path can exercise in-flight ownership. */
        return CO_402_DRIVE_BUSY;
    }

    axis->position = axis->ppTarget;
    axis->velocity = 0;
    axis->ppActive = false;
    axis->ppHalted = false;
    CO_RTT_LOG_I("axis=%u PP complete position=%ld", (unsigned int)axis->axis, (long)axis->position);
    return CO_402_DRIVE_DONE;
}

static bool CO_402_demoPvChanged(const CO_402_demo_axis_t *axis,
                                 const CO_402_profile_velocity_command_t *command)
{
    const CO_402_profile_velocity_command_t *last = &axis->lastPvCommand;

    return !axis->pvCommandValid || last->targetVelocity != command->targetVelocity
           || last->profileAcceleration != command->profileAcceleration
           || last->profileDeceleration != command->profileDeceleration
           || last->quickStopDeceleration != command->quickStopDeceleration
           || last->halt != command->halt;
}

static CO_402_drive_result_t CO_402_demoProfileVelocity(
    void *object, const CO_402_profile_velocity_command_t *command)
{
    CO_402_demo_axis_t *axis = (CO_402_demo_axis_t *)object;

    if (axis == NULL || command == NULL) {
        return CO_402_DRIVE_ERROR;
    }

    if (CO_402_demoPvChanged(axis, command)) {
        CO_RTT_LOG_I("axis=%u PV apply target=%ld acc=%lu dec=%lu halt=%u",
                     (unsigned int)axis->axis, (long)command->targetVelocity,
                     (unsigned long)command->profileAcceleration,
                     (unsigned long)command->profileDeceleration, command->halt ? 1U : 0U);
        axis->lastPvCommand = *command;
        axis->pvCommandValid = true;
    }

    axis->velocity = command->halt ? 0 : command->targetVelocity;
    return CO_402_DRIVE_DONE;
}

static CO_402_drive_result_t CO_402_demoHoming(void *object, const CO_402_homing_command_t *command)
{
    CO_402_demo_axis_t *axis = (CO_402_demo_axis_t *)object;

    if (axis == NULL || command == NULL) {
        return CO_402_DRIVE_ERROR;
    }

    if (command->startEdge) {
        axis->hmActive = true;
        axis->hmHalted = false;
        CO_RTT_LOG_I("axis=%u HM start method=%d offset=%ld speed-switch=%lu speed-zero=%lu acc=%lu",
                     (unsigned int)axis->axis, (int)command->homingMethod, (long)command->homeOffset,
                     (unsigned long)command->speedSwitch, (unsigned long)command->speedZero,
                     (unsigned long)command->acceleration);
    }

    if (!axis->hmActive) {
        return CO_402_DRIVE_DONE;
    }

    if (!command->start) {
        axis->hmActive = false;
        axis->hmHalted = false;
        axis->velocity = 0;
        CO_RTT_LOG_I("axis=%u HM abort acknowledged at position=%ld",
                     (unsigned int)axis->axis, (long)axis->position);
        return CO_402_DRIVE_DONE;
    }

    if (command->halt) {
        axis->velocity = 0;
        if (!axis->hmHalted) {
            axis->hmHalted = true;
            CO_RTT_LOG_I("axis=%u HM halted at position=%ld", (unsigned int)axis->axis,
                         (long)axis->position);
        }
        return CO_402_DRIVE_BUSY;
    }

    if (command->startEdge) {
        /* Match PP: expose one BUSY interval before publishing deterministic completion feedback. */
        return CO_402_DRIVE_BUSY;
    }

    axis->position = command->homeOffset;
    axis->velocity = 0;
    axis->hmActive = false;
    axis->hmHalted = false;
    CO_RTT_LOG_I("axis=%u HM complete home-position=%ld", (unsigned int)axis->axis,
                 (long)axis->position);
    return CO_402_DRIVE_DONE;
}

#if CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST
/*
 * Apply one coherent cyclic command without logging. This callback runs from co_rt;
 * optional demo tracing is deferred to the lower-priority co_402 worker.
 */
static bool CO_402_demoSyncPublishCommand(void *object, const CO_402_sync_command_t *command)
{
    CO_402_demo_axis_t *axis = (CO_402_demo_axis_t *)object;

    if (axis == NULL || command == NULL || axis->mode != command->mode) {
        return false;
    }

    switch (command->mode) {
#if CO_402_CONFIG_MODE_CSP
        case CO_402_MODE_CYCLIC_SYNC_POSITION:
            axis->position = command->targetPosition;
            axis->velocity = 0;
            axis->torque = 0;
            break;
#endif /* CO_402_CONFIG_MODE_CSP */
#if CO_402_CONFIG_MODE_CSV
        case CO_402_MODE_CYCLIC_SYNC_VELOCITY:
            axis->velocity = command->targetVelocity;
            axis->torque = 0;
            break;
#endif /* CO_402_CONFIG_MODE_CSV */
#if CO_402_CONFIG_MODE_CST
        case CO_402_MODE_CYCLIC_SYNC_TORQUE:
            axis->velocity = 0;
            axis->torque = command->targetTorque;
            break;
#endif /* CO_402_CONFIG_MODE_CST */
        case CO_402_MODE_NONE:
        default:
            return false;
    }

    axis->syncFeedback.sequence = command->sequence;
    axis->syncFeedback.positionActual = axis->position;
    axis->syncFeedback.velocityActual = axis->velocity;
    axis->syncFeedback.torqueActual = axis->torque;
    axis->syncFeedback.driveFollowsCommand = true;
    axis->syncCommandValid = true;
    return true;
}

/* Return only a complete feedback generation previously produced by publishCommand(). */
static bool CO_402_demoSyncReadFeedback(void *object, CO_402_sync_feedback_t *feedback)
{
    CO_402_demo_axis_t *axis = (CO_402_demo_axis_t *)object;

    if (axis == NULL || feedback == NULL || !axis->syncCommandValid) {
        return false;
    }

    *feedback = axis->syncFeedback;
    return true;
}

/* Software SyncIF advertises only cyclic modes enabled by this build. */
static const CO_402_device_sync_if_t CO_402_demoSyncIf = {
    .supportedModes =
#if CO_402_CONFIG_MODE_CSP
        CO_402_SUPPORTED_MODE_CSP |
#endif /* CO_402_CONFIG_MODE_CSP */
#if CO_402_CONFIG_MODE_CSV
        CO_402_SUPPORTED_MODE_CSV |
#endif /* CO_402_CONFIG_MODE_CSV */
#if CO_402_CONFIG_MODE_CST
        CO_402_SUPPORTED_MODE_CST |
#endif /* CO_402_CONFIG_MODE_CST */
        0U,
    .publishCommand = CO_402_demoSyncPublishCommand,
    .readFeedback = CO_402_demoSyncReadFeedback,
};
#endif /* CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST */

#if CO_402_CONFIG_DIAGNOSTICS
/*
 * Demo-only device-specific mappings exercise the A7 transport contract without
 * pretending to define a product fault taxonomy. Real products must replace
 * these values with their selected CiA 301/402/product fault matrix.
 */
static bool CO_402_demoGetFaultInfo(void *object, CO_402_device_fault_origin_t origin,
                                    CO_402_device_fault_info_t *info)
{
    CO_402_demo_axis_t *axis = (CO_402_demo_axis_t *)object;

    if (axis == NULL || info == NULL || origin == CO_402_FAULT_ORIGIN_PRODUCT) {
        return false;
    }

    info->pdsErrorCode = (uint16_t)(0xFF10U + (uint16_t)origin);
    info->emcyCode = CO_EMC_DEVICE_SPECIFIC;
    info->infoCode = 0x402A0000UL | ((uint32_t)axis->axis << 8U) | (uint32_t)origin;
    return true;
}

static const CO_402_device_diag_if_t CO_402_demoDiagIf = {
    .getFaultInfo = CO_402_demoGetFaultInfo,
};
#endif /* CO_402_CONFIG_DIAGNOSTICS */

/* Deterministic software DriveIF used to validate protocol and supervisor behavior without motor hardware. */
static const CO_402_drive_if_t CO_402_demoDriveIf = {
    .shutdown = CO_402_demoShutdown,
    .switchOn = CO_402_demoSwitchOn,
    .enableOperation = CO_402_demoEnableOperation,
    .disableOperation = CO_402_demoDisableOperation,
    .quickStop = CO_402_demoQuickStop,
    .faultReaction = CO_402_demoFaultReaction,
    .faultReset = CO_402_demoFaultReset,
    .getPosition = CO_402_demoGetPosition,
    .getVelocity = CO_402_demoGetVelocity,
    .getTorque = CO_402_demoGetTorque,
    .disableVoltage = CO_402_demoDisableVoltage,
    .modeEnter = CO_402_demoModeEnter,
    .modeExit = CO_402_demoModeExit,
    .profilePosition = CO_402_demoProfilePosition,
    .profileVelocity = CO_402_demoProfileVelocity,
    .homing = CO_402_demoHoming,
};

/* Generated demo OD provides three consecutive local logical-device blocks. */
static const CO_402_device_axis_config_t CO_402_demoAxisConfigs[] = {
    {
        .logicalDevice = 0U,
        .drive = &CO_402_demoDriveIf,
        .driveObject = &CO_402_demoAxes[0],
#if CO_402_CONFIG_DIAGNOSTICS
        .diag = &CO_402_demoDiagIf,
        .diagObject = &CO_402_demoAxes[0],
#endif /* CO_402_CONFIG_DIAGNOSTICS */
#if CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST
        .sync = &CO_402_demoSyncIf,
        .syncObject = &CO_402_demoAxes[0],
#endif /* CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST */
    },
    {
        .logicalDevice = 1U,
        .drive = &CO_402_demoDriveIf,
        .driveObject = &CO_402_demoAxes[1],
#if CO_402_CONFIG_DIAGNOSTICS
        .diag = &CO_402_demoDiagIf,
        .diagObject = &CO_402_demoAxes[1],
#endif /* CO_402_CONFIG_DIAGNOSTICS */
#if CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST
        .sync = &CO_402_demoSyncIf,
        .syncObject = &CO_402_demoAxes[1],
#endif /* CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST */
    },
    {
        .logicalDevice = 2U,
        .drive = &CO_402_demoDriveIf,
        .driveObject = &CO_402_demoAxes[2],
#if CO_402_CONFIG_DIAGNOSTICS
        .diag = &CO_402_demoDiagIf,
        .diagObject = &CO_402_demoAxes[2],
#endif /* CO_402_CONFIG_DIAGNOSTICS */
#if CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST
        .sync = &CO_402_demoSyncIf,
        .syncObject = &CO_402_demoAxes[2],
#endif /* CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST */
    },
};

CO_402_DEVICE_RTT_AUTOSTART_DEFINE(cia402_demo, CO_402_demoAxisConfigs,
                                    PKG_CANOPENNODE_CIA402_DEMO_AXIS_COUNT);
