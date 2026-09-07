/**
 * @file CO_401_analog.c
 * @brief Analogue process-image, event and fail-safe helpers for the CiA 401 Device core.
 */

#include <stdint.h>

#include "CO_401_analog.h"

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING)
static bool conditionProcess16Checked(int16_t base, int32_t scaling, int32_t offset, int16_t *conditioned)
{
    const int64_t widened = ((int64_t)base * (int64_t)scaling) + (int64_t)offset;

    /* 0x6431/0x6446 are ordinary Integer32 offsets; do not left-adjust them with the 16-bit process value. */
    if (widened < INT16_MIN || widened > INT16_MAX) {
        return false;
    }

    *conditioned = (int16_t)widened;
    return true;
}
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING */

static bool applyInputConditioning(CO_401_device_t *device, uint8_t subIndex, int16_t raw, int16_t *value)
{
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING)
    int32_t offset = 0;
    int32_t scaling = 1;
    int16_t conditioned;

    if (device->bound.analogInputOffset32 == NULL || device->bound.analogInputPrescaling32 == NULL
        || OD_get_i32(device->bound.analogInputOffset32, subIndex, &offset, true) != ODR_OK
        || OD_get_i32(device->bound.analogInputPrescaling32, subIndex, &scaling, true) != ODR_OK) {
        return false;
    }

    if (!conditionProcess16Checked(raw, scaling, offset, &conditioned)) {
        return false;
    }
    *value = conditioned;
#else
    (void)device;
    (void)subIndex;
    *value = raw;
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING */
    return true;
}

static bool applyOutputConditioning(CO_401_device_t *device, uint8_t subIndex, int16_t command, int16_t *value)
{
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING)
    int32_t offset = 0;
    int32_t scaling = 1;
    int16_t conditioned;

    if (device->bound.analogOutputOffset32 == NULL || device->bound.analogOutputScaling32 == NULL
        || OD_get_i32(device->bound.analogOutputOffset32, subIndex, &offset, true) != ODR_OK
        || OD_get_i32(device->bound.analogOutputScaling32, subIndex, &scaling, true) != ODR_OK) {
        return false;
    }

    if (!conditionProcess16Checked(command, scaling, offset, &conditioned)) {
        return false;
    }
    *value = conditioned;
#else
    (void)device;
    (void)subIndex;
    *value = command;
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING */
    return true;
}

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
static void latchAnalogEvent(CO_401_device_t *device, uint8_t channel, uint8_t subIndex, int16_t value)
{
    uint8_t trigger = 0U;
    uint8_t enabled = 0U;
    int32_t upper = 0;
    int32_t lower = 0;
    uint32_t delta = 0U;
    uint32_t negativeDelta = 0U;
    uint32_t positiveDelta = 0U;
    const int64_t current = (int64_t)value * 65536LL;
    bool fired = false;

    if (OD_get_u8(device->bound.analogInterruptEnable, 0U, &enabled, true) != ODR_OK || enabled == 0U
        || OD_get_u8(device->bound.analogInterruptTrigger, subIndex, &trigger, true) != ODR_OK) {
        return;
    }

    /* 0x6421 == 0 means no additional comparator filter; the changed input itself is the event. */
    fired = trigger == 0U;

    if ((trigger & 0x01U) != 0U
        && OD_get_i32(device->bound.analogInterruptUpper32, subIndex, &upper, true) == ODR_OK
        && current >= (int64_t)upper) {
        fired = true;
    }
    if ((trigger & 0x02U) != 0U
        && OD_get_i32(device->bound.analogInterruptLower32, subIndex, &lower, true) == ODR_OK
        && current < (int64_t)lower) {
        fired = true;
    }

    if (device->analogLastCommunicatedValid[channel]) {
        const int64_t previous = (int64_t)device->analogLastCommunicated[channel] * 65536LL;
        const uint64_t rising = current > previous ? (uint64_t)(current - previous) : 0U;
        const uint64_t falling = previous > current ? (uint64_t)(previous - current) : 0U;
        const uint64_t absolute = rising != 0U ? rising : falling;

        if ((trigger & 0x04U) != 0U
            && OD_get_u32(device->bound.analogInterruptDeltaU32, subIndex, &delta, true) == ODR_OK
            && absolute > (uint64_t)delta) {
            fired = true;
        }
        if ((trigger & 0x08U) != 0U
            && OD_get_u32(device->bound.analogInterruptNegDeltaU32, subIndex, &negativeDelta, true) == ODR_OK
            && falling > (uint64_t)negativeDelta) {
            fired = true;
        }
        if ((trigger & 0x10U) != 0U
            && OD_get_u32(device->bound.analogInterruptPosDeltaU32, subIndex, &positiveDelta, true) == ODR_OK
            && rising > (uint64_t)positiveDelta) {
            fired = true;
        }
    }

    if (fired) {
        const uint8_t sourceSub = (uint8_t)((channel / 32U) + 1U);
        const uint32_t bit = (uint32_t)1UL << (channel % 32U);
        const uint8_t pendingMask = (uint8_t)(1U << (channel & 0x07U));
        uint32_t sources = 0U;

        if (OD_get_u32(device->bound.analogInterruptSource, sourceSub, &sources, true) == ODR_OK) {
            (void)OD_set_u32(device->bound.analogInterruptSource, sourceSub, sources | bit, true);
        }
        device->analogInputEventTpdoPending[channel >> 3] |= pendingMask;
    }
}

static void requestPendingAnalogInputTpdo(CO_401_device_t *device, uint8_t channel, uint8_t subIndex)
{
    const uint8_t pendingMask = (uint8_t)(1U << (channel & 0x07U));

    if ((device->analogInputEventTpdoPending[channel >> 3] & pendingMask) != 0U) {
        /*
         * CANopenNode consumes an OD request while preparing the TPDO, before the
         * CO_CANsend() result is known. Reassert it until transport success retires
         * this profile-level marker.
         */
        OD_requestTPDO(device->bound.analogInput16, subIndex);
    }
}

static void requestPendingAnalogSourceTpdos(CO_401_device_t *device)
{
    const uint8_t sourceBanks = (uint8_t)((device->config.analogInputChannels + 31U) / 32U);
    uint8_t bank;

    for (bank = 0U; bank < sourceBanks; bank++) {
        const uint8_t sourceSub = (uint8_t)(bank + 1U);
        uint32_t sources = 0U;

        if (OD_get_u32(device->bound.analogInterruptSource, sourceSub, &sources, true) == ODR_OK && sources != 0U) {
            OD_requestTPDO(device->bound.analogInterruptSource, sourceSub);
        }
    }
}
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */

void CO_401_analog_refreshInputs(CO_401_device_t *device)
{
    uint8_t channel;

    if (device == NULL || !device->odBound || device->config.io == NULL
        || device->config.io->readAnalog16 == NULL || device->bound.analogInput16 == NULL
        || device->config.analogInputChannels == 0U) {
        return;
    }

    for (channel = 0U; channel < device->config.analogInputChannels; channel++) {
        const uint8_t subIndex = (uint8_t)(channel + 1U);
        int16_t raw;
        int16_t value;
        int16_t previous = 0;
        CO_401_io_result_t result = device->config.io->readAnalog16(device->config.ioObject, channel, &raw);

        if (result == CO_401_IO_OK && applyInputConditioning(device, subIndex, raw, &value)) {
            (void)OD_get_i16(device->bound.analogInput16, subIndex, &previous, true);
            (void)OD_set_i16(device->bound.analogInput16, subIndex, value, true);
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
            if (value != previous) {
                latchAnalogEvent(device, channel, subIndex, value);
            }
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */
        }
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
        requestPendingAnalogInputTpdo(device, channel, subIndex);
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */
    }
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
    requestPendingAnalogSourceTpdos(device);
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */
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
        const uint8_t subIndex = (uint8_t)(channel + 1U);
        int16_t command;
        int16_t physical;

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
        const bool failSafeActive =
            device->analogOutputFaultActive || device->nmtStopped || device->communicationFaultActive;
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */

        if (OD_get_i16(device->bound.analogOutput16, subIndex, &command, true) != ODR_OK) {
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
            if (failSafeActive) {
                device->failSafeOutputApplyComplete = false;
            }
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
            continue;
        }

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
        if (failSafeActive) {
            uint8_t errorMode = 1U;

            if (OD_get_u8(device->bound.analogOutputErrorMode, subIndex, &errorMode, true) != ODR_OK) {
                device->failSafeOutputApplyComplete = false;
                continue;
            }
            if (errorMode == 0U) {
                continue;
            }
            if (errorMode == 1U) {
                int32_t errorValue = 0;

                if (OD_get_i32(device->bound.analogOutputErrorValue32, subIndex, &errorValue, true) != ODR_OK) {
                    device->failSafeOutputApplyComplete = false;
                    continue;
                }
                physical = (int16_t)(errorValue >> 16);
            } else {
                device->failSafeOutputApplyComplete = false;
                continue;
            }
        } else
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
        if (!applyOutputConditioning(device, subIndex, command, &physical)) {
            continue;
        }

        if (device->config.io->writeAnalog16(device->config.ioObject, channel, physical) != CO_401_IO_OK) {
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
            if (failSafeActive) {
                device->failSafeOutputApplyComplete = false;
            }
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
        }
    }
}
