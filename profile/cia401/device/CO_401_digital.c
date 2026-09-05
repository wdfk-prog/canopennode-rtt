/**
 * @file CO_401_digital.c
 * @brief Stage-1 digital process-image exchange for the CiA 401 Device core.
 */

#include "CO_401_digital.h"

void CO_401_digital_refreshInputs(CO_401_device_t *device)
{
    uint8_t bank;

    if (device == NULL || !device->odBound || device->config.io == NULL
        || device->config.io->readDigital8 == NULL || device->bound.digitalInput8 == NULL
        || device->config.digitalInputBanks == 0U) {
        return;
    }

    for (bank = 0U; bank < device->config.digitalInputBanks; bank++) {
        uint8_t value;
        CO_401_io_result_t result = device->config.io->readDigital8(device->config.ioObject, bank, &value);

        if (result == CO_401_IO_OK) {
            (void)OD_set_u8(device->bound.digitalInput8, (uint8_t)(bank + 1U), value, true);
        }
    }
}

void CO_401_digital_applyOutputs(CO_401_device_t *device)
{
    uint8_t bank;

    if (device == NULL || !device->odBound || device->config.io == NULL
        || device->config.io->writeDigital8 == NULL || device->bound.digitalOutput8 == NULL
        || device->config.digitalOutputBanks == 0U) {
        return;
    }

    for (bank = 0U; bank < device->config.digitalOutputBanks; bank++) {
        uint8_t value;

        if (OD_get_u8(device->bound.digitalOutput8, (uint8_t)(bank + 1U), &value, true) == ODR_OK) {
            (void)device->config.io->writeDigital8(device->config.ioObject, bank, value);
        }
    }
}
