/**
 * @file CO_402_mode_hm.c
 * @brief Pure-C CiA 402 Homing mode supervisor.
 */

#include <string.h>

#include "CO_402_device.h"
#include "CO_402_mode_hm.h"

/* Read the complete HM parameter set once so an active homing action cannot observe later OD writes. */
static bool readCommand(CO_402_device_axis_t *axis, CO_402_homing_command_t *command)
{
    if (OD_get_i32(axis->od.homeOffset, 0U, &command->homeOffset, true) != ODR_OK
        || OD_get_i8(axis->od.homingMethod, 0U, &command->homingMethod, true) != ODR_OK
        || OD_get_u32(axis->od.homingSpeeds, 1U, &command->speedSwitch, true) != ODR_OK
        || OD_get_u32(axis->od.homingSpeeds, 2U, &command->speedZero, true) != ODR_OK
        || OD_get_u32(axis->od.homingAcceleration, 0U, &command->acceleration, true) != ODR_OK) {
        return false;
    }

    command->start = false;
    command->startEdge = false;
    command->halt = false;
    return true;
}

void CO_402_mode_hm_reset(CO_402_mode_hm_t *runtime, uint16_t controlword)
{
    if (runtime == NULL) {
        return;
    }

    memset(runtime, 0, sizeof(*runtime));
    runtime->previousStart = (controlword & CO_402_CONTROLWORD_MODE_BIT4) != 0U;
    /* CiA 402 HM uses bit 10 for idle/interrupted/not-started as well as completed target reach. */
    runtime->targetReached = true;
}

bool CO_402_mode_hm_process(CO_402_device_axis_t *axis, uint16_t controlword)
{
    CO_402_mode_hm_t *runtime;
    CO_402_homing_command_t command;
    CO_402_drive_result_t result;
    bool start;
    bool risingEdge;

    if (axis == NULL || axis->drive == NULL || axis->drive->homing == NULL) {
        return false;
    }
    runtime = &axis->hm;
    start = (controlword & CO_402_CONTROLWORD_MODE_BIT4) != 0U;
    risingEdge = start && !runtime->previousStart;
    runtime->previousStart = start;
    runtime->error = false;

    if (risingEdge && !runtime->active) {
        /* One start edge owns its parameters through normal completion or abort completion. */
        if (!readCommand(axis, &runtime->acceptedCommand)) {
            runtime->active = false;
            runtime->aborting = false;
            runtime->attained = false;
            runtime->targetReached = false;
            runtime->error = true;
            return false;
        }
        runtime->active = true;
        runtime->attained = false;
        runtime->targetReached = false;
    } else if (!runtime->active) {
        return true;
    }

    if (runtime->active && !start) {
        /* A started abort owns the DriveIF until DONE; an early bit-4 rise cannot restart homing. */
        runtime->aborting = true;
    }

    command = runtime->acceptedCommand;
    command.halt = (controlword & CO_402_CONTROLWORD_HALT) != 0U;
    if (runtime->aborting) {
        command.start = false;
        command.startEdge = false;
    } else {
        command.start = start;
        command.startEdge = risingEdge;
    }

    result = axis->drive->homing(axis->driveObject, &command);
    if (result == CO_402_DRIVE_ERROR) {
        runtime->active = false;
        runtime->aborting = false;
        runtime->attained = false;
        runtime->targetReached = false;
        runtime->error = true;
        return false;
    }
    if (result == CO_402_DRIVE_BUSY) {
        runtime->active = true;
        runtime->targetReached = false;
        return true;
    }

    runtime->active = false;
    runtime->targetReached = true;
    if (runtime->aborting || !start) {
        /* DONE from the abort path is an acknowledged interrupt, not homing completion. */
        runtime->attained = false;
    } else {
        runtime->attained = true;
    }
    runtime->aborting = false;
    return true;
}

uint16_t CO_402_mode_hm_statusword(const CO_402_mode_hm_t *runtime)
{
    uint16_t statusword = 0U;

    if (runtime == NULL) {
        return 0U;
    }
    if (runtime->targetReached) {
        statusword |= CO_402_STATUSWORD_TARGET_REACHED;
    }
    if (runtime->attained) {
        statusword |= CO_402_STATUSWORD_HM_ATTAINED;
    }
    if (runtime->error) {
        statusword |= CO_402_STATUSWORD_HM_ERROR;
    }

    return statusword;
}
