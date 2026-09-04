/**
 * @file CO_402_mode_pv.h
 * @brief Pure-C CiA 402 Profile Velocity mode runtime.
 */

#ifndef CO_402_MODE_PV_H
#define CO_402_MODE_PV_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct CO_402_device_axis;

/** Per-axis PV completion state for the most recently processed live command. */
typedef struct {
    bool targetReached; /**< Bit-10 state; true when the latest DriveIF velocity pass returned DONE. */
} CO_402_mode_pv_t;

/**
 * @brief Reset PV mode-result state when the PDS gate closes or PV is entered/exited.
 * @param runtime Per-axis PV runtime.
 */
void CO_402_mode_pv_reset(CO_402_mode_pv_t *runtime);

/**
 * @brief Process one bounded PV supervisor pass for an Operation-enabled axis.
 * @param axis Axis whose OD and DriveIF are used for the command.
 * @param controlword Coherent Controlword snapshot for this supervisor pass.
 * @return true on a valid/accepted mode step; false requests PDS fault reaction.
 */
bool CO_402_mode_pv_process(struct CO_402_device_axis *axis, uint16_t controlword);

/**
 * @brief Encode the PV-owned target-reached Statusword bit.
 * @param runtime Per-axis PV runtime.
 * @return Mode-specific Statusword bit mask.
 */
uint16_t CO_402_mode_pv_statusword(const CO_402_mode_pv_t *runtime);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_MODE_PV_H */
