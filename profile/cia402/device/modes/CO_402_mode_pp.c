/**
 * @file CO_402_mode_pp.c
 * @brief Pure-C CiA 402 Profile Position mode supervisor.
 */

#include <string.h>

#include "CO_402_device.h"
#include "CO_402_mode_pp.h"

/* Read all edge-latched PP parameters before the set-point is handed to the DriveIF. */
static bool readCommand(CO_402_device_axis_t *axis, uint16_t controlword,
                        CO_402_profile_position_command_t *command)
{
    if (OD_get_i32(axis->od.targetPosition, 0U, &command->targetPosition, true) != ODR_OK
        || OD_get_u32(axis->od.profileVelocity, 0U, &command->profileVelocity, true) != ODR_OK
        || OD_get_u32(axis->od.profileAcceleration, 0U, &command->profileAcceleration, true) != ODR_OK
        || OD_get_u32(axis->od.profileDeceleration, 0U, &command->profileDeceleration, true) != ODR_OK
        || OD_get_u32(axis->od.quickStopDeceleration, 0U, &command->quickStopDeceleration, true) != ODR_OK
        || OD_get_i16(axis->od.motionProfileType, 0U, &command->motionProfileType, true) != ODR_OK) {
        return false;
    }

    command->newSetPoint = false;
    command->changeImmediately = (controlword & CO_402_CONTROLWORD_PP_CHANGE_IMMEDIATELY) != 0U;
    command->relative = (controlword & CO_402_CONTROLWORD_PP_RELATIVE) != 0U;
    command->halt = (controlword & CO_402_CONTROLWORD_HALT) != 0U;
    return true;
}

void CO_402_mode_pp_reset(CO_402_mode_pp_t *runtime, uint16_t controlword)
{
    if (runtime == NULL) {
        return;
    }

    memset(runtime, 0, sizeof(*runtime));
    runtime->previousNewSetPoint = (controlword & CO_402_CONTROLWORD_MODE_BIT4) != 0U;
}

bool CO_402_mode_pp_process(CO_402_device_axis_t *axis, uint16_t controlword)
{
    CO_402_mode_pp_t *runtime;
    CO_402_profile_position_command_t command;
    CO_402_drive_result_t result;
    bool newSetPoint;
    bool risingEdge;

    if (axis == NULL || axis->drive == NULL || axis->drive->profilePosition == NULL) {
        return false;
    }
    runtime = &axis->pp;
    newSetPoint = (controlword & CO_402_CONTROLWORD_MODE_BIT4) != 0U;
    risingEdge = newSetPoint && !runtime->previousNewSetPoint;

    /* Bit 12 remains asserted only while the controller keeps bit 4 high after an accepted edge. */
    if (!newSetPoint) {
        runtime->setPointAcknowledge = false;
    }
    runtime->previousNewSetPoint = newSetPoint;
    runtime->followingError = false;

    if (risingEdge) {
        /* The edge owns this command snapshot; BUSY polls must not consume later OD writes as the same set-point. */
        if (!readCommand(axis, controlword, &runtime->acceptedCommand)) {
            runtime->followingError = true;
            runtime->targetReached = false;
            runtime->setPointAcknowledge = false;
            runtime->commandActive = false;
            return false;
        }
    } else if (!runtime->commandActive) {
        return true;
    }

    command = runtime->acceptedCommand;
    command.newSetPoint = risingEdge;
    command.halt = (controlword & CO_402_CONTROLWORD_HALT) != 0U;
    result = axis->drive->profilePosition(axis->driveObject, &command);
    if (result == CO_402_DRIVE_ERROR) {
        runtime->followingError = true;
        runtime->targetReached = false;
        runtime->setPointAcknowledge = false;
        runtime->commandActive = false;
        return false;
    }

    if (risingEdge) {
        /* BUSY and DONE both mean the new set-point was accepted by the product DriveIF. */
        runtime->setPointAcknowledge = true;
    }
    runtime->commandActive = result == CO_402_DRIVE_BUSY;
    runtime->targetReached = result == CO_402_DRIVE_DONE;
    return true;
}

uint16_t CO_402_mode_pp_statusword(const CO_402_mode_pp_t *runtime)
{
    uint16_t statusword = 0U;

    if (runtime == NULL) {
        return 0U;
    }
    if (runtime->targetReached) {
        statusword |= CO_402_STATUSWORD_TARGET_REACHED;
    }
    if (runtime->setPointAcknowledge) {
        statusword |= CO_402_STATUSWORD_PP_SETPOINT_ACKNOWLEDGE;
    }
    if (runtime->followingError) {
        statusword |= CO_402_STATUSWORD_PP_FOLLOWING_ERROR;
    }

    return statusword;
}
