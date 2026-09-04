/**
 * @file CO_402_mode_csv.c
 * @brief Pure-C CiA 402 cyclic synchronous velocity OD snapshot helper.
 */
#include "CO_402_device.h"
#include "CO_402_mode_csv.h"

bool CO_402_mode_csv_snapshot(CO_402_device_axis_t *axis, CO_402_sync_command_t *command)
{
    if (axis == NULL || command == NULL) {
        return false;
    }

    return OD_get_i32(axis->od.targetVelocity, 0U, &command->targetVelocity, true) == ODR_OK;
}
