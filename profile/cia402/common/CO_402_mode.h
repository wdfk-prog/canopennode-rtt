/**
 * @file CO_402_mode.h
 * @brief Role-neutral CiA 402 operation-mode identifiers and mode bits.
 */

#ifndef CO_402_MODE_H
#define CO_402_MODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Platform adapters may define these role-neutral feature switches before this
 * header is parsed. Standalone Pure-C integrations default to no mode runtime.
 */
#ifndef CO_402_CONFIG_MODE_PP
#define CO_402_CONFIG_MODE_PP 0
#endif /* !defined(CO_402_CONFIG_MODE_PP) */
#ifndef CO_402_CONFIG_MODE_PV
#define CO_402_CONFIG_MODE_PV 0
#endif /* !defined(CO_402_CONFIG_MODE_PV) */
#ifndef CO_402_CONFIG_MODE_HM
#define CO_402_CONFIG_MODE_HM 0
#endif /* !defined(CO_402_CONFIG_MODE_HM) */
#ifndef CO_402_CONFIG_MODE_CSP
#define CO_402_CONFIG_MODE_CSP 0
#endif /* !defined(CO_402_CONFIG_MODE_CSP) */
#ifndef CO_402_CONFIG_MODE_CSV
#define CO_402_CONFIG_MODE_CSV 0
#endif /* !defined(CO_402_CONFIG_MODE_CSV) */
#ifndef CO_402_CONFIG_MODE_CST
#define CO_402_CONFIG_MODE_CST 0
#endif /* !defined(CO_402_CONFIG_MODE_CST) */

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

/** Standard 0x6502 capability bits used by implemented modes. */
#define CO_402_SUPPORTED_MODE_PP  (1UL << 0)
#define CO_402_SUPPORTED_MODE_PV  (1UL << 2)
#define CO_402_SUPPORTED_MODE_HM  (1UL << 5)
#define CO_402_SUPPORTED_MODE_CSP (1UL << 7)
#define CO_402_SUPPORTED_MODE_CSV (1UL << 8)
#define CO_402_SUPPORTED_MODE_CST (1UL << 9)

/** Controlword bits whose meaning is interpreted by PP/HM/PV mode supervisors. */
#define CO_402_CONTROLWORD_MODE_BIT4              (1U << 4)
#define CO_402_CONTROLWORD_PP_CHANGE_IMMEDIATELY  (1U << 5)
#define CO_402_CONTROLWORD_PP_RELATIVE             (1U << 6)
#define CO_402_CONTROLWORD_HALT                    (1U << 8)

/** Statusword bits contributed by the active operation mode. */
#define CO_402_STATUSWORD_TARGET_REACHED             (1U << 10)
#define CO_402_STATUSWORD_PP_SETPOINT_ACKNOWLEDGE    (1U << 12)
#define CO_402_STATUSWORD_HM_ATTAINED                (1U << 12)
#define CO_402_STATUSWORD_DRIVE_FOLLOWS_COMMAND      (1U << 12)
#define CO_402_STATUSWORD_PP_FOLLOWING_ERROR         (1U << 13)
#define CO_402_STATUSWORD_HM_ERROR                   (1U << 13)

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

/**
 * @brief Return the standard 0x6502 bit for one recognized operation mode.
 *
 * @param mode Semantic operation mode.
 * @return Capability bit mask, or zero for NONE/unsupported values.
 */
static inline uint32_t CO_402_modeCapabilityBit(CO_402_mode_t mode)
{
    switch (mode) {
        case CO_402_MODE_PROFILE_POSITION:
            return CO_402_SUPPORTED_MODE_PP;
        case CO_402_MODE_PROFILE_VELOCITY:
            return CO_402_SUPPORTED_MODE_PV;
        case CO_402_MODE_HOMING:
            return CO_402_SUPPORTED_MODE_HM;
        case CO_402_MODE_CYCLIC_SYNC_POSITION:
            return CO_402_SUPPORTED_MODE_CSP;
        case CO_402_MODE_CYCLIC_SYNC_VELOCITY:
            return CO_402_SUPPORTED_MODE_CSV;
        case CO_402_MODE_CYCLIC_SYNC_TORQUE:
            return CO_402_SUPPORTED_MODE_CST;
        case CO_402_MODE_NONE:
        default:
            return 0U;
    }
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_MODE_H */
