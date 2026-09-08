/**
 * @file CO_401_device_RTT_demo.c
 * @brief Software-only CiA 401 IOIF backend for the mixed CiA 402/CiA 401 demo OD.
 */
#include <string.h>

#include "CO_401_device_RTT.h"
#include "CO_401_device_RTT_demo.h"

/* The demo OD reserves slots 0..2 for CiA 402 axes and places generic I/O in logical-device slot 3. */
#define CO_401_DEMO_LOGICAL_DEVICE 3U

#define CO_401_DEMO_DIGITAL_BANKS CO_401_DEVICE_RTT_DEMO_DIGITAL_BANKS
#define CO_401_DEMO_ANALOG_CHANNELS CO_401_DEVICE_RTT_DEMO_ANALOG_CHANNELS

typedef struct {
    uint8_t digitalInput[CO_401_DEMO_DIGITAL_BANKS];
    uint8_t digitalOutput[CO_401_DEMO_DIGITAL_BANKS];
    int16_t analogInput[CO_401_DEMO_ANALOG_CHANNELS];
    int16_t analogOutput[CO_401_DEMO_ANALOG_CHANNELS];
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    uint8_t digitalInputFilter[CO_401_DEMO_DIGITAL_BANKS];
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS */
#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_MSH)
    rt_atomic_t outputSupervisionEstablished;
    rt_atomic_t outputSupervisionFault;
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_MSH */
} CO_401_demo_io_t;

#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_MSH)
CO_401_demo_io_t demoIo = {
    .outputSupervisionEstablished = 1,
};
#else
CO_401_demo_io_t demoIo;
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_MSH */

static CO_401_io_result_t readDigital8(void *object, uint8_t bank, uint8_t *value)
{
    CO_401_demo_io_t *io = object;

    if (io == NULL || value == NULL || bank >= CO_401_DEMO_DIGITAL_BANKS) {
        return CO_401_IO_ERROR;
    }
    *value = io->digitalInput[bank];
    return CO_401_IO_OK;
}

static CO_401_io_result_t writeDigital8(void *object, uint8_t bank, uint8_t value)
{
    CO_401_demo_io_t *io = object;

    if (io == NULL || bank >= CO_401_DEMO_DIGITAL_BANKS) {
        return CO_401_IO_ERROR;
    }
    io->digitalOutput[bank] = value;
    return CO_401_IO_OK;
}

static CO_401_io_result_t readAnalog16(void *object, uint8_t channel, int16_t *value)
{
    CO_401_demo_io_t *io = object;

    if (io == NULL || value == NULL || channel >= CO_401_DEMO_ANALOG_CHANNELS) {
        return CO_401_IO_ERROR;
    }
    *value = io->analogInput[channel];
    return CO_401_IO_OK;
}

static CO_401_io_result_t writeAnalog16(void *object, uint8_t channel, int16_t value)
{
    CO_401_demo_io_t *io = object;

    if (io == NULL || channel >= CO_401_DEMO_ANALOG_CHANNELS) {
        return CO_401_IO_ERROR;
    }
    io->analogOutput[channel] = value;
    return CO_401_IO_OK;
}

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
static CO_401_io_result_t setDigitalInputFilter8(void *object, uint8_t bank, uint8_t mask)
{
    CO_401_demo_io_t *io = object;

    if (io == NULL || bank >= CO_401_DEMO_DIGITAL_BANKS) {
        return CO_401_IO_ERROR;
    }
    io->digitalInputFilter[bank] = mask;
    return CO_401_IO_OK;
}
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS */

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
static CO_401_io_result_t writeDigital8Masked(void *object, uint8_t bank, uint8_t value, uint8_t mask)
{
    CO_401_demo_io_t *io = object;

    if (io == NULL || bank >= CO_401_DEMO_DIGITAL_BANKS) {
        return CO_401_IO_ERROR;
    }
    io->digitalOutput[bank] = (uint8_t)((io->digitalOutput[bank] & (uint8_t)(~mask)) | (value & mask));
    return CO_401_IO_OK;
}
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE */

static const CO_401_io_if_t demoIoIf = {
    .readDigital8 = readDigital8,
    .writeDigital8 = writeDigital8,
    .readAnalog16 = readAnalog16,
    .writeAnalog16 = writeAnalog16,
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    .setDigitalInputFilter8 = setDigitalInputFilter8,
#endif
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    .writeDigital8Masked = writeDigital8Masked,
#endif
};

static bool demoOutputSupervisionEstablished(void *object, const CO_t *co)
{
    (void)co;
#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_MSH)
    CO_401_demo_io_t *io = object;

    return io != NULL && rt_atomic_load(&io->outputSupervisionEstablished) != 0;
#else
    (void)object;
    /* Without the bench frontend, preserve the original software-demo supervision model. */
    return true;
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_MSH */
}

static bool demoOutputSupervisionFaultActive(void *object, const CO_t *co)
{
    (void)co;
#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_MSH)
    CO_401_demo_io_t *io = object;

    return io != NULL && rt_atomic_load(&io->outputSupervisionFault) != 0;
#else
    (void)object;
    /* No remote output-setting peer exists in the plain software demo. */
    return false;
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_MSH */
}

#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_MSH)
bool CO_401_device_RTT_demoSetDigitalInput(uint8_t bank, uint8_t value)
{
    if (bank >= CO_401_DEMO_DIGITAL_BANKS) {
        return false;
    }
    demoIo.digitalInput[bank] = value;
    return true;
}

bool CO_401_device_RTT_demoSetAnalogInput(uint8_t channel, int16_t value)
{
    if (channel >= CO_401_DEMO_ANALOG_CHANNELS) {
        return false;
    }
    demoIo.analogInput[channel] = value;
    return true;
}

void CO_401_device_RTT_demoSetOutputSupervisionEstablished(bool established)
{
    rt_atomic_store(&demoIo.outputSupervisionEstablished, established ? 1 : 0);
}

void CO_401_device_RTT_demoSetOutputSupervisionFault(bool active)
{
    rt_atomic_store(&demoIo.outputSupervisionFault, active ? 1 : 0);
}

void CO_401_device_RTT_demoGetSnapshot(CO_401_device_RTT_demo_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    (void)memcpy(snapshot->digitalInput, demoIo.digitalInput, sizeof(snapshot->digitalInput));
    (void)memcpy(snapshot->digitalOutput, demoIo.digitalOutput, sizeof(snapshot->digitalOutput));
    (void)memcpy(snapshot->analogInput, demoIo.analogInput, sizeof(snapshot->analogInput));
    (void)memcpy(snapshot->analogOutput, demoIo.analogOutput, sizeof(snapshot->analogOutput));
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    (void)memcpy(snapshot->digitalInputFilter, demoIo.digitalInputFilter, sizeof(snapshot->digitalInputFilter));
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS */
    snapshot->outputSupervisionEstablished = rt_atomic_load(&demoIo.outputSupervisionEstablished) != 0;
    snapshot->outputSupervisionFault = rt_atomic_load(&demoIo.outputSupervisionFault) != 0;
}
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_MSH */

static const CO_401_device_RTT_config_t demoConfig = {
    .device = {
        .io = &demoIoIf,
        .ioObject = &demoIo,
        .digitalInputBanks = CO_401_DEMO_DIGITAL_BANKS,
        .digitalOutputBanks = CO_401_DEMO_DIGITAL_BANKS,
        .analogInputChannels = CO_401_DEMO_ANALOG_CHANNELS,
        .analogOutputChannels = CO_401_DEMO_ANALOG_CHANNELS,
        .logicalDevice = CO_401_DEMO_LOGICAL_DEVICE,
    },
    .outputSupervisionEstablished = demoOutputSupervisionEstablished,
#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_MSH)
    .outputSupervisionObject = &demoIo,
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_MSH */
    .outputSupervisionFaultActive = demoOutputSupervisionFaultActive,
};

#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_AUTOSTART)
CO_401_DEVICE_RTT_AUTOSTART_DEFINE(cia401_demo, &demoConfig);
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_AUTOSTART */
