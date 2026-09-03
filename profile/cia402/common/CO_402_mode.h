/**
 * @file CO_402_mode.h
 * @brief Role-neutral CiA 402 operation-mode identifiers.
 */

#ifndef CO_402_MODE_H
#define CO_402_MODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Raw value stored in the CiA 402 INTEGER8 mode objects (0x6060/0x6061). */
typedef int8_t CO_402_mode_raw_t;

/** Symbolic CiA 402 operation mode recognized by the profile runtime. */
typedef enum {
    CO_402_MODE_NONE = 0,                 /**< No operation mode selected. */
    CO_402_MODE_PROFILE_POSITION = 1,     /**< Profile position mode. */
    CO_402_MODE_PROFILE_VELOCITY = 3,     /**< Profile velocity mode. */
    CO_402_MODE_HOMING = 6,               /**< Homing mode. */
    CO_402_MODE_CYCLIC_SYNC_POSITION = 8, /**< Cyclic synchronous position mode. */
    CO_402_MODE_CYCLIC_SYNC_VELOCITY = 9, /**< Cyclic synchronous velocity mode. */
    CO_402_MODE_CYCLIC_SYNC_TORQUE = 10   /**< Cyclic synchronous torque mode. */
} CO_402_mode_t;

/**
 * @brief Convert an INTEGER8 mode value to a recognized semantic mode.
 *
 * @param raw Raw OD value.
 * @param mode Destination for the semantic mode.
 * @return true if @p raw is recognized, otherwise false.
 */
static inline bool CO_402_modeFromRaw(CO_402_mode_raw_t raw, CO_402_mode_t *mode)
{
    if (mode == NULL) {
        return false;
    }

    switch (raw) {
        case (CO_402_mode_raw_t)CO_402_MODE_NONE:
        case (CO_402_mode_raw_t)CO_402_MODE_PROFILE_POSITION:
        case (CO_402_mode_raw_t)CO_402_MODE_PROFILE_VELOCITY:
        case (CO_402_mode_raw_t)CO_402_MODE_HOMING:
        case (CO_402_mode_raw_t)CO_402_MODE_CYCLIC_SYNC_POSITION:
        case (CO_402_mode_raw_t)CO_402_MODE_CYCLIC_SYNC_VELOCITY:
        case (CO_402_mode_raw_t)CO_402_MODE_CYCLIC_SYNC_TORQUE:
            *mode = (CO_402_mode_t)raw;
            return true;
        default:
            return false;
    }
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_MODE_H */
