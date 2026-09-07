/**
 * @file CO_401_device_RTT_demo.c
 * @brief Software-only CiA 401 IOIF backend for RT-Thread lifecycle testing.
 */
#include <string.h>

#include "CO_401_device_RTT.h"

typedef struct {
    uint8_t digitalInput;
    uint8_t digitalOutput;
    int16_t analogInput;
    int16_t analogOutput;
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    uint8_t digitalInputFilter;
#endif
} CO_401_demo_io_t;

static CO_401_demo_io_t demoIo;

static CO_401_io_result_t readDigital8(void *object, uint8_t bank, uint8_t *value)
{
    CO_401_demo_io_t *io = object;
    if (io == NULL || value == NULL || bank != 0U) return CO_401_IO_ERROR;
    *value = io->digitalInput; return CO_401_IO_OK;
}

static CO_401_io_result_t writeDigital8(void *object, uint8_t bank, uint8_t value)
{
    CO_401_demo_io_t *io = object;
    if (io == NULL || bank != 0U) return CO_401_IO_ERROR;
    io->digitalOutput = value; return CO_401_IO_OK;
}

static CO_401_io_result_t readAnalog16(void *object, uint8_t channel, int16_t *value)
{
    CO_401_demo_io_t *io = object;
    if (io == NULL || value == NULL || channel != 0U) return CO_401_IO_ERROR;
    *value = io->analogInput; return CO_401_IO_OK;
}

static CO_401_io_result_t writeAnalog16(void *object, uint8_t channel, int16_t value)
{
    CO_401_demo_io_t *io = object;
    if (io == NULL || channel != 0U) return CO_401_IO_ERROR;
    io->analogOutput = value; return CO_401_IO_OK;
}

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
static CO_401_io_result_t setDigitalInputFilter8(void *object, uint8_t bank, uint8_t mask)
{
    CO_401_demo_io_t *io = object;
    if (io == NULL || bank != 0U) return CO_401_IO_ERROR;
    io->digitalInputFilter = mask; return CO_401_IO_OK;
}
#endif

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
static CO_401_io_result_t writeDigital8Masked(void *object, uint8_t bank, uint8_t value, uint8_t mask)
{
    CO_401_demo_io_t *io = object;
    if (io == NULL || bank != 0U) return CO_401_IO_ERROR;
    io->digitalOutput = (uint8_t)((io->digitalOutput & (uint8_t)(~mask)) | (value & mask));
    return CO_401_IO_OK;
}
#endif

static const CO_401_io_if_t demoIoIf = {
    .readDigital8 = readDigital8, .writeDigital8 = writeDigital8,
    .readAnalog16 = readAnalog16, .writeAnalog16 = writeAnalog16,
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    .setDigitalInputFilter8 = setDigitalInputFilter8,
#endif
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    .writeDigital8Masked = writeDigital8Masked,
#endif
};

static bool demoOutputSupervisionEstablished(void *object, const CO_t *co)
{
    (void)object;
    (void)co;
    /* This software-only demo has no remote output-setting peer, so model the external supervision prerequisite. */
    return true;
}

static bool demoOutputSupervisionFaultActive(void *object, const CO_t *co)
{
    (void)object;
    (void)co;
    /* No remote output-setting peer exists in this demo, so no peer-specific Heartbeat fault can become active. */
    return false;
}

static const CO_401_device_RTT_config_t demoConfig = {
    .device = {
        .io = &demoIoIf, .ioObject = &demoIo,
        .digitalInputBanks = 1U, .digitalOutputBanks = 1U,
        .analogInputChannels = 1U, .analogOutputChannels = 1U,
    },
    .outputSupervisionEstablished = demoOutputSupervisionEstablished,
    .outputSupervisionFaultActive = demoOutputSupervisionFaultActive,
};

#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_AUTOSTART)
CO_401_DEVICE_RTT_AUTOSTART_DEFINE(cia401_demo, &demoConfig);
#endif
