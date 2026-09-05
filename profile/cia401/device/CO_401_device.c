/**
 * @file CO_401_device.c
 * @brief Pure-C Stage-1 CiA 401 Device manager.
 */

#include <string.h>

#include "CO_401_analog.h"
#include "CO_401_device.h"
#include "CO_401_digital.h"

static void setDiag(CO_401_init_diag_t *diag, CO_401_init_error_t error)
{
    if (diag != NULL) {
        diag->error = error;
        diag->index = 0U;
        diag->subIndex = 0U;
    }
}

static bool validCount(uint8_t count)
{
    return count <= CO_401_PROCESS_IMAGE_COUNT_MAX;
}

static CO_401_capabilities_t capabilitiesFromConfig(const CO_401_device_config_t *config)
{
    CO_401_capabilities_t capabilities = 0U;

    if (config->digitalInputBanks != 0U) {
        capabilities |= CO_401_CAP_DIGITAL_INPUT;
    }
    if (config->digitalOutputBanks != 0U) {
        capabilities |= CO_401_CAP_DIGITAL_OUTPUT;
    }
    if (config->analogInputChannels != 0U) {
        capabilities |= CO_401_CAP_ANALOG_INPUT;
    }
    if (config->analogOutputChannels != 0U) {
        capabilities |= CO_401_CAP_ANALOG_OUTPUT;
    }

    return capabilities;
}

static CO_401_init_error_t validateConfig(const CO_401_device_config_t *config, CO_401_capabilities_t capabilities)
{
    if (config == NULL || config->io == NULL || capabilities == 0U) {
        return CO_401_INIT_CONFIG;
    }
    if (!validCount(config->digitalInputBanks) || !validCount(config->digitalOutputBanks)
        || !validCount(config->analogInputChannels) || !validCount(config->analogOutputChannels)) {
        return CO_401_INIT_CONFIG;
    }
    if (config->digitalInputBanks != 0U && config->io->readDigital8 == NULL) {
        return CO_401_INIT_IO_IF;
    }
    if (config->digitalOutputBanks != 0U && config->io->writeDigital8 == NULL) {
        return CO_401_INIT_IO_IF;
    }
    if (config->analogInputChannels != 0U && config->io->readAnalog16 == NULL) {
        return CO_401_INIT_IO_IF;
    }
    if (config->analogOutputChannels != 0U && config->io->writeAnalog16 == NULL) {
        return CO_401_INIT_IO_IF;
    }

    return CO_401_INIT_OK;
}

CO_401_init_error_t CO_401_device_init(CO_401_device_t *device, OD_t *od,
                                        const CO_401_device_config_t *config, CO_401_init_diag_t *diag)
{
    CO_401_capabilities_t capabilities;
    CO_401_init_error_t result;

    if (device == NULL || od == NULL || config == NULL) {
        if (device != NULL) {
            (void)memset(device, 0, sizeof(*device));
        }
        setDiag(diag, CO_401_INIT_BAD_ARGUMENT);
        return CO_401_INIT_BAD_ARGUMENT;
    }

    capabilities = capabilitiesFromConfig(config);
    result = validateConfig(config, capabilities);
    if (result != CO_401_INIT_OK) {
        (void)memset(device, 0, sizeof(*device));
        setDiag(diag, result);
        return result;
    }

    (void)memset(device, 0, sizeof(*device));
    device->od = od;
    device->config = *config;
    device->capabilities = capabilities;

    return CO_401_device_bindOD(device, diag);
}

void CO_401_device_process(CO_401_device_t *device)
{
    if (device == NULL || !device->odBound || device->config.io == NULL) {
        return;
    }

    CO_401_digital_refreshInputs(device);
    CO_401_analog_refreshInputs(device);
    CO_401_digital_applyOutputs(device);
    CO_401_analog_applyOutputs(device);
}
