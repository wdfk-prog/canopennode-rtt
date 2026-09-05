/**
 * @file CO_401_defs.h
 * @brief CiA 401 generic I/O profile-wide definitions.
 */

#ifndef CO_401_DEFS_H
#define CO_401_DEFS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** CiA 401 device-profile number encoded in Object 0x1000 bits 0..15. */
#define CO_401_DEVICE_PROFILE_NUMBER 401U

/** Object 0x1000 capability bit for digital inputs. */
#define CO_401_DEVICE_TYPE_DIGITAL_INPUT (UINT32_C(1) << 16)
/** Object 0x1000 capability bit for digital outputs. */
#define CO_401_DEVICE_TYPE_DIGITAL_OUTPUT (UINT32_C(1) << 17)
/** Object 0x1000 capability bit for analogue inputs. */
#define CO_401_DEVICE_TYPE_ANALOG_INPUT (UINT32_C(1) << 18)
/** Object 0x1000 capability bit for analogue outputs. */
#define CO_401_DEVICE_TYPE_ANALOG_OUTPUT (UINT32_C(1) << 19)
/** Object 0x1000 bit selecting device-specific rather than generic PDO mapping. */
#define CO_401_DEVICE_TYPE_DEVICE_SPECIFIC_MAPPING (UINT32_C(1) << 23)

/** Stage-1 local capability flags, independent from the Object 0x1000 bit positions. */
typedef uint8_t CO_401_capabilities_t;

#define CO_401_CAP_DIGITAL_INPUT ((CO_401_capabilities_t)(1U << 0))
#define CO_401_CAP_DIGITAL_OUTPUT ((CO_401_capabilities_t)(1U << 1))
#define CO_401_CAP_ANALOG_INPUT ((CO_401_capabilities_t)(1U << 2))
#define CO_401_CAP_ANALOG_OUTPUT ((CO_401_capabilities_t)(1U << 3))
#define CO_401_CAP_ALL ((CO_401_capabilities_t)(CO_401_CAP_DIGITAL_INPUT | CO_401_CAP_DIGITAL_OUTPUT \
                                                | CO_401_CAP_ANALOG_INPUT | CO_401_CAP_ANALOG_OUTPUT))

/**
 * @brief Build the Stage-1 CiA 401 Object 0x1000 value for one capability set.
 *
 * Stage 1 implements the generic pre-defined PDO model (M bit clear) and no
 * profile-specific joystick function (bits 24..31 clear).
 *
 * @param capabilities Local capability flags.
 * @return Exact Object 0x1000 value expected by the Stage-1 runtime contract.
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
