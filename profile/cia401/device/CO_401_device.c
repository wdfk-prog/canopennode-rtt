/**
 * @file CO_401_device.c
 * @brief Pure-C CiA 401 Device manager.
 */

#include <string.h>

#include "CO_401_analog.h"
#include "CO_401_device.h"
#include "CO_401_digital.h"

static void setDiag(CO_401_init_diag_t *diag, CO_401_init_error_t error, uint8_t logicalDevice)
{
    if (diag != NULL) {
        diag->error = error;
        diag->index = 0U;
        diag->subIndex = 0U;
        diag->logicalDevice = logicalDevice;
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
    if (config == NULL || config->io == NULL || capabilities == 0U
        || config->logicalDevice >= CO_401_LOGICAL_DEVICE_COUNT_MAX) {
        return CO_401_INIT_CONFIG;
    }
    if (!validCount(config->digitalInputBanks) || !validCount(config->digitalOutputBanks)
        || !validCount(config->analogInputChannels) || !validCount(config->analogOutputChannels)) {
        return CO_401_INIT_CONFIG;
    }
    if (config->digitalInputBanks != 0U && config->io->readDigital8 == NULL) {
        return CO_401_INIT_IO_IF;
    }
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    if (config->digitalInputBanks != 0U) {
        const uint16_t requestableSubIndexes = (uint16_t)OD_FLAGS_PDO_SIZE * 8U;

        if (requestableSubIndexes == 0U || (uint16_t)config->digitalInputBanks >= requestableSubIndexes) {
            return CO_401_INIT_CONFIG;
        }
        if (config->io->setDigitalInputFilter8 == NULL) {
            return CO_401_INIT_IO_IF;
        }
    }
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS */
    if (config->digitalOutputBanks != 0U && config->io->writeDigital8 == NULL) {
        return CO_401_INIT_IO_IF;
    }
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    if (config->digitalOutputBanks != 0U && config->io->writeDigital8Masked == NULL) {
        return CO_401_INIT_IO_IF;
    }
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE */
    if (config->analogInputChannels != 0U && config->io->readAnalog16 == NULL) {
        return CO_401_INIT_IO_IF;
    }
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
    if (config->analogInputChannels != 0U) {
        const uint16_t requestableSubIndexes = (uint16_t)OD_FLAGS_PDO_SIZE * 8U;
        if (requestableSubIndexes == 0U || (uint16_t)config->analogInputChannels >= requestableSubIndexes) {
            return CO_401_INIT_CONFIG;
        }
    }
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */
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
        setDiag(diag, CO_401_INIT_BAD_ARGUMENT, config != NULL ? config->logicalDevice : 0U);
        return CO_401_INIT_BAD_ARGUMENT;
    }

    capabilities = capabilitiesFromConfig(config);
    result = validateConfig(config, capabilities);
    if (result != CO_401_INIT_OK) {
        (void)memset(device, 0, sizeof(*device));
        setDiag(diag, result, config->logicalDevice);
        return result;
    }

    (void)memset(device, 0, sizeof(*device));
    device->od = od;
    device->config = *config;
    device->capabilities = capabilities;
    device->logicalDevice = config->logicalDevice;
    device->odBase = CO_401_objectIndex(config->logicalDevice, CO_401_PROFILE_INDEX_BASE);

    return CO_401_device_bindOD(device, diag);
}

void CO_401_device_notifyOutputSupervision(CO_401_device_t *device)
{
    if (device != NULL) {
        /* CiA 401 latches this prerequisite for the application lifetime, not one communication generation. */
        device->outputSupervisionReady = true;
    }
}

void CO_401_device_setOutputSupervisionProbe(CO_401_device_t *device, void *object,
                                              bool (*probe)(void *object))
{
    if (device != NULL) {
        device->outputSupervisionProbeObject = probe != NULL ? object : NULL;
        device->outputSupervisionProbe = probe;
    }
}

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
void CO_401_device_setDigitalOutputFault(CO_401_device_t *device, bool active)
{
    if (device != NULL) {
        device->digitalOutputFaultActive = active;
    }
}
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE */

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
void CO_401_device_setNmtStopped(CO_401_device_t *device, bool stopped)
{
    if (device != NULL) {
        device->nmtStopped = stopped;
    }
}

void CO_401_device_setCommunicationFault(CO_401_device_t *device, bool active)
{
    if (device != NULL) {
        device->communicationFaultActive = active;
    }
}
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */


void CO_401_device_setNmtOperational(CO_401_device_t *device, bool operational)
{
    if (device == NULL) {
        return;
    }

    if (operational && !device->nmtOperational && device->odBound
        && device->bound.analogInterruptEnable != NULL) {
        uint8_t enabled = 0U;

        if (OD_get_u8(device->bound.analogInterruptEnable, 0U, &enabled, true) == ODR_OK && enabled == 0U
            && !device->errorEventPending) {
            device->pendingErrorCode = 0x0080U;
            device->errorEventPending = true;
        }
    }
    device->nmtOperational = operational;
}

bool CO_401_device_takeErrorEvent(CO_401_device_t *device, CO_401_error_event_t *event)
{
    if (device == NULL || event == NULL || !device->errorEventPending) {
        return false;
    }

    event->errorCode = device->pendingErrorCode;
    device->pendingErrorCode = 0U;
    device->errorEventPending = false;
    return true;
}

void CO_401_device_resetCommunicationState(CO_401_device_t *device)
{
    if (device == NULL) {
        return;
    }

    /* NMT edge/pending warning ownership ends with the current communication generation. */
    device->nmtOperational = false;
    device->pendingErrorCode = 0U;
    device->errorEventPending = false;
}

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
void CO_401_device_retireDigitalInputTpdoEvent(CO_401_device_t *device, uint8_t subIndex)
{
    uint8_t bank;
    uint8_t mask;

    if (device == NULL || subIndex == 0U || subIndex > device->config.digitalInputBanks) {
        return;
    }

    bank = (uint8_t)(subIndex - 1U);
    mask = (uint8_t)(1U << (bank & 0x07U));
    device->digitalInputEventTpdoPending[bank >> 3] &= (uint8_t)(~mask);
}
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS */

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
void CO_401_device_setSdoReadMatcher(CO_401_device_t *device, void *object,
                                     bool (*matcher)(void *object, const OD_stream_t *stream))
{
    if (device != NULL) {
        device->sdoReadMatchObject = matcher != NULL ? object : NULL;
        device->sdoReadMatch = matcher;
    }
}

void CO_401_device_commitAnalogInputCommunication(CO_401_device_t *device, uint8_t subIndex, int16_t value)
{
    uint8_t channel;

    if (device == NULL || subIndex == 0U || subIndex > device->config.analogInputChannels) {
        return;
    }

    channel = (uint8_t)(subIndex - 1U);
    device->analogLastCommunicated[channel] = value;
    device->analogLastCommunicatedValid[channel] = true;
}

void CO_401_device_retireAnalogInputTpdoEvent(CO_401_device_t *device, uint8_t subIndex)
{
    uint8_t channel;
    uint8_t mask;

    if (device == NULL || subIndex == 0U || subIndex > device->config.analogInputChannels) {
        return;
    }

    channel = (uint8_t)(subIndex - 1U);
    mask = (uint8_t)(1U << (channel & 0x07U));
    device->analogInputEventTpdoPending[channel >> 3] &= (uint8_t)(~mask);
}

void CO_401_device_commitAnalogInputTpdoCommunication(CO_401_device_t *device, uint8_t subIndex, int16_t value)
{
    CO_401_device_commitAnalogInputCommunication(device, subIndex, value);
    CO_401_device_retireAnalogInputTpdoEvent(device, subIndex);
}

void CO_401_device_commitAnalogSourceCommunication(CO_401_device_t *device, uint8_t subIndex,
                                                    uint32_t communicatedBits)
{
    uint32_t current;

    if (device == NULL || !device->odBound || device->bound.analogInterruptSource == NULL
        || subIndex == 0U || communicatedBits == 0U) {
        return;
    }

    if (OD_get_u32(device->bound.analogInterruptSource, subIndex, &current, true) == ODR_OK) {
        /* Preserve bits latched after the communicated snapshot; only the transmitted/read snapshot is consumed. */
        (void)OD_set_u32(device->bound.analogInterruptSource, subIndex, current & ~communicatedBits, true);
    }
}
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
void CO_401_device_setAnalogOutputFault(CO_401_device_t *device, bool active)
{
    if (device != NULL) {
        device->analogOutputFaultActive = active;
    }
}
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */

void CO_401_device_process(CO_401_device_t *device)
{
    if (device == NULL) {
        return;
    }
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    /* This pass owns the completion snapshot consumed immediately by the RT adapter under the same OD lock. */
    device->failSafeOutputApplyComplete = false;
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
    if (!device->odBound || device->config.io == NULL) {
        return;
    }
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    device->failSafeOutputApplyComplete = true;
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */

    CO_401_digital_refreshInputs(device);
    CO_401_analog_refreshInputs(device);
    CO_401_digital_applyOutputs(device);
    CO_401_analog_applyOutputs(device);
}
