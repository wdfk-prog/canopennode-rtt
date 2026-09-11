/**
 * @file CO_401_controller.h
 * @brief Transport-agnostic CiA 401 remote I/O Controller core.
 */

#ifndef CO_401_CONTROLLER_H_
#define CO_401_CONTROLLER_H_

#include <stdbool.h>
#include <stdint.h>

#include "CO_401_types.h"
#include "CO_profile_controller.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Expected remote CiA 401 process-image shape and logical-device ownership. */
typedef struct {
    uint8_t logicalDevice; /**< Zero-based CiA 301 logical-device slot. */
    uint8_t digitalInputBanks; /**< Number of remote 0x6000 8-bit banks. */
    uint8_t digitalOutputBanks; /**< Number of remote 0x6200 8-bit banks. */
    uint8_t analogInputChannels; /**< Number of remote 0x6401 INTEGER16 channels. */
    uint8_t analogOutputChannels; /**< Number of remote 0x6411 INTEGER16 channels. */
} CO_401_controller_config_t;

/** Caller-owned stack-neutral runtime for one remote CiA 401 logical device. */
typedef struct {
    CO_401_controller_config_t config; /**< Copied immutable remote capability declaration. */
} CO_401_controller_t;

/**
 * @brief Initialize one transport-neutral remote CiA 401 Controller.
 *
 * The Controller owns no Node-ID, CANopen stack object, RTOS object, heap allocation or transfer state. Capability
 * counts are caller declarations, normally derived from the remote EDS/XDD/product configuration.
 *
 * @param controller Caller-owned Controller storage.
 * @param config Remote logical-device/capability declaration.
 * @return true when the configuration is valid; otherwise false and @p controller is cleared.
 */
bool CO_401_controller_init(CO_401_controller_t *controller, const CO_401_controller_config_t *config);

/**
 * @brief Resolve an arbitrary canonical CiA 401 profile scalar into one logical-device block.
 *
 * This low-level helper intentionally validates only the CiA 301 profile block and scalar width. Remote OD access,
 * object-specific sub-index rules and read/write attributes remain transport/remote-device contracts.
 *
 * @param controller Initialized Controller.
 * @param canonicalIndex Canonical profile index in 0x6000..0x67FF.
 * @param subIndex Remote sub-index.
 * @param size Expected scalar width in bytes, range 1..8.
 * @param ref Resolved absolute remote reference.
 * @return true on success; false for invalid arguments/index/size.
 */
bool CO_401_controller_profileRef(const CO_401_controller_t *controller, uint16_t canonicalIndex,
                                  uint8_t subIndex, uint8_t size, CO_profile_object_ref_t *ref);

/**
 * @brief Resolve one configured 8-bit digital-input bank at canonical Object 0x6000.
 *
 * @param controller Initialized Controller.
 * @param bank One-based remote bank number.
 * @param ref Resolved absolute remote Object Dictionary reference.
 * @return true when @p bank is configured; otherwise false.
 */
bool CO_401_controller_digitalInputRef(const CO_401_controller_t *controller, uint8_t bank,
                                       CO_profile_object_ref_t *ref);

/**
 * @brief Resolve one configured 8-bit digital-output bank at canonical Object 0x6200.
 *
 * @param controller Initialized Controller.
 * @param bank One-based remote bank number.
 * @param ref Resolved absolute remote Object Dictionary reference.
 * @return true when @p bank is configured; otherwise false.
 */
bool CO_401_controller_digitalOutputRef(const CO_401_controller_t *controller, uint8_t bank,
                                        CO_profile_object_ref_t *ref);

/**
 * @brief Resolve one configured INTEGER16 analogue-input channel at canonical Object 0x6401.
 *
 * @param controller Initialized Controller.
 * @param channel One-based remote channel number.
 * @param ref Resolved absolute remote Object Dictionary reference.
 * @return true when @p channel is configured; otherwise false.
 */
bool CO_401_controller_analogInputRef(const CO_401_controller_t *controller, uint8_t channel,
                                      CO_profile_object_ref_t *ref);

/**
 * @brief Resolve one configured INTEGER16 analogue-output channel at canonical Object 0x6411.
 *
 * @param controller Initialized Controller.
 * @param channel One-based remote channel number.
 * @param ref Resolved absolute remote Object Dictionary reference.
 * @return true when @p channel is configured; otherwise false.
 */
bool CO_401_controller_analogOutputRef(const CO_401_controller_t *controller, uint8_t channel,
                                       CO_profile_object_ref_t *ref);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_401_CONTROLLER_H_ */
