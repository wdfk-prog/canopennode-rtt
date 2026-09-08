/**
 * @file CO_profile_layout.h
 * @brief CiA 301 multi-logical-device profile layout helpers.
 */

#ifndef CO_PROFILE_LAYOUT_H
#define CO_PROFILE_LAYOUT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of logical-device profile slots in one CANopen device. */
#define CO_PROFILE_LOGICAL_DEVICE_COUNT_MAX 8U

/** First index in the canonical application-profile block. */
#define CO_PROFILE_OD_BASE 0x6000U
/** Offset between adjacent logical-device application-profile blocks. */
#define CO_PROFILE_OD_STRIDE 0x0800U
/** Last index in the canonical application-profile block. */
#define CO_PROFILE_OD_CANONICAL_LAST 0x67FFU

/** Number of PDO numbers reserved for each logical-device slot. */
#define CO_PROFILE_PDOS_PER_LOGICAL_DEVICE 64U

/** Invalid Object Dictionary index returned by profile-layout helpers. */
#define CO_PROFILE_INDEX_INVALID 0U
/** Invalid PDO number returned by profile-layout helpers. */
#define CO_PROFILE_PDO_NUMBER_INVALID 0U

/**
 * @brief Check whether a logical-device slot is valid.
 *
 * @param logicalDevice Zero-based logical-device slot.
 * @return true when @p logicalDevice is in the supported range; otherwise false.
 */
static inline bool CO_profileLogicalDeviceValid(uint8_t logicalDevice)
{
    return logicalDevice < CO_PROFILE_LOGICAL_DEVICE_COUNT_MAX;
}

/**
 * @brief Check whether an index belongs to the canonical application-profile block.
 *
 * @param canonicalIndex Canonical profile index before logical-device translation.
 * @return true for indices from 0x6000 through 0x67FF; otherwise false.
 */
static inline bool CO_profileCanonicalIndexValid(uint16_t canonicalIndex)
{
    return canonicalIndex >= CO_PROFILE_OD_BASE && canonicalIndex <= CO_PROFILE_OD_CANONICAL_LAST;
}

/**
 * @brief Translate a canonical application-profile index into one logical-device block.
 *
 * @param logicalDevice Zero-based logical-device slot.
 * @param canonicalIndex Canonical Object Dictionary index in the 0x6000..0x67FF block.
 * @return Translated Object Dictionary index, or CO_PROFILE_INDEX_INVALID for invalid input.
 */
static inline uint16_t CO_profileIndex(uint8_t logicalDevice, uint16_t canonicalIndex)
{
    if (!CO_profileLogicalDeviceValid(logicalDevice) || !CO_profileCanonicalIndexValid(canonicalIndex)) {
        return CO_PROFILE_INDEX_INVALID;
    }

    return (uint16_t)(canonicalIndex + ((uint16_t)logicalDevice * (uint16_t)CO_PROFILE_OD_STRIDE));
}

/**
 * @brief Resolve the logical-device Device type index.
 *
 * @param logicalDevice Zero-based logical-device slot.
 * @return Device type Object Dictionary index, or CO_PROFILE_INDEX_INVALID for an invalid slot.
 */
static inline uint16_t CO_profileDeviceTypeIndex(uint8_t logicalDevice)
{
    return CO_profileIndex(logicalDevice, CO_PROFILE_OD_CANONICAL_LAST);
}

/**
 * @brief Translate a profile-local PDO number into the CANopen-device PDO number.
 *
 * @param logicalDevice Zero-based logical-device slot.
 * @param localPdoNumber Profile-local PDO number in the inclusive range 1..64.
 * @return Device-wide PDO number, or CO_PROFILE_PDO_NUMBER_INVALID for invalid input.
 */
static inline uint16_t CO_profilePdoNumber(uint8_t logicalDevice, uint16_t localPdoNumber)
{
    if (!CO_profileLogicalDeviceValid(logicalDevice)
        || localPdoNumber == 0U
        || localPdoNumber > CO_PROFILE_PDOS_PER_LOGICAL_DEVICE) {
        return CO_PROFILE_PDO_NUMBER_INVALID;
    }

    return (uint16_t)(((uint16_t)logicalDevice * (uint16_t)CO_PROFILE_PDOS_PER_LOGICAL_DEVICE)
                      + localPdoNumber);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_PROFILE_LAYOUT_H */
