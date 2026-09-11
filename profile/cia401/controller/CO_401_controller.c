/**
 * @file CO_401_controller.c
 * @brief Pure-C CiA 401 remote I/O Controller object-resolution implementation.
 */

#include <string.h>

#include "CO_401_controller.h"
#include "CO_401_objects.h"
#include "CO_profile_layout.h"

bool CO_401_controller_init(CO_401_controller_t *controller, const CO_401_controller_config_t *config)
{
    if (controller == NULL || config == NULL || !CO_profileLogicalDeviceValid(config->logicalDevice)
        || config->digitalInputBanks > CO_401_PROCESS_IMAGE_COUNT_MAX
        || config->digitalOutputBanks > CO_401_PROCESS_IMAGE_COUNT_MAX
        || config->analogInputChannels > CO_401_PROCESS_IMAGE_COUNT_MAX
        || config->analogOutputChannels > CO_401_PROCESS_IMAGE_COUNT_MAX
        || (config->digitalInputBanks == 0U && config->digitalOutputBanks == 0U
            && config->analogInputChannels == 0U && config->analogOutputChannels == 0U)) {
        if (controller != NULL) {
            (void)memset(controller, 0, sizeof(*controller));
        }
        return false;
    }

    (void)memset(controller, 0, sizeof(*controller));
    controller->config = *config;
    return true;
}

bool CO_401_controller_profileRef(const CO_401_controller_t *controller, uint16_t canonicalIndex,
                                  uint8_t subIndex, uint8_t size, CO_profile_object_ref_t *ref)
{
    uint16_t index;

    if (controller == NULL || ref == NULL || size == 0U || size > CO_PROFILE_CONTROLLER_VALUE_SIZE_MAX) {
        return false;
    }
    index = CO_profileIndex(controller->config.logicalDevice, canonicalIndex);
    if (index == CO_PROFILE_INDEX_INVALID) {
        return false;
    }

    ref->index = index;
    ref->subIndex = subIndex;
    ref->size = size;
    return true;
}

/** Resolve one 1-based configured array element. */
static bool arrayRef(const CO_401_controller_t *controller, uint16_t canonicalIndex, uint8_t element,
                     uint8_t count, uint8_t size, CO_profile_object_ref_t *ref)
{
    if (element == 0U || element > count) {
        return false;
    }
    return CO_401_controller_profileRef(controller, canonicalIndex, element, size, ref);
}

bool CO_401_controller_digitalInputRef(const CO_401_controller_t *controller, uint8_t bank,
                                       CO_profile_object_ref_t *ref)
{
    return controller != NULL
        && arrayRef(controller, CO_401_INDEX_DIGITAL_INPUT_8, bank, controller->config.digitalInputBanks, 1U, ref);
}

bool CO_401_controller_digitalOutputRef(const CO_401_controller_t *controller, uint8_t bank,
                                        CO_profile_object_ref_t *ref)
{
    return controller != NULL
        && arrayRef(controller, CO_401_INDEX_DIGITAL_OUTPUT_8, bank, controller->config.digitalOutputBanks, 1U, ref);
}

bool CO_401_controller_analogInputRef(const CO_401_controller_t *controller, uint8_t channel,
                                      CO_profile_object_ref_t *ref)
{
    return controller != NULL
        && arrayRef(controller, CO_401_INDEX_ANALOG_INPUT_16, channel, controller->config.analogInputChannels,
                    2U, ref);
}

bool CO_401_controller_analogOutputRef(const CO_401_controller_t *controller, uint8_t channel,
                                       CO_profile_object_ref_t *ref)
{
    return controller != NULL
        && arrayRef(controller, CO_401_INDEX_ANALOG_OUTPUT_16, channel, controller->config.analogOutputChannels,
                    2U, ref);
}
