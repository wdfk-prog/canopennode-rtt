/**
 * @file CO_402_mode_pv.c
 * @brief Pure-C CiA 402 Profile Velocity mode supervisor.
 */

#include <string.h>

#include "CO_402_device.h"
#include "CO_402_mode_pv.h"

void CO_402_mode_pv_reset(CO_402_mode_pv_t *runtime)
{
    if (runtime != NULL) {
        memset(runtime, 0, sizeof(*runtime));
    }
}

bool CO_402_mode_pv_process(CO_402_device_axis_t *axis, uint16_t controlword)
{
    CO_402_profile_velocity_command_t command;
    CO_402_drive_result_t result;

    if (axis == NULL || axis->drive == NULL || axis->drive->profileVelocity == NULL) {
        return false;
    }
    if (OD_get_i32(axis->od.targetVelocity, 0U, &command.targetVelocity, true) != ODR_OK
        || OD_get_u32(axis->od.profileAcceleration, 0U, &command.profileAcceleration, true) != ODR_OK
        || OD_get_u32(axis->od.profileDeceleration, 0U, &command.profileDeceleration, true) != ODR_OK
        || OD_get_u32(axis->od.quickStopDeceleration, 0U, &command.quickStopDeceleration, true) != ODR_OK) {
        axis->pv.targetReached = false;
        return false;
    }

    command.halt = (controlword & CO_402_CONTROLWORD_HALT) != 0U;
    result = axis->drive->profileVelocity(axis->driveObject, &command);
    if (result == CO_402_DRIVE_ERROR) {
        axis->pv.targetReached = false;
        return false;
    }

    axis->pv.targetReached = result == CO_402_DRIVE_DONE;
    return true;
}

uint16_t CO_402_mode_pv_statusword(const CO_402_mode_pv_t *runtime)
{
    if (runtime != NULL && runtime->targetReached) {
        return CO_402_STATUSWORD_TARGET_REACHED;
    }

    return 0U;
}
