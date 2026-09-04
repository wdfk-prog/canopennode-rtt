/**
 * @file CO_402_mode_pp.h
 * @brief Pure-C CiA 402 Profile Position mode runtime.
 */

#ifndef CO_402_MODE_PP_H
#define CO_402_MODE_PP_H

#include <stdbool.h>
#include <stdint.h>

#include "CO_402_drive.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CO_402_device_axis;

/** Per-axis PP handshake and in-flight command ownership state. */
typedef struct {
    CO_402_profile_position_command_t acceptedCommand; /**< Snapshot retained until the accepted set-point finishes/fails. */
    bool previousNewSetPoint; /**< Previous bit-4 level used to reject repeated high levels as new commands. */
    bool commandActive;       /**< True while DriveIF reports BUSY for the accepted set-point. */
    bool setPointAcknowledge; /**< Bit-12 latch set after acceptance and cleared when Controlword bit 4 returns low. */
    bool targetReached;       /**< Bit-10 state set only after DriveIF reports DONE for the accepted set-point. */
    bool followingError;      /**< Bit-13 state set when command snapshot/read or DriveIF execution fails. */
} CO_402_mode_pp_t;

/**
 * @brief Reset PP handshake state when the PDS gate closes or PP is entered/exited.
 * @param runtime Per-axis PP runtime.
 * @param controlword Current Controlword snapshot used to seed bit-4 edge detection.
 */
void CO_402_mode_pp_reset(CO_402_mode_pp_t *runtime, uint16_t controlword);

/**
 * @brief Process one bounded PP supervisor pass for an Operation-enabled axis.
 *
 * A valid Controlword bit-4 rising edge snapshots the PP OD command. A BUSY
 * DriveIF result retains that snapshot until completion or a later valid
 * set-point edge; ordinary OD writes cannot mutate the in-flight command.
 * Controlword halt remains live while polling the accepted command.
 *
 * @param axis Axis whose OD and DriveIF are used for the command.
 * @param controlword Coherent Controlword snapshot for this supervisor pass.
 * @return true on a valid/accepted mode step; false requests PDS fault reaction.
 */
bool CO_402_mode_pp_process(struct CO_402_device_axis *axis, uint16_t controlword);

/**
 * @brief Encode PP-owned Statusword bits 10, 12 and 13.
 * @param runtime Per-axis PP runtime.
 * @return Mode-specific Statusword bit mask.
 */
uint16_t CO_402_mode_pp_statusword(const CO_402_mode_pp_t *runtime);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_MODE_PP_H */
