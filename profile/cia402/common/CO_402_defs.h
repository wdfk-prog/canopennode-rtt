/**
 * @file CO_402_defs.h
 * @brief Role-neutral CiA 402 common definitions.
 *
 * This header contains only profile-wide definitions shared by local Device and
 * future Controller implementations. It intentionally has no RT-Thread,
 * hardware-drive, Object Dictionary ownership, or remote-SDO dependencies.
 */

#ifndef CO_402_DEFS_H
#define CO_402_DEFS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of logical devices in one CANopen device. */
#define CO_402_LOGICAL_DEVICE_COUNT_MAX 8U

/** Axis-0 CiA 402 application-profile object range base. */
#define CO_402_PROFILE_INDEX_BASE 0x6000U

/** Offset between adjacent logical-device CiA 402 object ranges. */
#define CO_402_PROFILE_INDEX_STRIDE 0x0800U

/** Last index in the axis-0 CiA 402 application-profile object range. */
#define CO_402_PROFILE_INDEX_LAST 0x67FFU

/**
 * @brief Resolve an axis-0 CiA 402 object index for one logical device.
 *
 * @param logicalDevice Zero-based logical-device index in the CANopen device.
 * @param axis0Index Object index in the axis-0 0x6000..0x67FF profile range.
 * @return Object Dictionary index for the selected logical device.
 *
 * @pre logicalDevice is less than CO_402_LOGICAL_DEVICE_COUNT_MAX.
 * @pre axis0Index is in the inclusive range 0x6000..0x67FF.
 */
static inline uint16_t CO_402_objectIndex(uint8_t logicalDevice, uint16_t axis0Index)
{
    return (uint16_t)(axis0Index + ((uint16_t)logicalDevice * (uint16_t)CO_402_PROFILE_INDEX_STRIDE));
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_DEFS_H */
