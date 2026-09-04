/**
 * @file CO_402_mode_hm.h
 * @brief Pure-C CiA 402 Homing mode runtime.
 */

#ifndef CO_402_MODE_HM_H
#define CO_402_MODE_HM_H

#include <stdbool.h>
#include <stdint.h>

#include "CO_402_drive.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CO_402_device_axis;

/** Per-axis HM start/abort ownership and result state. */
typedef struct {
    CO_402_homing_command_t acceptedCommand; /**< Start-edge snapshot retained through normal completion or abort. */
    bool previousStart; /**< Previous bit-4 level used to detect a fresh homing start request. */
    bool active;        /**< True while the DriveIF still owns an accepted homing or abort operation. */
    bool aborting;      /**< True after start drops; restart is blocked until the DriveIF acknowledges abort completion. */
    bool attained;      /**< Bit-12 state set only when the active homing operation completes normally. */
    bool error;         /**< Bit-13 state set when parameter capture or DriveIF execution fails. */
    bool targetReached; /**< Bit-10 state: false while homing/abort is active, true when idle or finished. */
} CO_402_mode_hm_t;

/**
 * @brief Reset HM start/result state when the PDS gate closes or HM is entered/exited.
 * @param runtime Per-axis HM runtime.
 * @param controlword Current Controlword snapshot used to seed bit-4 edge detection.
 */
void CO_402_mode_hm_reset(CO_402_mode_hm_t *runtime, uint16_t controlword);

/**
 * @brief Process one bounded Homing supervisor pass for an Operation-enabled axis.
 *
 * A valid start edge snapshots 0x607C/0x6098/0x6099/0x609A. Active and abort
 * polling retain those parameters until DONE/ERROR, so later OD writes apply
 * only to the next fresh start edge. Controlword start/halt states remain live.
 *
 * @param axis Axis whose OD and DriveIF are used for the command.
 * @param controlword Coherent Controlword snapshot for this supervisor pass.
 * @return true on a valid/accepted mode step; false requests PDS fault reaction.
 */
bool CO_402_mode_hm_process(struct CO_402_device_axis *axis, uint16_t controlword);

/**
 * @brief Encode HM-owned Statusword bits 10, 12 and 13.
 * @param runtime Per-axis HM runtime.
 * @return Mode-specific Statusword bit mask.
 */
uint16_t CO_402_mode_hm_statusword(const CO_402_mode_hm_t *runtime);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_MODE_HM_H */
