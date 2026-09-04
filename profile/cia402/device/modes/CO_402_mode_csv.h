/**
 * @file CO_402_mode_csv.h
 * @brief Pure-C CiA 402 cyclic synchronous velocity snapshot helper.
 */
#ifndef CO_402_MODE_CSV_H
#define CO_402_MODE_CSV_H

#include <stdbool.h>

#include "CO_402_device_sync.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct CO_402_device_axis;

/**
 * @brief Snapshot the bound CSV target into one current-generation command.
 * @param axis Bound Device axis whose RPDO-updated OD target is sampled.
 * @param command Current-generation command receiving the velocity target.
 * @return true when the OD snapshot succeeded, otherwise false.
 */
bool CO_402_mode_csv_snapshot(struct CO_402_device_axis *axis, CO_402_sync_command_t *command);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* CO_402_MODE_CSV_H */
