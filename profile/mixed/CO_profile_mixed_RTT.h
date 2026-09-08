/**
 * @file CO_profile_mixed_RTT.h
 * @brief Profile-neutral RT-Thread lifecycle adapter for logical-device ownership validation.
 */

#ifndef CO_PROFILE_MIXED_RTT_H_
#define CO_PROFILE_MIXED_RTT_H_

#include <stdint.h>

#include <rtthread.h>

#include "CO_profile_registry.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct CANopenNodeRTT;
typedef struct CANopenNodeRTT CANopenNodeRTT;

/** Caller-provided logical-device ownership table for one CANopenNodeRTT instance. */
typedef struct {
    const CO_profile_descriptor_t *descriptors; /**< Complete ownership declarations copied during attach. */
    uint8_t descriptorCount;                    /**< Number of entries in @p descriptors, range 1..8. */
} CO_profile_mixed_RTT_config_t;

/**
 * @brief Attach profile-neutral logical-device ownership validation to one RT-Thread application.
 *
 * The adapter copies the complete descriptor table into its lifecycle-owned runtime context,
 * so the caller's configuration only needs to remain valid for this call. Each communication
 * bind validates the copied table against the generated OD Device Type entries before later
 * profile adapters bind their OD extensions. Communication quiesce clears the published registry.
 *
 * This function does not choose Logical Device slots or Device Profile numbers. Product/demo
 * code owns those declarations and must arrange this lifecycle extension before profile-specific
 * bind extensions when fail-closed cross-profile validation is required.
 *
 * @param app Zero-initialized CANopenNode RT-Thread application instance before runtime init.
 * @param config Caller-owned descriptor configuration containing 1..8 descriptors.
 * @return RT_EOK on success, -RT_EINVAL for invalid arguments, -RT_ENOMEM on allocation failure,
 *         or the lifecycle registration error returned by CO_RTT_lifecycleRegisterEx().
 */
rt_err_t CO_profileMixedRTTAttach(CANopenNodeRTT *app, const CO_profile_mixed_RTT_config_t *config);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_PROFILE_MIXED_RTT_H_ */
