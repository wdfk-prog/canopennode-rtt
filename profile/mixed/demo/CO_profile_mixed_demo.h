/**
 * @file CO_profile_mixed_demo.h
 * @brief Canonical logical-device layout for the package mixed CiA 402/CiA 401 demo.
 */

#ifndef CO_PROFILE_MIXED_DEMO_H_
#define CO_PROFILE_MIXED_DEMO_H_

#include <stdint.h>

#include "CO_profile_registry.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** CiA 402 profile number used by all three mixed-demo drive logical devices. */
#define CO_PROFILE_MIXED_DEMO_CIA402_PROFILE_NUMBER 402U
/** CiA 401 profile number used by the mixed-demo generic I/O logical device. */
#define CO_PROFILE_MIXED_DEMO_CIA401_PROFILE_NUMBER 401U

/** Logical-device slot occupied by CiA 402 axis 0. */
#define CO_PROFILE_MIXED_DEMO_CIA402_AXIS0_LOGICAL_DEVICE 0U
/** Logical-device slot occupied by CiA 402 axis 1. */
#define CO_PROFILE_MIXED_DEMO_CIA402_AXIS1_LOGICAL_DEVICE 1U
/** Logical-device slot occupied by CiA 402 axis 2. */
#define CO_PROFILE_MIXED_DEMO_CIA402_AXIS2_LOGICAL_DEVICE 2U
/** Logical-device slot occupied by the CiA 401 generic I/O device. */
#define CO_PROFILE_MIXED_DEMO_CIA401_LOGICAL_DEVICE       3U

/** Number of canonical logical-device ownership declarations in the mixed demo. */
#define CO_PROFILE_MIXED_DEMO_DESCRIPTOR_COUNT 4U

/** Package-demo ownership declarations passed to the profile-neutral mixed RT-Thread adapter. */
extern const CO_profile_descriptor_t CO_profileMixedDemoDescriptors[CO_PROFILE_MIXED_DEMO_DESCRIPTOR_COUNT];

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_PROFILE_MIXED_DEMO_H_ */
