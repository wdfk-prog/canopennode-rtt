/**
 * @file CO_401_analog.c
 * @brief Stage-1 analogue process-image exchange for the CiA 401 Device core.
 */

#include "CO_401_analog.h"

void CO_401_analog_refreshInputs(CO_401_device_t *device)
{
    uint8_t channel;

    if (device == NULL || !device->odBound || device->config.io == NULL
        || device->config.io->readAnalog16 == NULL || device->bound.analogInput16 == NULL
        || device->config.analogInputChannels == 0U) {
        return;
    }

    for (channel = 0U; channel < device->config.analogInputChannels; channel++) {
        int16_t value;
        CO_401_io_result_t result = device->config.io->readAnalog16(device->config.ioObject, channel, &value);

        if (result == CO_401_IO_OK) {
            (void)OD_set_i16(device->bound.analogInput16, (uint8_t)(channel + 1U), value, true);
        }
    }
}

void CO_401_analog_applyOutputs(CO_401_device_t *device)
{
    uint8_t channel;

    if (device == NULL || !device->odBound || device->config.io == NULL
        || device->config.io->writeAnalog16 == NULL || device->bound.analogOutput16 == NULL
        || device->config.analogOutputChannels == 0U) {
        return;
    }

    for (channel = 0U; channel < device->config.analogOutputChannels; channel++) {
        int16_t value;

        if (OD_get_i16(device->bound.analogOutput16, (uint8_t)(channel + 1U), &value, true) == ODR_OK) {
            (void)device->config.io->writeAnalog16(device->config.ioObject, channel, value);
        }
    }
}
