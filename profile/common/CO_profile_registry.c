/**
 * @file CO_profile_registry.c
 * @brief Profile-neutral logical-device ownership validation for one CANopen node.
 */

#include "CO_profile_registry.h"

#include <string.h>

static void diagReset(CO_profile_registry_diag_t *diag)
{
    if (diag != NULL) {
        (void)memset(diag, 0, sizeof(*diag));
        diag->result = CO_PROFILE_REGISTRY_OK;
        diag->descriptorIndex = CO_PROFILE_REGISTRY_DESCRIPTOR_NONE;
        diag->logicalDevice = CO_PROFILE_LOGICAL_DEVICE_COUNT_MAX;
    }
}

static CO_profile_registry_result_t fail(
    CO_profile_registry_t *registry,
    CO_profile_registry_diag_t *diag,
    CO_profile_registry_result_t result,
    uint8_t descriptorIndex,
    uint8_t logicalDevice,
    CO_profile_number_t profileNumber,
    uint32_t deviceType)
{
    CO_profileRegistryClear(registry);
    if (diag != NULL) {
        diag->result = result;
        diag->descriptorIndex = descriptorIndex;
        diag->logicalDevice = logicalDevice;
        diag->profileNumber = profileNumber;
        diag->deviceType = deviceType;
    }
    return result;
}

void CO_profileRegistryClear(CO_profile_registry_t *registry)
{
    if (registry != NULL) {
        (void)memset(registry, 0, sizeof(*registry));
    }
}

CO_profile_registry_result_t CO_profileRegistryBuild(
    CO_profile_registry_t *registry,
    const CO_profile_descriptor_t *descriptors,
    uint8_t descriptorCount,
    CO_profile_device_type_reader_t readDeviceType,
    void *readerObject,
    CO_profile_registry_diag_t *diag)
{
    CO_profile_registry_t candidate;
    uint8_t i;

    diagReset(diag);
    if (registry == NULL) {
        return fail(NULL, diag, CO_PROFILE_REGISTRY_BAD_ARGUMENT, CO_PROFILE_REGISTRY_DESCRIPTOR_NONE,
                    CO_PROFILE_LOGICAL_DEVICE_COUNT_MAX, 0U, 0U);
    }

    CO_profileRegistryClear(registry);
    if (descriptorCount > CO_PROFILE_LOGICAL_DEVICE_COUNT_MAX
        || (descriptorCount > 0U && (descriptors == NULL || readDeviceType == NULL))) {
        return fail(registry, diag, CO_PROFILE_REGISTRY_BAD_ARGUMENT, CO_PROFILE_REGISTRY_DESCRIPTOR_NONE,
                    CO_PROFILE_LOGICAL_DEVICE_COUNT_MAX, 0U, 0U);
    }

    (void)memset(&candidate, 0, sizeof(candidate));
    for (i = 0U; i < descriptorCount; i++) {
        const CO_profile_descriptor_t *descriptor = &descriptors[i];
        uint32_t deviceType = 0U;

        if (!CO_profileLogicalDeviceValid(descriptor->logicalDevice)) {
            return fail(registry, diag, CO_PROFILE_REGISTRY_INVALID_LOGICAL_DEVICE, i,
                        descriptor->logicalDevice, descriptor->profileNumber, 0U);
        }
        if (descriptor->profileNumber == 0U) {
            return fail(registry, diag, CO_PROFILE_REGISTRY_INVALID_PROFILE_NUMBER, i,
                        descriptor->logicalDevice, descriptor->profileNumber, 0U);
        }
        if (candidate.owner[descriptor->logicalDevice] != 0U) {
            return fail(registry, diag, CO_PROFILE_REGISTRY_DUPLICATE_SLOT, i,
                        descriptor->logicalDevice, descriptor->profileNumber, 0U);
        }
        if (!readDeviceType(readerObject, descriptor->logicalDevice, &deviceType)) {
            return fail(registry, diag, CO_PROFILE_REGISTRY_DEVICE_TYPE_UNAVAILABLE, i,
                        descriptor->logicalDevice, descriptor->profileNumber, 0U);
        }
        if ((CO_profile_number_t)(deviceType & UINT16_MAX) != descriptor->profileNumber) {
            return fail(registry, diag, CO_PROFILE_REGISTRY_DEVICE_TYPE_MISMATCH, i,
                        descriptor->logicalDevice, descriptor->profileNumber, deviceType);
        }

        candidate.owner[descriptor->logicalDevice] = descriptor->profileNumber;
        candidate.count++;
    }

    *registry = candidate;
    return CO_PROFILE_REGISTRY_OK;
}

CO_profile_number_t CO_profileRegistryOwner(const CO_profile_registry_t *registry, uint8_t logicalDevice)
{
    if (registry == NULL || !CO_profileLogicalDeviceValid(logicalDevice)) {
        return 0U;
    }
    return registry->owner[logicalDevice];
}
