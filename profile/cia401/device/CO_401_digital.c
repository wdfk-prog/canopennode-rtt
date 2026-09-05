/**
 * @file CO_401_digital.c
 * @brief Digital process-image exchange for the CiA 401 Device core.
 */

#include "CO_401_digital.h"

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
static bool forwardInputFilterConfiguration(CO_401_device_t *device)
{
    uint8_t bank;
    bool allApplied = true;

    if (!device->digitalInputFilterDirty) {
        return true;
    }
    if (device->bound.digitalInputFilter8 == NULL || device->config.io->setDigitalInputFilter8 == NULL) {
        return false;
    }

    /* 0x6003 OD writes only mark dirty; product I/O stays deferred to the bounded process context. */
    for (bank = 0U; bank < device->config.digitalInputBanks; bank++) {
        uint8_t enabledMask;

        if (OD_get_u8(device->bound.digitalInputFilter8, (uint8_t)(bank + 1U), &enabledMask, true) != ODR_OK
            || device->config.io->setDigitalInputFilter8(device->config.ioObject, bank, enabledMask) != CO_401_IO_OK) {
            allApplied = false;
        }
    }

    if (allApplied) {
        device->digitalInputFilterDirty = false;
    }
    return allApplied;
}

static bool getInputEventConfiguration(CO_401_device_t *device, uint8_t subIndex, uint8_t *polarity,
                                       uint8_t *anyMask, uint8_t *risingMask, uint8_t *fallingMask)
{
    return OD_get_u8(device->bound.digitalInputPolarity8, subIndex, polarity, true) == ODR_OK
        && OD_get_u8(device->bound.digitalInterruptAny8, subIndex, anyMask, true) == ODR_OK
        && OD_get_u8(device->bound.digitalInterruptRising8, subIndex, risingMask, true) == ODR_OK
        && OD_get_u8(device->bound.digitalInterruptFalling8, subIndex, fallingMask, true) == ODR_OK;
}
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS */

void CO_401_digital_refreshInputs(CO_401_device_t *device)
{
    uint8_t bank;
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    uint8_t interruptEnable = 0U;
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS */

    if (device == NULL || !device->odBound || device->config.io == NULL
        || device->config.io->readDigital8 == NULL || device->bound.digitalInput8 == NULL
        || device->config.digitalInputBanks == 0U) {
        return;
    }

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    /* Do not publish input values while 0x6003 and the product filter state disagree. */
    if (!forwardInputFilterConfiguration(device)) {
        return;
    }
    if (device->bound.digitalInterruptEnable != NULL) {
        (void)OD_get_u8(device->bound.digitalInterruptEnable, 0U, &interruptEnable, true);
    }
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS */

    for (bank = 0U; bank < device->config.digitalInputBanks; bank++) {
        uint8_t value;
        const uint8_t subIndex = (uint8_t)(bank + 1U);
        CO_401_io_result_t result = device->config.io->readDigital8(device->config.ioObject, bank, &value);

        if (result != CO_401_IO_OK) {
            continue;
        }

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
        uint8_t previous;
        uint8_t polarity;
        uint8_t anyMask;
        uint8_t risingMask;
        uint8_t fallingMask;

        if (OD_get_u8(device->bound.digitalInput8, subIndex, &previous, true) != ODR_OK
            || !getInputEventConfiguration(device, subIndex, &polarity, &anyMask, &risingMask, &fallingMask)) {
            continue;
        }

        value ^= polarity;
        if (OD_set_u8(device->bound.digitalInput8, subIndex, value, true) != ODR_OK) {
            continue;
        }

        if (interruptEnable != 0U) {
            const uint8_t changed = (uint8_t)(previous ^ value);
            const uint8_t rising = (uint8_t)((uint8_t)(~previous) & value);
            const uint8_t falling = (uint8_t)(previous & (uint8_t)(~value));
            const uint8_t eventBits = (uint8_t)((changed & anyMask) | (rising & risingMask)
                                                | (falling & fallingMask));

            if (eventBits != 0U) {
                /* Request by OD entry/sub-index so any current TPDO mapping observes the event. */
                OD_requestTPDO(device->bound.digitalInput8, subIndex);
            }
        }
#else
        (void)OD_set_u8(device->bound.digitalInput8, subIndex, value, true);
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS */
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
        const uint8_t subIndex = (uint8_t)(bank + 1U);

        if (OD_get_u8(device->bound.digitalOutput8, subIndex, &value, true) != ODR_OK) {
            continue;
        }

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
        uint8_t polarity;
        uint8_t filterMask;

        if (OD_get_u8(device->bound.digitalOutputPolarity8, subIndex, &polarity, true) != ODR_OK
            || OD_get_u8(device->bound.digitalOutputFilter8, subIndex, &filterMask, true) != ODR_OK) {
            continue;
        }

        if (device->digitalOutputFaultActive) {
            uint8_t errorMode;
            uint8_t errorValue;
            uint8_t updateMask;

            if (OD_get_u8(device->bound.digitalOutputErrorMode8, subIndex, &errorMode, true) != ODR_OK
                || OD_get_u8(device->bound.digitalOutputErrorValue8, subIndex, &errorValue, true) != ODR_OK) {
                continue;
            }
            updateMask = (uint8_t)(errorMode & filterMask);
            if (updateMask == 0U) {
                /* 0x6206 keep bits and 0x6208 blocked bits both retain their current physical state. */
                continue;
            }

            /* CiA 401 applies fault selection before polarity and the final 0x6208 physical-output filter. */
            value = (uint8_t)(errorValue ^ polarity);
            (void)device->config.io->writeDigital8Masked(device->config.ioObject, bank, value, updateMask);
            continue;
        }

        value ^= polarity;
        if (filterMask == 0U) {
            continue;
        }
        if (filterMask != UINT8_MAX) {
            /* The backend owns the old physical value required by 0x6208 bits that are filtered out. */
            (void)device->config.io->writeDigital8Masked(device->config.ioObject, bank, value, filterMask);
            continue;
        }
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE */

        (void)device->config.io->writeDigital8(device->config.ioObject, bank, value);
    }
}
