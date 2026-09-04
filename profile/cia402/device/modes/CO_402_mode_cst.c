/**
 * @file CO_402_mode_cst.c
 * @brief Pure-C CiA 402 cyclic synchronous torque OD snapshot helper.
 */
#include "CO_402_device.h"
#include "CO_402_mode_cst.h"

bool CO_402_mode_cst_snapshot(CO_402_device_axis_t *axis, CO_402_sync_command_t *command)
{
    if (axis == NULL || command == NULL) {
        return false;
    }

    return OD_get_i16(axis->od.targetTorque, 0U, &command->targetTorque, true) == ODR_OK;
}
