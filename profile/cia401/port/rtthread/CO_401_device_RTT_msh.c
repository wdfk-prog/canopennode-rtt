/**
 * @file CO_401_device_RTT_msh.c
 * @brief RT-Thread MSH bench frontend for the software-only CiA 401 Device demo.
 */

#include "CO_app_RTT.h"
#include "CO_401_device_RTT.h"

#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_MSH)

#include <finsh.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "CO_401_device_RTT_demo.h"

typedef struct {
    CANopenNodeRTT *app;
    CO_401_device_RTT_t *runtime;
} CO_401_device_RTT_msh_t;

typedef struct {
    uint8_t logicalDevice;
    uint8_t digitalInputBanks;
    uint8_t digitalOutputBanks;
    uint8_t analogInputChannels;
    uint8_t analogOutputChannels;
    rt_bool_t attached;
    rt_bool_t deviceInitialized;
    rt_bool_t communicationReady;
    bool odBound;
    bool outputSupervisionReady;
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    bool digitalOutputFaultActive;
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE */
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    bool analogOutputFaultActive;
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    bool communicationFaultActive;
    bool nmtStopped;
    bool failSafeOutputApplyComplete;
    CO_NMT_internalState_t nmtState;
#endif /* output fail-safe */
#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE)
    rt_bool_t analogWarningReported;
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE */
    CO_401_device_RTT_demo_snapshot_t demo;
    uint8_t digitalInputOd[CO_401_DEVICE_RTT_DEMO_DIGITAL_BANKS];
    uint8_t digitalOutputOd[CO_401_DEVICE_RTT_DEMO_DIGITAL_BANKS];
    int16_t analogInputOd[CO_401_DEVICE_RTT_DEMO_ANALOG_CHANNELS];
    int16_t analogOutputOd[CO_401_DEVICE_RTT_DEMO_ANALOG_CHANNELS];
} CO_401_device_RTT_msh_status_t;

static CO_401_device_RTT_msh_t CO_401_msh;
static struct rt_mutex CO_401_mshBindingMutex;
static rt_bool_t CO_401_mshBindingMutexInitialized;

/** Initialize the process-lifetime lock which serializes the singleton MSH binding. */
static int CO_401_mshBindingInit(void)
{
    rt_err_t ret = rt_mutex_init(&CO_401_mshBindingMutex, "401_bnd", RT_IPC_FLAG_PRIO);

    if (ret == RT_EOK) {
        CO_401_mshBindingMutexInitialized = RT_TRUE;
    }
    return (int)ret;
}
INIT_COMPONENT_EXPORT(CO_401_mshBindingInit);

static rt_err_t CO_401_mshBindingTake(void)
{
    if (CO_401_mshBindingMutexInitialized != RT_TRUE) {
        return -RT_ERROR;
    }
    return rt_mutex_take(&CO_401_mshBindingMutex, RT_WAITING_FOREVER);
}

static bool CO_401_mshParseMagnitude(const char *text, bool allowSign, bool *negative, uint64_t *value)
{
    const char *cursor = text;
    uint64_t parsed = 0U;
    unsigned int base = 10U;
    bool hasDigit = false;

    if (text == NULL || negative == NULL || value == NULL || text[0] == '\0') {
        return false;
    }

    *negative = false;
    if (*cursor == '+' || *cursor == '-') {
        if (!allowSign) {
            return false;
        }
        *negative = *cursor == '-';
        cursor++;
    }
    if (cursor[0] == '0' && (cursor[1] == 'x' || cursor[1] == 'X')) {
        base = 16U;
        cursor += 2;
    }

    while (*cursor != '\0') {
        unsigned int digit;

        if (*cursor >= '0' && *cursor <= '9') {
            digit = (unsigned int)(*cursor - '0');
        } else if (base == 16U && *cursor >= 'a' && *cursor <= 'f') {
            digit = (unsigned int)(*cursor - 'a') + 10U;
        } else if (base == 16U && *cursor >= 'A' && *cursor <= 'F') {
            digit = (unsigned int)(*cursor - 'A') + 10U;
        } else {
            return false;
        }
        if (digit >= base || parsed > (UINT64_MAX - digit) / base) {
            return false;
        }

        parsed = parsed * base + digit;
        hasDigit = true;
        cursor++;
    }

    if (!hasDigit) {
        return false;
    }

    *value = parsed;
    return true;
}

static bool CO_401_mshParseLong(const char *text, long minimum, long maximum, long *value)
{
    uint64_t magnitude;
    bool negative;
    int64_t parsed;

    if (value == NULL || !CO_401_mshParseMagnitude(text, true, &negative, &magnitude)
        || magnitude > (uint64_t)INT32_MAX + 1U) {
        return false;
    }

    parsed = negative ? -(int64_t)magnitude : (int64_t)magnitude;
    if (parsed < (int64_t)minimum || parsed > (int64_t)maximum) {
        return false;
    }

    *value = (long)parsed;
    return true;
}

static bool CO_401_mshParseUnsignedLong(const char *text, unsigned long maximum, unsigned long *value)
{
    uint64_t parsed;
    bool negative;

    if (value == NULL || !CO_401_mshParseMagnitude(text, false, &negative, &parsed)
        || negative || parsed > (uint64_t)maximum) {
        return false;
    }

    *value = (unsigned long)parsed;
    return true;
}

static bool CO_401_mshParseBool(const char *text, bool *value)
{
    unsigned long parsed;

    if (value == NULL || !CO_401_mshParseUnsignedLong(text, 1U, &parsed)) {
        return false;
    }
    *value = parsed != 0U;
    return true;
}

static void CO_401_mshPrintUsage(void)
{
    rt_kprintf("CiA 401 software-demo debug commands:\n");
    rt_kprintf("  cia401 status\n");
    rt_kprintf("  cia401 di <bank> <0..255>           # inject raw digital input\n");
    rt_kprintf("  cia401 ai <channel> <-32768..32767> # inject raw analogue input\n");
    rt_kprintf("  cia401 do <bank> <0..255>           # write 0x6200 command image\n");
    rt_kprintf("  cia401 ao <channel> <-32768..32767> # write 0x6411 command image\n");
    rt_kprintf("  cia401 devent <bank> <en> <pol> <filter> <any> <rise> <fall>\n");
    rt_kprintf("  cia401 aevent <ch> <en> <trigger> <upper> <lower> <delta> <neg> <pos>\n");
    rt_kprintf("  cia401 dfailsafe <bank> <pol> <mode> <value> <filter>\n");
    rt_kprintf("  cia401 afailsafe <ch> <mode:0|1> <value>\n");
    rt_kprintf("  cia401 condition <ai|ao> <ch> <scale> <offset>\n");
    rt_kprintf("  cia401 fault <digital|analog> <0|1> # product/internal output fault\n");
    rt_kprintf("  cia401 supervision ready <0|1>      # demo supervision source\n");
    rt_kprintf("  cia401 supervision fault <0|1>      # demo peer communication fault\n");
    rt_kprintf("  cia401 supervision reset            # test-only clear of source and latched ready state\n");
    rt_kprintf("  cia401 process                      # wake one bounded profile pass\n");
}

/*
 * Follow the same singleton publication and lifecycleMutex -> OD lock order as
 * the CiA 402 MSH frontend. Revalidation after taking lifecycleMutex prevents a
 * command from retaining a runtime pointer across final teardown.
 */
static rt_err_t CO_401_mshLock(CANopenNodeRTT **appOut, CO_401_device_RTT_t **runtimeOut, CO_t **coOut)
{
    CANopenNodeRTT *app;
    CO_401_device_RTT_t *runtime;
    CO_t *co;
    rt_err_t ret;

    ret = CO_401_mshBindingTake();
    if (ret != RT_EOK) {
        return ret;
    }
    app = CO_401_msh.app;
    (void)rt_mutex_release(&CO_401_mshBindingMutex);

    if (app == NULL) {
        return -RT_ERROR;
    }

    ret = rt_mutex_take(&app->lifecycleMutex, RT_WAITING_FOREVER);
    if (ret != RT_EOK) {
        return ret;
    }

    ret = CO_401_mshBindingTake();
    if (ret != RT_EOK) {
        (void)rt_mutex_release(&app->lifecycleMutex);
        return ret;
    }
    runtime = (CO_401_msh.app == app) ? CO_401_msh.runtime : NULL;
    (void)rt_mutex_release(&CO_401_mshBindingMutex);

    if (runtime == NULL || runtime->deviceInitialized != RT_TRUE
        || runtime->communicationReady != RT_TRUE || !runtime->device.odBound) {
        (void)rt_mutex_release(&app->lifecycleMutex);
        return -RT_ERROR;
    }

    co = app->canOpenStack;
    if (co == NULL || co->CANmodule == NULL) {
        (void)rt_mutex_release(&app->lifecycleMutex);
        return -RT_ERROR;
    }

    /*
     * RT-Thread CO_LOCK_OD/CO_UNLOCK_OD form one lexical macro scope.
     * MSH keeps this lock across helper calls, so use the same mutex
     * directly while lifecycleMutex pins the current CANmodule.
     */
    (void)rt_mutex_take(&co->CANmodule->odMutex, RT_WAITING_FOREVER);
    *appOut = app;
    *runtimeOut = runtime;
    *coOut = co;
    return RT_EOK;
}

static void CO_401_mshUnlock(CANopenNodeRTT *app, CO_t *co)
{
    (void)rt_mutex_release(&co->CANmodule->odMutex);
    (void)rt_mutex_release(&app->lifecycleMutex);
}

/* Publish the latest bench state and coalesce the wake exactly like the timer path. */
static void CO_401_mshPublishAndUnlock(CANopenNodeRTT *app, CO_401_device_RTT_t *runtime, CO_t *co)
{
    (void)rt_mutex_release(&co->CANmodule->odMutex);
    (void)CO_401_device_RTT_requestProcess(runtime);
    (void)rt_mutex_release(&app->lifecycleMutex);
}

static rt_err_t CO_401_mshReadStatus(CO_401_device_RTT_t *runtime, CO_401_device_RTT_msh_status_t *status)
{
    uint8_t i;

    (void)memset(status, 0, sizeof(*status));
    status->logicalDevice = runtime->device.logicalDevice;
    status->digitalInputBanks = runtime->config.device.digitalInputBanks;
    status->digitalOutputBanks = runtime->config.device.digitalOutputBanks;
    status->analogInputChannels = runtime->config.device.analogInputChannels;
    status->analogOutputChannels = runtime->config.device.analogOutputChannels;
    status->attached = runtime->attached;
    status->deviceInitialized = runtime->deviceInitialized;
    status->communicationReady = runtime->communicationReady;
    status->odBound = runtime->device.odBound;
    status->outputSupervisionReady = runtime->device.outputSupervisionReady;
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    status->digitalOutputFaultActive = runtime->device.digitalOutputFaultActive;
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE */
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    status->analogOutputFaultActive = runtime->device.analogOutputFaultActive;
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    status->communicationFaultActive = runtime->device.communicationFaultActive;
    status->nmtStopped = runtime->device.nmtStopped;
    status->failSafeOutputApplyComplete = runtime->device.failSafeOutputApplyComplete;
    status->nmtState = runtime->nmtState;
#endif /* output fail-safe */
#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE)
    status->analogWarningReported = runtime->analogWarningReported;
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE */

    CO_401_device_RTT_demoGetSnapshot(&status->demo);
    for (i = 0U; i < CO_401_DEVICE_RTT_DEMO_DIGITAL_BANKS; i++) {
        if (OD_get_u8(runtime->device.bound.digitalInput8, (uint8_t)(i + 1U), &status->digitalInputOd[i], true)
                != ODR_OK
            || OD_get_u8(runtime->device.bound.digitalOutput8, (uint8_t)(i + 1U), &status->digitalOutputOd[i], true)
                != ODR_OK) {
            return -RT_ERROR;
        }
    }
    for (i = 0U; i < CO_401_DEVICE_RTT_DEMO_ANALOG_CHANNELS; i++) {
        if (OD_get_i16(runtime->device.bound.analogInput16, (uint8_t)(i + 1U), &status->analogInputOd[i], true)
                != ODR_OK
            || OD_get_i16(runtime->device.bound.analogOutput16, (uint8_t)(i + 1U), &status->analogOutputOd[i], true)
                != ODR_OK) {
            return -RT_ERROR;
        }
    }
    return RT_EOK;
}

static int CO_401_mshStatus(int argc, char **argv)
{
    CANopenNodeRTT *app;
    CO_401_device_RTT_t *runtime;
    CO_401_device_RTT_msh_status_t status;
    CO_t *co;
    rt_err_t ret;
    uint8_t i;

    (void)argv;
    if (argc != 2) {
        rt_kprintf("usage: cia401 status\n");
        return -RT_EINVAL;
    }

    ret = CO_401_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        rt_kprintf("cia401: runtime is not ready\n");
        return ret;
    }
    ret = CO_401_mshReadStatus(runtime, &status);
    CO_401_mshUnlock(app, co);
    if (ret != RT_EOK) {
        rt_kprintf("cia401: failed to snapshot Object Dictionary\n");
        return ret;
    }

    rt_kprintf("cia401: logical=%u attached=%u initialized=%u ready=%u od=%u\n",
               status.logicalDevice, status.attached, status.deviceInitialized,
               status.communicationReady, status.odBound ? 1U : 0U);
    rt_kprintf("  supervision: source=%u latched=%u peer-fault=%u\n",
               status.demo.outputSupervisionEstablished ? 1U : 0U,
               status.outputSupervisionReady ? 1U : 0U,
               status.demo.outputSupervisionFault ? 1U : 0U);
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    rt_kprintf("  nmt=%d stopped=%u comm-fault=%u failsafe-complete=%u\n",
               (int)status.nmtState, status.nmtStopped ? 1U : 0U,
               status.communicationFaultActive ? 1U : 0U,
               status.failSafeOutputApplyComplete ? 1U : 0U);
#endif /* output fail-safe */
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    rt_kprintf("  digital-product-fault=%u\n", status.digitalOutputFaultActive ? 1U : 0U);
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE */
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    rt_kprintf("  analog-product-fault=%u\n", status.analogOutputFaultActive ? 1U : 0U);
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE)
    rt_kprintf("  analog-warning-reported=%u\n", status.analogWarningReported);
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE */

    for (i = 0U; i < CO_401_DEVICE_RTT_DEMO_DIGITAL_BANKS; i++) {
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
        rt_kprintf("  DI[%u]: raw=0x%02X od=0x%02X filter=0x%02X\n",
                   i, status.demo.digitalInput[i], status.digitalInputOd[i], status.demo.digitalInputFilter[i]);
#else
        rt_kprintf("  DI[%u]: raw=0x%02X od=0x%02X\n", i, status.demo.digitalInput[i], status.digitalInputOd[i]);
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS */
        rt_kprintf("  DO[%u]: command=0x%02X physical=0x%02X\n",
                   i, status.digitalOutputOd[i], status.demo.digitalOutput[i]);
    }
    for (i = 0U; i < CO_401_DEVICE_RTT_DEMO_ANALOG_CHANNELS; i++) {
        rt_kprintf("  AI[%u]: raw=%d od=%d\n", i, status.demo.analogInput[i], status.analogInputOd[i]);
        rt_kprintf("  AO[%u]: command=%d physical=%d\n", i, status.analogOutputOd[i], status.demo.analogOutput[i]);
    }
    return RT_EOK;
}

static int CO_401_mshDigitalInput(int argc, char **argv)
{
    CANopenNodeRTT *app;
    CO_401_device_RTT_t *runtime;
    CO_t *co;
    unsigned long bank;
    unsigned long value;
    rt_err_t ret;

    if (argc != 4 || !CO_401_mshParseUnsignedLong(argv[2], UINT8_MAX, &bank)
        || !CO_401_mshParseUnsignedLong(argv[3], UINT8_MAX, &value)
        || bank >= CO_401_DEVICE_RTT_DEMO_DIGITAL_BANKS) {
        rt_kprintf("usage: cia401 di <bank:0..%u> <0..255>\n",
                   CO_401_DEVICE_RTT_DEMO_DIGITAL_BANKS - 1U);
        return -RT_EINVAL;
    }

    ret = CO_401_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        rt_kprintf("cia401: runtime is not ready\n");
        return ret;
    }
    if (!CO_401_device_RTT_demoSetDigitalInput((uint8_t)bank, (uint8_t)value)) {
        CO_401_mshUnlock(app, co);
        return -RT_EINVAL;
    }
    CO_401_mshPublishAndUnlock(app, runtime, co);
    rt_kprintf("cia401: DI[%lu] raw=0x%02lX queued\n", bank, value);
    return RT_EOK;
}

static int CO_401_mshAnalogInput(int argc, char **argv)
{
    CANopenNodeRTT *app;
    CO_401_device_RTT_t *runtime;
    CO_t *co;
    unsigned long channel;
    long value;
    rt_err_t ret;

    if (argc != 4 || !CO_401_mshParseUnsignedLong(argv[2], UINT8_MAX, &channel)
        || !CO_401_mshParseLong(argv[3], INT16_MIN, INT16_MAX, &value)
        || channel >= CO_401_DEVICE_RTT_DEMO_ANALOG_CHANNELS) {
        rt_kprintf("usage: cia401 ai <channel:0..%u> <-32768..32767>\n",
                   CO_401_DEVICE_RTT_DEMO_ANALOG_CHANNELS - 1U);
        return -RT_EINVAL;
    }

    ret = CO_401_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        rt_kprintf("cia401: runtime is not ready\n");
        return ret;
    }
    if (!CO_401_device_RTT_demoSetAnalogInput((uint8_t)channel, (int16_t)value)) {
        CO_401_mshUnlock(app, co);
        return -RT_EINVAL;
    }
    CO_401_mshPublishAndUnlock(app, runtime, co);
    rt_kprintf("cia401: AI[%lu] raw=%ld queued\n", channel, value);
    return RT_EOK;
}

static int CO_401_mshDigitalOutput(int argc, char **argv)
{
    CANopenNodeRTT *app;
    CO_401_device_RTT_t *runtime;
    CO_t *co;
    unsigned long bank;
    unsigned long value;
    rt_err_t ret;
    ODR_t odRet;

    if (argc != 4 || !CO_401_mshParseUnsignedLong(argv[2], UINT8_MAX, &bank)
        || !CO_401_mshParseUnsignedLong(argv[3], UINT8_MAX, &value)
        || bank >= CO_401_DEVICE_RTT_DEMO_DIGITAL_BANKS) {
        rt_kprintf("usage: cia401 do <bank:0..%u> <0..255>\n",
                   CO_401_DEVICE_RTT_DEMO_DIGITAL_BANKS - 1U);
        return -RT_EINVAL;
    }

    ret = CO_401_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        rt_kprintf("cia401: runtime is not ready\n");
        return ret;
    }
    odRet = OD_set_u8(runtime->device.bound.digitalOutput8, (uint8_t)(bank + 1U), (uint8_t)value, false);
    if (odRet != ODR_OK) {
        CO_401_mshUnlock(app, co);
        rt_kprintf("cia401: 0x6200 write failed (%d)\n", (int)odRet);
        return -RT_ERROR;
    }
    CO_401_mshPublishAndUnlock(app, runtime, co);
    rt_kprintf("cia401: DO[%lu] command=0x%02lX queued\n", bank, value);
    return RT_EOK;
}

static int CO_401_mshAnalogOutput(int argc, char **argv)
{
    CANopenNodeRTT *app;
    CO_401_device_RTT_t *runtime;
    CO_t *co;
    unsigned long channel;
    long value;
    rt_err_t ret;
    ODR_t odRet;

    if (argc != 4 || !CO_401_mshParseUnsignedLong(argv[2], UINT8_MAX, &channel)
        || !CO_401_mshParseLong(argv[3], INT16_MIN, INT16_MAX, &value)
        || channel >= CO_401_DEVICE_RTT_DEMO_ANALOG_CHANNELS) {
        rt_kprintf("usage: cia401 ao <channel:0..%u> <-32768..32767>\n",
                   CO_401_DEVICE_RTT_DEMO_ANALOG_CHANNELS - 1U);
        return -RT_EINVAL;
    }

    ret = CO_401_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        rt_kprintf("cia401: runtime is not ready\n");
        return ret;
    }
    odRet = OD_set_i16(runtime->device.bound.analogOutput16, (uint8_t)(channel + 1U), (int16_t)value, false);
    if (odRet != ODR_OK) {
        CO_401_mshUnlock(app, co);
        rt_kprintf("cia401: 0x6411 write failed (%d)\n", (int)odRet);
        return -RT_ERROR;
    }
    CO_401_mshPublishAndUnlock(app, runtime, co);
    rt_kprintf("cia401: AO[%lu] command=%ld queued\n", channel, value);
    return RT_EOK;
}

static int CO_401_mshDigitalEvent(int argc, char **argv)
{
    unsigned long bank;
    unsigned long polarity;
    unsigned long filter;
    unsigned long anyMask;
    unsigned long risingMask;
    unsigned long fallingMask;
    bool enabled;

    if (argc != 9 || !CO_401_mshParseUnsignedLong(argv[2], UINT8_MAX, &bank)
        || !CO_401_mshParseBool(argv[3], &enabled)
        || !CO_401_mshParseUnsignedLong(argv[4], UINT8_MAX, &polarity)
        || !CO_401_mshParseUnsignedLong(argv[5], UINT8_MAX, &filter)
        || !CO_401_mshParseUnsignedLong(argv[6], UINT8_MAX, &anyMask)
        || !CO_401_mshParseUnsignedLong(argv[7], UINT8_MAX, &risingMask)
        || !CO_401_mshParseUnsignedLong(argv[8], UINT8_MAX, &fallingMask)
        || bank >= CO_401_DEVICE_RTT_DEMO_DIGITAL_BANKS) {
        rt_kprintf("usage: cia401 devent <bank> <en:0|1> <pol> <filter> <any> <rise> <fall>\n");
        return -RT_EINVAL;
    }

#if !defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    rt_kprintf("cia401: digital input events are not enabled\n");
    return -RT_ENOSYS;
#else
    CANopenNodeRTT *app;
    CO_401_device_RTT_t *runtime;
    CO_t *co;
    rt_err_t ret;
    ODR_t odRet;
    uint8_t subIndex;

    ret = CO_401_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        rt_kprintf("cia401: runtime is not ready\n");
        return ret;
    }
    subIndex = (uint8_t)(bank + 1U);
    odRet = OD_set_u8(runtime->device.bound.digitalInterruptEnable, 0U, enabled ? 1U : 0U, false);
    if (odRet == ODR_OK) {
        odRet = OD_set_u8(runtime->device.bound.digitalInputPolarity8, subIndex, (uint8_t)polarity, false);
    }
    if (odRet == ODR_OK) {
        odRet = OD_set_u8(runtime->device.bound.digitalInputFilter8, subIndex, (uint8_t)filter, false);
    }
    if (odRet == ODR_OK) {
        odRet = OD_set_u8(runtime->device.bound.digitalInterruptAny8, subIndex, (uint8_t)anyMask, false);
    }
    if (odRet == ODR_OK) {
        odRet = OD_set_u8(runtime->device.bound.digitalInterruptRising8, subIndex, (uint8_t)risingMask, false);
    }
    if (odRet == ODR_OK) {
        odRet = OD_set_u8(runtime->device.bound.digitalInterruptFalling8, subIndex, (uint8_t)fallingMask, false);
    }
    if (odRet != ODR_OK) {
        CO_401_mshUnlock(app, co);
        rt_kprintf("cia401: digital event configuration failed (%d)\n", (int)odRet);
        return -RT_ERROR;
    }
    CO_401_mshPublishAndUnlock(app, runtime, co);
    rt_kprintf("cia401: digital event bank=%lu enable=%u queued\n", bank, enabled ? 1U : 0U);
    return RT_EOK;
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS */
}

static bool CO_401_mshProcessUnitsToI32(long value, int32_t *encoded)
{
    int64_t widened;

    if (encoded == NULL || value < INT16_MIN || value > INT16_MAX) {
        return false;
    }
    widened = (int64_t)value * 65536LL;
    *encoded = (int32_t)widened;
    return true;
}

static bool CO_401_mshProcessUnitsToU32(unsigned long value, uint32_t *encoded)
{
    uint64_t widened;

    if (encoded == NULL || value > UINT16_MAX) {
        return false;
    }
    widened = (uint64_t)value * 65536ULL;
    *encoded = (uint32_t)widened;
    return true;
}

static int CO_401_mshAnalogEvent(int argc, char **argv)
{
    unsigned long channel;
    unsigned long trigger;
    unsigned long delta;
    unsigned long negativeDelta;
    unsigned long positiveDelta;
    long upper;
    long lower;
    int32_t upperEncoded;
    int32_t lowerEncoded;
    uint32_t deltaEncoded;
    uint32_t negativeEncoded;
    uint32_t positiveEncoded;
    bool enabled;

    if (argc != 10 || !CO_401_mshParseUnsignedLong(argv[2], UINT8_MAX, &channel)
        || !CO_401_mshParseBool(argv[3], &enabled)
        || !CO_401_mshParseUnsignedLong(argv[4], 0x1FU, &trigger)
        || !CO_401_mshParseLong(argv[5], INT16_MIN, INT16_MAX, &upper)
        || !CO_401_mshParseLong(argv[6], INT16_MIN, INT16_MAX, &lower)
        || !CO_401_mshParseUnsignedLong(argv[7], UINT16_MAX, &delta)
        || !CO_401_mshParseUnsignedLong(argv[8], UINT16_MAX, &negativeDelta)
        || !CO_401_mshParseUnsignedLong(argv[9], UINT16_MAX, &positiveDelta)
        || channel >= CO_401_DEVICE_RTT_DEMO_ANALOG_CHANNELS
        || !CO_401_mshProcessUnitsToI32(upper, &upperEncoded)
        || !CO_401_mshProcessUnitsToI32(lower, &lowerEncoded)
        || !CO_401_mshProcessUnitsToU32(delta, &deltaEncoded)
        || !CO_401_mshProcessUnitsToU32(negativeDelta, &negativeEncoded)
        || !CO_401_mshProcessUnitsToU32(positiveDelta, &positiveEncoded)) {
        rt_kprintf("usage: cia401 aevent <ch> <en:0|1> <trigger> <upper> <lower> <delta> <neg> <pos>\n");
        rt_kprintf("       thresholds/deltas use INTEGER16 process units; trigger uses 0x6421 bits 0..4\n");
        return -RT_EINVAL;
    }

#if !defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
    rt_kprintf("cia401: analogue input events are not enabled\n");
    return -RT_ENOSYS;
#else
    CANopenNodeRTT *app;
    CO_401_device_RTT_t *runtime;
    CO_t *co;
    rt_err_t ret;
    ODR_t odRet;
    uint8_t subIndex;

    ret = CO_401_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        rt_kprintf("cia401: runtime is not ready\n");
        return ret;
    }
    subIndex = (uint8_t)(channel + 1U);
    odRet = OD_set_u8(runtime->device.bound.analogInterruptEnable, 0U, enabled ? 1U : 0U, false);
    if (odRet == ODR_OK) {
        odRet = OD_set_u8(runtime->device.bound.analogInterruptTrigger, subIndex, (uint8_t)trigger, false);
    }
    if (odRet == ODR_OK) {
        odRet = OD_set_i32(runtime->device.bound.analogInterruptUpper32, subIndex, upperEncoded, false);
    }
    if (odRet == ODR_OK) {
        odRet = OD_set_i32(runtime->device.bound.analogInterruptLower32, subIndex, lowerEncoded, false);
    }
    if (odRet == ODR_OK) {
        odRet = OD_set_u32(runtime->device.bound.analogInterruptDeltaU32, subIndex, deltaEncoded, false);
    }
    if (odRet == ODR_OK) {
        odRet = OD_set_u32(runtime->device.bound.analogInterruptNegDeltaU32, subIndex, negativeEncoded, false);
    }
    if (odRet == ODR_OK) {
        odRet = OD_set_u32(runtime->device.bound.analogInterruptPosDeltaU32, subIndex, positiveEncoded, false);
    }
    if (odRet != ODR_OK) {
        CO_401_mshUnlock(app, co);
        rt_kprintf("cia401: analogue event configuration failed (%d)\n", (int)odRet);
        return -RT_ERROR;
    }
    CO_401_mshPublishAndUnlock(app, runtime, co);
    rt_kprintf("cia401: analogue event channel=%lu enable=%u trigger=0x%02lX queued\n",
               channel, enabled ? 1U : 0U, trigger);
    return RT_EOK;
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */
}

static int CO_401_mshDigitalFailsafe(int argc, char **argv)
{
    unsigned long bank;
    unsigned long polarity;
    unsigned long mode;
    unsigned long value;
    unsigned long filter;

    if (argc != 7 || !CO_401_mshParseUnsignedLong(argv[2], UINT8_MAX, &bank)
        || !CO_401_mshParseUnsignedLong(argv[3], UINT8_MAX, &polarity)
        || !CO_401_mshParseUnsignedLong(argv[4], UINT8_MAX, &mode)
        || !CO_401_mshParseUnsignedLong(argv[5], UINT8_MAX, &value)
        || !CO_401_mshParseUnsignedLong(argv[6], UINT8_MAX, &filter)
        || bank >= CO_401_DEVICE_RTT_DEMO_DIGITAL_BANKS) {
        rt_kprintf("usage: cia401 dfailsafe <bank> <polarity> <mode-mask> <error-value> <filter-mask>\n");
        return -RT_EINVAL;
    }

#if !defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    rt_kprintf("cia401: digital output fail-safe is not enabled\n");
    return -RT_ENOSYS;
#else
    CANopenNodeRTT *app;
    CO_401_device_RTT_t *runtime;
    CO_t *co;
    rt_err_t ret;
    ODR_t odRet;
    uint8_t subIndex;

    ret = CO_401_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        rt_kprintf("cia401: runtime is not ready\n");
        return ret;
    }
    subIndex = (uint8_t)(bank + 1U);
    odRet = OD_set_u8(runtime->device.bound.digitalOutputPolarity8, subIndex, (uint8_t)polarity, false);
    if (odRet == ODR_OK) {
        odRet = OD_set_u8(runtime->device.bound.digitalOutputErrorMode8, subIndex, (uint8_t)mode, false);
    }
    if (odRet == ODR_OK) {
        odRet = OD_set_u8(runtime->device.bound.digitalOutputErrorValue8, subIndex, (uint8_t)value, false);
    }
    if (odRet == ODR_OK) {
        odRet = OD_set_u8(runtime->device.bound.digitalOutputFilter8, subIndex, (uint8_t)filter, false);
    }
    if (odRet != ODR_OK) {
        CO_401_mshUnlock(app, co);
        rt_kprintf("cia401: digital fail-safe configuration failed (%d)\n", (int)odRet);
        return -RT_ERROR;
    }
    CO_401_mshPublishAndUnlock(app, runtime, co);
    rt_kprintf("cia401: digital fail-safe bank=%lu queued\n", bank);
    return RT_EOK;
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE */
}

static int CO_401_mshAnalogFailsafe(int argc, char **argv)
{
    unsigned long channel;
    unsigned long mode;
    long value;
    int32_t encodedValue;

    if (argc != 5 || !CO_401_mshParseUnsignedLong(argv[2], UINT8_MAX, &channel)
        || !CO_401_mshParseUnsignedLong(argv[3], 1U, &mode)
        || !CO_401_mshParseLong(argv[4], INT16_MIN, INT16_MAX, &value)
        || channel >= CO_401_DEVICE_RTT_DEMO_ANALOG_CHANNELS
        || !CO_401_mshProcessUnitsToI32(value, &encodedValue)) {
        rt_kprintf("usage: cia401 afailsafe <channel> <mode:0|1> <-32768..32767>\n");
        return -RT_EINVAL;
    }

#if !defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    rt_kprintf("cia401: analogue output fail-safe is not enabled\n");
    return -RT_ENOSYS;
#else
    CANopenNodeRTT *app;
    CO_401_device_RTT_t *runtime;
    CO_t *co;
    rt_err_t ret;
    ODR_t odRet;
    uint8_t subIndex;

    ret = CO_401_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        rt_kprintf("cia401: runtime is not ready\n");
        return ret;
    }
    subIndex = (uint8_t)(channel + 1U);
    odRet = OD_set_u8(runtime->device.bound.analogOutputErrorMode, subIndex, (uint8_t)mode, false);
    if (odRet == ODR_OK) {
        odRet = OD_set_i32(runtime->device.bound.analogOutputErrorValue32, subIndex, encodedValue, false);
    }
    if (odRet != ODR_OK) {
        CO_401_mshUnlock(app, co);
        rt_kprintf("cia401: analogue fail-safe configuration failed (%d)\n", (int)odRet);
        return -RT_ERROR;
    }
    CO_401_mshPublishAndUnlock(app, runtime, co);
    rt_kprintf("cia401: analogue fail-safe channel=%lu mode=%lu value=%ld queued\n", channel, mode, value);
    return RT_EOK;
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
}

static int CO_401_mshCondition(int argc, char **argv)
{
    unsigned long channel;
    long scaling;
    long offset;

    if (argc != 6 || (strcmp(argv[2], "ai") != 0 && strcmp(argv[2], "ao") != 0)
        || !CO_401_mshParseUnsignedLong(argv[3], UINT8_MAX, &channel)
        || !CO_401_mshParseLong(argv[4], INT32_MIN, INT32_MAX, &scaling)
        || !CO_401_mshParseLong(argv[5], INT32_MIN, INT32_MAX, &offset)
        || channel >= CO_401_DEVICE_RTT_DEMO_ANALOG_CHANNELS) {
        rt_kprintf("usage: cia401 condition <ai|ao> <channel> <scale:int32> <offset:int32>\n");
        return -RT_EINVAL;
    }

#if !defined(PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING)
    rt_kprintf("cia401: analogue conditioning is not enabled\n");
    return -RT_ENOSYS;
#else
    CANopenNodeRTT *app;
    CO_401_device_RTT_t *runtime;
    CO_t *co;
    rt_err_t ret;
    ODR_t odRet;
    uint8_t subIndex;

    ret = CO_401_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        rt_kprintf("cia401: runtime is not ready\n");
        return ret;
    }
    subIndex = (uint8_t)(channel + 1U);
    if (strcmp(argv[2], "ai") == 0) {
        odRet = OD_set_i32(runtime->device.bound.analogInputPrescaling32, subIndex, (int32_t)scaling, false);
        if (odRet == ODR_OK) {
            odRet = OD_set_i32(runtime->device.bound.analogInputOffset32, subIndex, (int32_t)offset, false);
        }
    } else {
        odRet = OD_set_i32(runtime->device.bound.analogOutputScaling32, subIndex, (int32_t)scaling, false);
        if (odRet == ODR_OK) {
            odRet = OD_set_i32(runtime->device.bound.analogOutputOffset32, subIndex, (int32_t)offset, false);
        }
    }
    if (odRet != ODR_OK) {
        CO_401_mshUnlock(app, co);
        rt_kprintf("cia401: analogue conditioning configuration failed (%d)\n", (int)odRet);
        return -RT_ERROR;
    }
    CO_401_mshPublishAndUnlock(app, runtime, co);
    rt_kprintf("cia401: %s channel=%lu scale=%ld offset=%ld queued\n", argv[2], channel, scaling, offset);
    return RT_EOK;
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING */
}

static int CO_401_mshFault(int argc, char **argv)
{
    CANopenNodeRTT *app;
    CO_401_device_RTT_t *runtime;
    CO_t *co;
    bool active;
    rt_err_t ret;

    if (argc != 4 || !CO_401_mshParseBool(argv[3], &active)) {
        rt_kprintf("usage: cia401 fault <digital|analog> <0|1>\n");
        return -RT_EINVAL;
    }

    ret = CO_401_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        rt_kprintf("cia401: runtime is not ready\n");
        return ret;
    }
    if (strcmp(argv[2], "digital") == 0) {
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
        CO_401_device_setDigitalOutputFault(&runtime->device, active);
#else
        CO_401_mshUnlock(app, co);
        rt_kprintf("cia401: digital output fail-safe is not enabled\n");
        return -RT_ENOSYS;
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE */
    } else if (strcmp(argv[2], "analog") == 0) {
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
        CO_401_device_setAnalogOutputFault(&runtime->device, active);
#else
        CO_401_mshUnlock(app, co);
        rt_kprintf("cia401: analogue output fail-safe is not enabled\n");
        return -RT_ENOSYS;
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
    } else {
        CO_401_mshUnlock(app, co);
        rt_kprintf("usage: cia401 fault <digital|analog> <0|1>\n");
        return -RT_EINVAL;
    }

    CO_401_mshPublishAndUnlock(app, runtime, co);
    rt_kprintf("cia401: %s product fault=%u queued\n", argv[2], active ? 1U : 0U);
    return RT_EOK;
}

static int CO_401_mshSupervision(int argc, char **argv)
{
    CANopenNodeRTT *app;
    CO_401_device_RTT_t *runtime;
    CO_t *co;
    bool active;
    rt_err_t ret;

    if (argc == 3 && strcmp(argv[2], "reset") == 0) {
        ret = CO_401_mshLock(&app, &runtime, &co);
        if (ret != RT_EOK) {
            rt_kprintf("cia401: runtime is not ready\n");
            return ret;
        }
        /* Bench-only escape hatch: production code may clear this latch only on application reset. */
        CO_401_device_RTT_demoSetOutputSupervisionEstablished(false);
        CO_401_device_RTT_demoSetOutputSupervisionFault(false);
        runtime->device.outputSupervisionReady = false;
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
        CO_401_device_setCommunicationFault(&runtime->device, false);
#endif /* output fail-safe */
        CO_401_mshPublishAndUnlock(app, runtime, co);
        rt_kprintf("cia401: supervision source and latched ready state cleared for bench test\n");
        return RT_EOK;
    }

    if (argc != 4 || !CO_401_mshParseBool(argv[3], &active)
        || (strcmp(argv[2], "ready") != 0 && strcmp(argv[2], "fault") != 0)) {
        rt_kprintf("usage: cia401 supervision <ready|fault> <0|1> | reset\n");
        return -RT_EINVAL;
    }

    ret = CO_401_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        rt_kprintf("cia401: runtime is not ready\n");
        return ret;
    }
    if (strcmp(argv[2], "ready") == 0) {
        CO_401_device_RTT_demoSetOutputSupervisionEstablished(active);
    } else {
        CO_401_device_RTT_demoSetOutputSupervisionFault(active);
    }
    CO_401_mshPublishAndUnlock(app, runtime, co);
    rt_kprintf("cia401: supervision %s source=%u queued\n", argv[2], active ? 1U : 0U);
    if (strcmp(argv[2], "ready") == 0 && !active) {
        rt_kprintf("cia401: note: an already latched ready state stays set; use 'cia401 supervision reset' for bench-only clear\n");
    }
    return RT_EOK;
}

static int CO_401_mshProcess(int argc, char **argv)
{
    CANopenNodeRTT *app;
    CO_401_device_RTT_t *runtime;
    CO_t *co;
    rt_err_t ret;

    (void)argv;
    if (argc != 2) {
        rt_kprintf("usage: cia401 process\n");
        return -RT_EINVAL;
    }

    ret = CO_401_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        rt_kprintf("cia401: runtime is not ready\n");
        return ret;
    }
    CO_401_mshPublishAndUnlock(app, runtime, co);
    rt_kprintf("cia401: profile process pass queued\n");
    return RT_EOK;
}

static int cia401(int argc, char **argv)
{
    if (argc < 2 || strcmp(argv[1], "help") == 0) {
        CO_401_mshPrintUsage();
        return argc < 2 ? -RT_EINVAL : RT_EOK;
    }
    if (strcmp(argv[1], "status") == 0) {
        return CO_401_mshStatus(argc, argv);
    }
    if (strcmp(argv[1], "di") == 0) {
        return CO_401_mshDigitalInput(argc, argv);
    }
    if (strcmp(argv[1], "ai") == 0) {
        return CO_401_mshAnalogInput(argc, argv);
    }
    if (strcmp(argv[1], "do") == 0) {
        return CO_401_mshDigitalOutput(argc, argv);
    }
    if (strcmp(argv[1], "ao") == 0) {
        return CO_401_mshAnalogOutput(argc, argv);
    }
    if (strcmp(argv[1], "devent") == 0) {
        return CO_401_mshDigitalEvent(argc, argv);
    }
    if (strcmp(argv[1], "aevent") == 0) {
        return CO_401_mshAnalogEvent(argc, argv);
    }
    if (strcmp(argv[1], "dfailsafe") == 0) {
        return CO_401_mshDigitalFailsafe(argc, argv);
    }
    if (strcmp(argv[1], "afailsafe") == 0) {
        return CO_401_mshAnalogFailsafe(argc, argv);
    }
    if (strcmp(argv[1], "condition") == 0) {
        return CO_401_mshCondition(argc, argv);
    }
    if (strcmp(argv[1], "fault") == 0) {
        return CO_401_mshFault(argc, argv);
    }
    if (strcmp(argv[1], "supervision") == 0) {
        return CO_401_mshSupervision(argc, argv);
    }
    if (strcmp(argv[1], "process") == 0) {
        return CO_401_mshProcess(argc, argv);
    }

    CO_401_mshPrintUsage();
    return -RT_EINVAL;
}
MSH_CMD_EXPORT(cia401, control and inspect the local CiA 401 software demo);

void CO_401_device_RTT_mshBind(CANopenNodeRTT *app, CO_401_device_RTT_t *runtime)
{
    if (CO_401_mshBindingTake() != RT_EOK) {
        return;
    }
    CO_401_msh.app = app;
    CO_401_msh.runtime = runtime;
    (void)rt_mutex_release(&CO_401_mshBindingMutex);
}

void CO_401_device_RTT_mshUnbind(CANopenNodeRTT *app, CO_401_device_RTT_t *runtime)
{
    if (CO_401_mshBindingTake() != RT_EOK) {
        return;
    }
    if (CO_401_msh.app == app && CO_401_msh.runtime == runtime) {
        CO_401_msh.app = NULL;
        CO_401_msh.runtime = NULL;
    }
    (void)rt_mutex_release(&CO_401_mshBindingMutex);
}

#endif /* defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_MSH) */
