/**
 * @file CO_402_device_internal.h
 * @brief Internal ownership predicates shared by CiA 402 Device execution paths.
 */
#ifndef CO_402_DEVICE_INTERNAL_H
#define CO_402_DEVICE_INTERNAL_H

#include "CO_402_device.h"

#define CO_402_DEVICE_CONTROLWORD_FAULT_RESET 0x0080U

/* A BUSY modeEnter/modeExit callback owns DriveIF until completion or a safety ownership transfer. */
static inline bool CO_402_device_modeTransitionPending(const CO_402_device_axis_t *axis)
{
    return axis != NULL
           && (axis->pendingExitMode != CO_402_MODE_NONE || axis->pendingEnterMode != CO_402_MODE_NONE);
}

#if CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST
/*
 * The synchronous RPDO is applied before this fast path, while the lower-priority
 * supervisor may still cache the previous PDS state and active mode. Re-read the
 * live control plane so a stop/disable or mode switch suppresses the old cyclic
 * command in the same SYNC generation. State transitions remain supervisor-owned.
 */
static inline bool CO_402_device_syncControlPlaneStable(const CO_402_device_axis_t *axis)
{
    uint16_t controlword;
    int8_t requestedModeRaw;
    CO_402_mode_t requestedMode;

    if (axis == NULL || axis->od.controlword == NULL || axis->od.modesOfOperation == NULL) {
        return false;
    }
    if (OD_get_u16(axis->od.controlword, 0U, &controlword, true) != ODR_OK) {
        return false;
    }
    controlword = (uint16_t)(controlword & (uint16_t)~CO_402_DEVICE_CONTROLWORD_FAULT_RESET);
    if (CO_402_decodeControlword(controlword) != CO_402_COMMAND_ENABLE_OPERATION) {
        return false;
    }
    if (OD_get_i8(axis->od.modesOfOperation, 0U, &requestedModeRaw, true) != ODR_OK
        || !CO_402_modeFromRaw(requestedModeRaw, &requestedMode)) {
        return false;
    }

    return requestedMode == axis->mode;
}

/*
 * A BUSY PDS callback or BUSY mode enter/exit is an exclusive DriveIF ownership
 * window. co_rt stays silent until completion or a safety ownership transfer;
 * while silent, the lower-priority supervisor may publish optional DriveIF feedback.
 */
static inline bool CO_402_device_syncFastPathEligible(const CO_402_device_axis_t *axis)
{
    if (axis == NULL || axis->state != CO_402_STATE_OPERATION_ENABLED
        || axis->pdsTransitionOperation != NULL || CO_402_device_modeTransitionPending(axis)
        || axis->sync == NULL
        || axis->sync->publishCommand == NULL || axis->sync->readFeedback == NULL) {
        return false;
    }

    switch (axis->mode) {
#if CO_402_CONFIG_MODE_CSP
        case CO_402_MODE_CYCLIC_SYNC_POSITION:
            break;
#endif /* CO_402_CONFIG_MODE_CSP */
#if CO_402_CONFIG_MODE_CSV
        case CO_402_MODE_CYCLIC_SYNC_VELOCITY:
            break;
#endif /* CO_402_CONFIG_MODE_CSV */
#if CO_402_CONFIG_MODE_CST
        case CO_402_MODE_CYCLIC_SYNC_TORQUE:
            break;
#endif /* CO_402_CONFIG_MODE_CST */
        case CO_402_MODE_NONE:
        default:
            return false;
    }

    return CO_402_device_syncControlPlaneStable(axis);
}
#endif /* CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST */

#endif /* CO_402_DEVICE_INTERNAL_H */
