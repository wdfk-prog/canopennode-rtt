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

#include "CO_profile_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Compatibility alias for the shared CiA 301 logical-device slot limit. */
#define CO_402_LOGICAL_DEVICE_COUNT_MAX CO_PROFILE_LOGICAL_DEVICE_COUNT_MAX

/** Compatibility alias for the shared canonical application-profile base. */
#define CO_402_PROFILE_INDEX_BASE CO_PROFILE_OD_BASE

/** Compatibility alias for the shared logical-device profile stride. */
#define CO_402_PROFILE_INDEX_STRIDE CO_PROFILE_OD_STRIDE

/** Compatibility alias for the shared canonical application-profile last index. */
#define CO_402_PROFILE_INDEX_LAST CO_PROFILE_OD_CANONICAL_LAST

/**
 * @brief Preserve the CiA 402 index helper while delegating translation to the shared CiA 301 layout.
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
    return CO_profileIndex(logicalDevice, axis0Index);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_DEFS_H */
