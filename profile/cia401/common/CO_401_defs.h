/**
 * @file CO_401_defs.h
 * @brief CiA 401 generic I/O profile-wide definitions.
 */

#ifndef CO_401_DEFS_H
#define CO_401_DEFS_H

#include <stdint.h>

#include "CO_profile_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Compatibility alias for the shared CiA 301 logical-device slot limit. */
#define CO_401_LOGICAL_DEVICE_COUNT_MAX CO_PROFILE_LOGICAL_DEVICE_COUNT_MAX

/** Compatibility alias for the shared canonical application-profile base. */
#define CO_401_PROFILE_INDEX_BASE CO_PROFILE_OD_BASE
/** Compatibility alias for the shared logical-device profile stride. */
#define CO_401_PROFILE_INDEX_STRIDE CO_PROFILE_OD_STRIDE
/** Compatibility alias for the shared canonical application-profile last index. */
#define CO_401_PROFILE_INDEX_LAST CO_PROFILE_OD_CANONICAL_LAST

/** CiA 401 device-profile number encoded in Device type bits 0..15. */
#define CO_401_DEVICE_PROFILE_NUMBER 401U

/** Device type capability bit for digital inputs. */
#define CO_401_DEVICE_TYPE_DIGITAL_INPUT (UINT32_C(1) << 16)
/** Device type capability bit for digital outputs. */
#define CO_401_DEVICE_TYPE_DIGITAL_OUTPUT (UINT32_C(1) << 17)
/** Device type capability bit for analogue inputs. */
#define CO_401_DEVICE_TYPE_ANALOG_INPUT (UINT32_C(1) << 18)
/** Device type capability bit for analogue outputs. */
#define CO_401_DEVICE_TYPE_ANALOG_OUTPUT (UINT32_C(1) << 19)
/** Device type bit selecting device-specific rather than generic PDO mapping. */
#define CO_401_DEVICE_TYPE_DEVICE_SPECIFIC_MAPPING (UINT32_C(1) << 23)

/** Stage-1 local capability flags, independent from the Device type bit positions. */
typedef uint8_t CO_401_capabilities_t;

#define CO_401_CAP_DIGITAL_INPUT ((CO_401_capabilities_t)(1U << 0))
#define CO_401_CAP_DIGITAL_OUTPUT ((CO_401_capabilities_t)(1U << 1))
#define CO_401_CAP_ANALOG_INPUT ((CO_401_capabilities_t)(1U << 2))
#define CO_401_CAP_ANALOG_OUTPUT ((CO_401_capabilities_t)(1U << 3))
#define CO_401_CAP_ALL ((CO_401_capabilities_t)(CO_401_CAP_DIGITAL_INPUT | CO_401_CAP_DIGITAL_OUTPUT \
                                                | CO_401_CAP_ANALOG_INPUT | CO_401_CAP_ANALOG_OUTPUT))

/**
 * @brief Preserve the CiA 401 index helper while delegating translation to the shared CiA 301 layout.
 *
 * @param logicalDevice Zero-based logical-device index in the CANopen device.
 * @param profileIndex Object index in the canonical 0x6000..0x67FF CiA 401 block.
 * @return Absolute Object Dictionary index for the selected logical-device slot.
 *
 * This helper translates only the standardized application-profile block. Communication-profile
 * objects such as 0x1000 are global and must not be passed through this function.
 *
 * @pre logicalDevice is less than CO_401_LOGICAL_DEVICE_COUNT_MAX.
 * @pre profileIndex is in the inclusive range CO_401_PROFILE_INDEX_BASE..CO_401_PROFILE_INDEX_LAST.
 */
static inline uint16_t CO_401_objectIndex(uint8_t logicalDevice, uint16_t profileIndex)
{
    return CO_profileIndex(logicalDevice, profileIndex);
}

/**
 * @brief Build the CiA 401 Device type value for one capability set.
 *
 * The current Device core implements the generic pre-defined PDO model (M bit clear)
 * and no profile-specific joystick function (bits 24..31 clear).
 *
 * @param capabilities Local capability flags.
 * @return Device type value expected for a standalone CiA 401 device or one logical-device type entry.
 */
static inline uint32_t CO_401_deviceTypeForCapabilities(CO_401_capabilities_t capabilities)
{
    uint32_t deviceType = (uint32_t)CO_401_DEVICE_PROFILE_NUMBER;

    if ((capabilities & CO_401_CAP_DIGITAL_INPUT) != 0U) {
        deviceType |= CO_401_DEVICE_TYPE_DIGITAL_INPUT;
    }
    if ((capabilities & CO_401_CAP_DIGITAL_OUTPUT) != 0U) {
        deviceType |= CO_401_DEVICE_TYPE_DIGITAL_OUTPUT;
    }
    if ((capabilities & CO_401_CAP_ANALOG_INPUT) != 0U) {
        deviceType |= CO_401_DEVICE_TYPE_ANALOG_INPUT;
    }
    if ((capabilities & CO_401_CAP_ANALOG_OUTPUT) != 0U) {
        deviceType |= CO_401_DEVICE_TYPE_ANALOG_OUTPUT;
    }

    return deviceType;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_401_DEFS_H */
