/**
 * @file CO_402_mode_csp.c
 * @brief Pure-C CiA 402 cyclic synchronous position OD snapshot helper.
 */
#include "CO_402_device.h"
#include "CO_402_mode_csp.h"

bool CO_402_mode_csp_snapshot(CO_402_device_axis_t *axis, CO_402_sync_command_t *command)
{
    if (axis == NULL || command == NULL) {
        return false;
    }

    return OD_get_i32(axis->od.targetPosition, 0U, &command->targetPosition, true) == ODR_OK;
}
