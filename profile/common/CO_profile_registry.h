/**
 * @file CO_profile_registry.h
 * @brief Profile-neutral logical-device ownership validation for one CANopen node.
 */

#ifndef CO_PROFILE_REGISTRY_H_
#define CO_PROFILE_REGISTRY_H_

#include <stdbool.h>
#include <stdint.h>

#include "CO_profile_layout.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** CANopen device-profile number stored in the low 16 bits of a Device Type value. */
typedef uint16_t CO_profile_number_t;

/** One product declaration assigning a CANopen Device Profile to a logical-device slot. */
typedef struct {
    uint8_t logicalDevice;               /**< Logical-device slot in range 0..7. */
    CO_profile_number_t profileNumber;   /**< Non-zero CANopen Device Profile number. */
} CO_profile_descriptor_t;

/** Result of validating and publishing one complete logical-device ownership table. */
typedef enum {
    CO_PROFILE_REGISTRY_OK = 0,
    CO_PROFILE_REGISTRY_BAD_ARGUMENT,
    CO_PROFILE_REGISTRY_INVALID_LOGICAL_DEVICE,
    CO_PROFILE_REGISTRY_INVALID_PROFILE_NUMBER,
    CO_PROFILE_REGISTRY_DUPLICATE_SLOT,
    CO_PROFILE_REGISTRY_DEVICE_TYPE_UNAVAILABLE,
    CO_PROFILE_REGISTRY_DEVICE_TYPE_MISMATCH
} CO_profile_registry_result_t;

/** Descriptor index used when no failing descriptor exists. */
#define CO_PROFILE_REGISTRY_DESCRIPTOR_NONE UINT8_MAX

/** Optional detail describing the first validation failure. */
typedef struct {
    CO_profile_registry_result_t result; /**< Validation result. */
    uint8_t descriptorIndex;             /**< Index in the caller descriptor table or 0xFF. */
    uint8_t logicalDevice;               /**< Logical-device slot associated with the failure. */
    CO_profile_number_t profileNumber;   /**< Declared profile number associated with the failure. */
    uint32_t deviceType;                 /**< Device Type value read from the generated OD when available. */
} CO_profile_registry_diag_t;

/**
 * @brief Read one logical device's 32-bit Device Type value.
 *
 * The callback lets the Pure-C registry validate generated OD/XDD capability without
 * depending on a specific Object Dictionary implementation or Device Profile module.
 *
 * @param object Caller-owned reader context.
 * @param logicalDevice Logical-device slot in range 0..7.
 * @param deviceType Receives the complete 32-bit Device Type value.
 * @return true when the value was read successfully, otherwise false.
 */
typedef bool (*CO_profile_device_type_reader_t)(void *object, uint8_t logicalDevice, uint32_t *deviceType);

/** Fixed-capacity ownership state for all logical-device slots in one CANopen node. */
typedef struct {
    CO_profile_number_t owner[CO_PROFILE_LOGICAL_DEVICE_COUNT_MAX]; /**< Profile number per slot, zero when unowned. */
    uint8_t count;                                                   /**< Number of owned logical-device slots. */
} CO_profile_registry_t;

/**
 * @brief Clear every logical-device owner so subsequent queries fail closed.
 *
 * @param registry Registry to clear; NULL is ignored.
 */
void CO_profileRegistryClear(CO_profile_registry_t *registry);

/**
 * @brief Validate and transactionally publish one complete logical-device ownership table.
 *
 * Validation checks slot range, non-zero profile numbers, duplicate ownership and the
 * low 16-bit profile number in each generated Device Type value. The destination stays
 * empty if any descriptor fails, so caller order cannot leave partial ownership behind.
 * A zero-descriptor table is valid and produces an empty registry. This API provides no
 * concurrent synchronization; callers must serialize build/clear/query access to one registry.
 *
 * @param registry Destination ownership registry.
 * @param descriptors Complete caller-owned descriptor table, or NULL when @p descriptorCount is zero.
 * @param descriptorCount Number of entries in @p descriptors, maximum 8.
 * @param readDeviceType Device-Type reader, required when @p descriptorCount is non-zero.
 * @param readerObject Caller-owned context passed to @p readDeviceType.
 * @param diag Optional first-failure detail.
 * @return CO_PROFILE_REGISTRY_OK on success, otherwise the first validation error.
 */
CO_profile_registry_result_t CO_profileRegistryBuild(
    CO_profile_registry_t *registry,
    const CO_profile_descriptor_t *descriptors,
    uint8_t descriptorCount,
    CO_profile_device_type_reader_t readDeviceType,
    void *readerObject,
    CO_profile_registry_diag_t *diag);

/**
 * @brief Return the profile number that owns one logical-device slot.
 *
 * @param registry Published ownership registry.
 * @param logicalDevice Logical-device slot in range 0..7.
 * @return Non-zero profile number for an owned slot, otherwise zero.
 */
CO_profile_number_t CO_profileRegistryOwner(const CO_profile_registry_t *registry, uint8_t logicalDevice);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_PROFILE_REGISTRY_H_ */
