/**
 * @file cia401-rtt-lifecycle-host-test.c
 * @brief Host contract test for the CiA 401 RT-Thread lifecycle adapter.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#define OD_DEFINITION
#include "CO_401_device_RTT.h"
#undef OD_DEFINITION
#include "CO_app_RTT.h"

#define ENTRY_COUNT 13U
#define CIA401_TEST_EMCY_STATUS_BIT 0x48U
#define TEST_ASSERT(x) \
    do { \
        if (!(x)) { \
            fprintf(stderr, "CIA401_RTT_LIFECYCLE_FAIL:%s:%d:%s\n", __func__, __LINE__, #x); \
            return false; \
        } \
    } while (0)

typedef struct {
    uint32_t deviceType;
    uint8_t subAi, subAo, subTrigger, subSource, subUpper, subLower, subDelta, subNeg, subPos;
    uint8_t subErrMode, subErrValue;
    uint8_t interruptEnable;
    int16_t ai[1], ao[1];
    uint8_t trigger[1], errorMode[1];
    uint32_t source[1], delta[1], negDelta[1], posDelta[1];
    int32_t upper[1], lower[1], errorValue[1];
    OD_obj_var_t deviceTypeObj, interruptEnableObj;
    OD_obj_array_t aiObj, aoObj, triggerObj, sourceObj, upperObj, lowerObj, deltaObj, negObj, posObj;
    OD_obj_array_t errorModeObj, errorValueObj;
    OD_entry_t entries[ENTRY_COUNT];
    OD_t od;
} fixture_t;

typedef struct {
    int16_t input;
    int16_t output;
    unsigned reads;
    unsigned writes;
    CO_401_io_result_t nextWriteResult;
    bool supervised;
    bool outputSettingPeerFault;
} io_t;

static const CO_RTT_lifecycle_ops_t *registeredOps;
static void *registeredContext;
static CO_RTT_lifecycle_context_release_t registeredRelease;
static unsigned allocCount;
static unsigned freeCount;
static unsigned semResetCount;
static unsigned semReleaseCount;
static unsigned threadStartCount;
static unsigned threadDeleteCount;
static bool failThreadCreate;
static struct rt_thread fakeThread;
static struct rt_thread fakeRealtimeThread;
static struct rt_device fakeDevice;
static rt_thread_t currentThread;
static rt_ssize_t nextDeviceWriteResult;
static unsigned deviceWriteCount;
#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE)
static unsigned emcyReportCount;
static unsigned emcyResetCount;
static uint8_t emcyLastErrorBit;
static uint16_t emcyLastErrorCode;
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE */
static jmp_buf workerExit;
static bool workerExitArmed;

static void initArray(OD_obj_array_t *obj, uint8_t *sub0, void *data, OD_size_t size, OD_attr_t attr)
{
    obj->dataOrig0 = sub0;
    obj->dataOrig = data;
    obj->attribute0 = ODA_SDO_R;
    obj->attribute = attr;
    obj->dataElementLength = size;
    obj->dataElementSizeof = size;
}

static void addEntry(fixture_t *fixture, uint16_t index, uint8_t count, uint8_t type, const void *object)
{
    OD_entry_t *entry = &fixture->entries[fixture->od.size++];

    entry->index = index;
    entry->subEntriesCount = count;
    entry->odObjectType = type;
    entry->odObject = object;
    entry->extension = NULL;
}

static void fixtureInit(fixture_t *fixture)
{
    (void)memset(fixture, 0, sizeof(*fixture));
    fixture->deviceType = CO_401_deviceTypeForCapabilities(CO_401_CAP_ANALOG_INPUT | CO_401_CAP_ANALOG_OUTPUT);
    fixture->subAi = fixture->subAo = fixture->subTrigger = fixture->subUpper = fixture->subLower = 1U;
    fixture->subDelta = fixture->subNeg = fixture->subPos = 1U;
    fixture->subErrMode = fixture->subErrValue = 1U;
    fixture->subSource = 1U;
    fixture->errorMode[0] = 1U;

    fixture->deviceTypeObj.dataOrig = &fixture->deviceType;
    fixture->deviceTypeObj.attribute = ODA_SDO_R | ODA_MB;
    fixture->deviceTypeObj.dataLength = 4U;
    fixture->interruptEnableObj.dataOrig = &fixture->interruptEnable;
    fixture->interruptEnableObj.attribute = ODA_SDO_RW;
    fixture->interruptEnableObj.dataLength = 1U;
    initArray(&fixture->aiObj, &fixture->subAi, fixture->ai, 2U, ODA_SDO_R | ODA_TPDO | ODA_MB);
    initArray(&fixture->aoObj, &fixture->subAo, fixture->ao, 2U, ODA_SDO_RW | ODA_RPDO | ODA_MB);
    initArray(&fixture->triggerObj, &fixture->subTrigger, fixture->trigger, 1U, ODA_SDO_RW);
    initArray(&fixture->sourceObj, &fixture->subSource, fixture->source, 4U, ODA_SDO_R | ODA_TPDO | ODA_MB);
    initArray(&fixture->upperObj, &fixture->subUpper, fixture->upper, 4U, ODA_SDO_RW | ODA_MB);
    initArray(&fixture->lowerObj, &fixture->subLower, fixture->lower, 4U, ODA_SDO_RW | ODA_MB);
    initArray(&fixture->deltaObj, &fixture->subDelta, fixture->delta, 4U, ODA_SDO_RW | ODA_MB);
    initArray(&fixture->negObj, &fixture->subNeg, fixture->negDelta, 4U, ODA_SDO_RW | ODA_MB);
    initArray(&fixture->posObj, &fixture->subPos, fixture->posDelta, 4U, ODA_SDO_RW | ODA_MB);
    initArray(&fixture->errorModeObj, &fixture->subErrMode, fixture->errorMode, 1U, ODA_SDO_RW);
    initArray(&fixture->errorValueObj, &fixture->subErrValue, fixture->errorValue, 4U, ODA_SDO_RW | ODA_MB);

    fixture->od.list = fixture->entries;
    addEntry(fixture, CO_401_INDEX_DEVICE_TYPE, 1U, ODT_VAR, &fixture->deviceTypeObj);
    addEntry(fixture, CO_401_INDEX_ANALOG_INPUT_16, 2U, ODT_ARR, &fixture->aiObj);
    addEntry(fixture, CO_401_INDEX_ANALOG_OUTPUT_16, 2U, ODT_ARR, &fixture->aoObj);
    addEntry(fixture, CO_401_INDEX_ANALOG_INTERRUPT_TRIGGER, 2U, ODT_ARR, &fixture->triggerObj);
    addEntry(fixture, CO_401_INDEX_ANALOG_INTERRUPT_SOURCE, 2U, ODT_ARR, &fixture->sourceObj);
    addEntry(fixture, CO_401_INDEX_ANALOG_INTERRUPT_ENABLE, 1U, ODT_VAR, &fixture->interruptEnableObj);
    addEntry(fixture, CO_401_INDEX_ANALOG_INTERRUPT_UPPER_32, 2U, ODT_ARR, &fixture->upperObj);
    addEntry(fixture, CO_401_INDEX_ANALOG_INTERRUPT_LOWER_32, 2U, ODT_ARR, &fixture->lowerObj);
    addEntry(fixture, CO_401_INDEX_ANALOG_INTERRUPT_DELTA_U32, 2U, ODT_ARR, &fixture->deltaObj);
    addEntry(fixture, CO_401_INDEX_ANALOG_INTERRUPT_NEG_DELTA_U32, 2U, ODT_ARR, &fixture->negObj);
    addEntry(fixture, CO_401_INDEX_ANALOG_INTERRUPT_POS_DELTA_U32, 2U, ODT_ARR, &fixture->posObj);
    addEntry(fixture, CO_401_INDEX_ANALOG_OUTPUT_ERROR_MODE, 2U, ODT_ARR, &fixture->errorModeObj);
    addEntry(fixture, CO_401_INDEX_ANALOG_OUTPUT_ERROR_VALUE_32, 2U, ODT_ARR, &fixture->errorValueObj);
}

static CO_401_io_result_t readAnalog16(void *object, uint8_t channel, int16_t *value)
{
    io_t *io = object;

    if (channel != 0U || value == NULL) {
        return CO_401_IO_ERROR;
    }
    io->reads++;
    *value = io->input;
    return CO_401_IO_OK;
}

static CO_401_io_result_t writeAnalog16(void *object, uint8_t channel, int16_t value)
{
    io_t *io = object;

    if (channel != 0U) {
        return CO_401_IO_ERROR;
    }
    io->writes++;
    if (io->nextWriteResult != CO_401_IO_OK) {
        return io->nextWriteResult;
    }
    io->output = value;
    return CO_401_IO_OK;
}

static const CO_401_io_if_t ioIf = {
    .readDigital8 = NULL,
    .writeDigital8 = NULL,
    .readAnalog16 = readAnalog16,
    .writeAnalog16 = writeAnalog16,
};

rt_err_t CO_RTT_lifecycleRegisterEx(CANopenNodeRTT *app, const CO_RTT_lifecycle_ops_t *ops, void *context,
                                    CO_RTT_lifecycle_context_release_t release)
{
    if (app == NULL || ops == NULL || context == NULL || registeredOps != NULL) {
        return -RT_EBUSY;
    }
    registeredOps = ops;
    registeredContext = context;
    registeredRelease = release;
    return RT_EOK;
}

rt_bool_t CO_RTT_lifecycleHasOps(const CANopenNodeRTT *app, const CO_RTT_lifecycle_ops_t *ops)
{
    return app != NULL && ops == registeredOps ? RT_TRUE : RT_FALSE;
}

rt_err_t rt_sem_init(struct rt_semaphore *sem, const char *name, unsigned value, unsigned flag)
{
    (void)name;
    (void)flag;
    sem->value = value;
    sem->initialized = RT_TRUE;
    return RT_EOK;
}

rt_err_t rt_sem_detach(struct rt_semaphore *sem)
{
    sem->initialized = RT_FALSE;
    sem->value = 0U;
    return RT_EOK;
}

rt_err_t rt_sem_take(struct rt_semaphore *sem, int timeout)
{
    (void)timeout;
    if (sem == NULL || sem->initialized != RT_TRUE) {
        return -RT_ERROR;
    }
    if (sem->value == 0U) {
        if (workerExitArmed) {
            workerExitArmed = false;
            longjmp(workerExit, 1);
        }
        return -RT_ERROR;
    }
    sem->value--;
    return RT_EOK;
}

rt_err_t rt_sem_release(struct rt_semaphore *sem)
{
    if (sem == NULL || sem->initialized != RT_TRUE) {
        return -RT_ERROR;
    }
    sem->value++;
    semReleaseCount++;
    return RT_EOK;
}

rt_err_t rt_sem_control(struct rt_semaphore *sem, int cmd, void *arg)
{
    (void)arg;
    if (sem == NULL || sem->initialized != RT_TRUE || cmd != RT_IPC_CMD_RESET) {
        return -RT_ERROR;
    }
    sem->value = 0U;
    semResetCount++;
    return RT_EOK;
}

rt_err_t rt_mutex_init(struct rt_mutex *mutex, const char *name, unsigned flag)
{
    (void)name;
    (void)flag;
    if (mutex == NULL) {
        return -RT_EINVAL;
    }
    mutex->locked = RT_FALSE;
    return RT_EOK;
}

rt_err_t rt_mutex_detach(struct rt_mutex *mutex)
{
    if (mutex == NULL) {
        return -RT_EINVAL;
    }
    mutex->locked = RT_FALSE;
    return RT_EOK;
}

rt_err_t rt_mutex_take(struct rt_mutex *mutex, int timeout)
{
    (void)timeout;
    if (mutex == NULL || mutex->locked == RT_TRUE) {
        return -RT_EBUSY;
    }
    mutex->locked = RT_TRUE;
    return RT_EOK;
}

rt_err_t rt_mutex_release(struct rt_mutex *mutex)
{
    if (mutex == NULL) {
        return -RT_EINVAL;
    }
    mutex->locked = RT_FALSE;
    return RT_EOK;
}

rt_thread_t rt_thread_create(const char *name, void (*entry)(void *parameter), void *parameter,
                             unsigned stackSize, unsigned priority, unsigned tick)
{
    (void)name;
    (void)stackSize;
    (void)priority;
    (void)tick;
    if (failThreadCreate) {
        return RT_NULL;
    }
    fakeThread.entry = entry;
    fakeThread.parameter = parameter;
    return &fakeThread;
}

rt_err_t rt_thread_startup(rt_thread_t thread)
{
    if (thread == RT_NULL) {
        return -RT_EINVAL;
    }
    threadStartCount++;
    return RT_EOK;
}

rt_err_t rt_thread_delete(rt_thread_t thread)
{
    if (thread == RT_NULL) {
        return -RT_EINVAL;
    }
    threadDeleteCount++;
    return RT_EOK;
}

rt_thread_t rt_thread_self(void)
{
    return currentThread;
}

void *rt_calloc(rt_size_t count, rt_size_t size)
{
    void *ptr = calloc(count, size);

    if (ptr != NULL) {
        allocCount++;
    }
    return ptr;
}

void rt_free(void *ptr)
{
    if (ptr != NULL) {
        freeCount++;
    }
    free(ptr);
}

rt_ssize_t rt_device_write(rt_device_t dev, rt_size_t pos, const void *buffer, rt_size_t size)
{
    (void)pos;
    (void)buffer;
    if (dev == RT_NULL || size != sizeof(struct rt_can_msg)) {
        return -RT_EINVAL;
    }
    deviceWriteCount++;
    return nextDeviceWriteResult;
}

rt_base_t rt_hw_interrupt_disable(void)
{
    return 0;
}

void rt_hw_interrupt_enable(rt_base_t level)
{
    (void)level;
}

void CO_error(CO_EM_t *em, bool_t setError, const uint8_t errorBit, uint16_t errorCode, uint32_t infoCode)
{
#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE)
    if (setError) {
        emcyReportCount++;
    } else {
        emcyResetCount++;
    }
    emcyLastErrorBit = errorBit;
    emcyLastErrorCode = errorCode;
#else
    (void)setError;
    (void)errorBit;
    (void)errorCode;
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE */
    (void)em;
    (void)infoCode;
}

#if defined(CIA401_TEST_BITWISE_TPDO)
static void setFrameBits(uint8_t *data, uint16_t bitOffset, uint16_t bitLength, uint32_t value)
{
    uint16_t bit;

    for (bit = 0U; bit < bitLength; bit++) {
        const uint16_t frameBit = (uint16_t)(bitOffset + bit);
        const uint8_t mask = (uint8_t)(1U << (frameBit & 7U));

        if ((value & ((uint32_t)1UL << bit)) != 0U) {
            data[frameBit >> 3] |= mask;
        } else {
            data[frameBit >> 3] &= (uint8_t)(~mask);
        }
    }
}
#endif /* CIA401_TEST_BITWISE_TPDO */

static void prepareTpdoFrame(CO_TPDO_t *tpdo, CO_CANtx_t *buffer, uint16_t input, uint32_t source)
{
    (void)memset(buffer->data, 0, sizeof(buffer->data));
#if defined(CIA401_TEST_BITWISE_TPDO)
    tpdo->PDO_common.mappedObjectsCount = 3U;
    tpdo->PDO_common.OD_IO[0].stream.index = 0x2000U;
    tpdo->PDO_common.OD_IO[0].stream.subIndex = 0U;
    tpdo->PDO_common.OD_IO[0].stream.dataOffset = 1U;
    tpdo->PDO_common.OD_IO[1].stream.index = CO_401_INDEX_ANALOG_INPUT_16;
    tpdo->PDO_common.OD_IO[1].stream.subIndex = 1U;
    tpdo->PDO_common.OD_IO[1].stream.dataOffset = 16U;
    tpdo->PDO_common.OD_IO[2].stream.index = CO_401_INDEX_ANALOG_INTERRUPT_SOURCE;
    tpdo->PDO_common.OD_IO[2].stream.subIndex = 1U;
    tpdo->PDO_common.OD_IO[2].stream.dataOffset = 32U;
    buffer->DLC = 7U;
    setFrameBits(buffer->data, 0U, 1U, 1U);
    setFrameBits(buffer->data, 1U, 16U, input);
    setFrameBits(buffer->data, 17U, 32U, source);
#else
    tpdo->PDO_common.mappedObjectsCount = 2U;
    tpdo->PDO_common.OD_IO[0].stream.index = CO_401_INDEX_ANALOG_INPUT_16;
    tpdo->PDO_common.OD_IO[0].stream.subIndex = 1U;
    tpdo->PDO_common.OD_IO[0].stream.dataOffset = 2U;
    tpdo->PDO_common.OD_IO[1].stream.index = CO_401_INDEX_ANALOG_INTERRUPT_SOURCE;
    tpdo->PDO_common.OD_IO[1].stream.subIndex = 1U;
    tpdo->PDO_common.OD_IO[1].stream.dataOffset = 4U;
    buffer->DLC = 6U;
    CO_setUint16(&buffer->data[0], input);
    CO_setUint32(&buffer->data[2], source);
#endif /* CIA401_TEST_BITWISE_TPDO */
}

#if defined(CIA401_TEST_BITWISE_TPDO)
static void preparePartialTpdoFrame(CO_TPDO_t *tpdo, CO_CANtx_t *buffer, uint8_t inputLow, uint8_t sourceLow)
{
    (void)memset(buffer->data, 0, sizeof(buffer->data));
    tpdo->PDO_common.mappedObjectsCount = 2U;
    tpdo->PDO_common.OD_IO[0].stream.index = CO_401_INDEX_ANALOG_INPUT_16;
    tpdo->PDO_common.OD_IO[0].stream.subIndex = 1U;
    tpdo->PDO_common.OD_IO[0].stream.dataOffset = 8U;
    tpdo->PDO_common.OD_IO[1].stream.index = CO_401_INDEX_ANALOG_INTERRUPT_SOURCE;
    tpdo->PDO_common.OD_IO[1].stream.subIndex = 1U;
    tpdo->PDO_common.OD_IO[1].stream.dataOffset = 8U;
    buffer->DLC = 2U;
    setFrameBits(buffer->data, 0U, 8U, inputLow);
    setFrameBits(buffer->data, 8U, 8U, sourceLow);
}
#endif /* CIA401_TEST_BITWISE_TPDO */

static void resetStubs(void)
{
    registeredOps = NULL;
    registeredContext = NULL;
    registeredRelease = NULL;
    allocCount = 0U;
    freeCount = 0U;
    semResetCount = 0U;
    semReleaseCount = 0U;
    threadStartCount = 0U;
    threadDeleteCount = 0U;
    failThreadCreate = false;
    workerExitArmed = false;
    currentThread = RT_NULL;
    nextDeviceWriteResult = (rt_ssize_t)sizeof(struct rt_can_msg);
    deviceWriteCount = 0U;
#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE)
    emcyReportCount = 0U;
    emcyResetCount = 0U;
    emcyLastErrorBit = 0U;
    emcyLastErrorCode = 0U;
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE */
    (void)memset(&fakeThread, 0, sizeof(fakeThread));
    (void)memset(&fakeRealtimeThread, 0, sizeof(fakeRealtimeThread));
    (void)memset(&fakeDevice, 0, sizeof(fakeDevice));
}

static bool outputSupervisionEstablished(void *object, const CO_t *co)
{
    io_t *io = object;

    (void)co;
    return io != NULL && io->supervised;
}

static bool outputSupervisionFaultActive(void *object, const CO_t *co)
{
    io_t *io = object;

    (void)co;
    return io != NULL && io->outputSettingPeerFault;
}

#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_AUTOSTART)
static bool test_autoattach_output_supervision_contract(void)
{
    CANopenNodeRTT app = {0};
    io_t io = {0};
    CO_401_device_RTT_config_t inputOnly = {
        .device = {
            .io = &ioIf,
            .ioObject = &io,
            .analogInputChannels = 1U,
        },
    };
    CO_401_device_RTT_config_t output = {
        .device = {
            .io = &ioIf,
            .ioObject = &io,
            .analogOutputChannels = 1U,
        },
    };
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE) \
    && !defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    CO_401_device_RTT_config_t digitalOutputWithoutFailSafe = {
        .device = {
            .io = &ioIf,
            .ioObject = &io,
            .digitalOutputBanks = 1U,
        },
        .outputSupervisionEstablished = outputSupervisionEstablished,
        .outputSupervisionObject = &io,
    };
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE && !PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE */

    resetStubs();
    TEST_ASSERT(CO_401_device_RTT_autoAttach(&app, &output) == -RT_EINVAL);
    TEST_ASSERT(allocCount == 0U);
    TEST_ASSERT(registeredOps == NULL);

    resetStubs();
    TEST_ASSERT(CO_401_device_RTT_autoAttach(&app, &inputOnly) == RT_EOK);
    TEST_ASSERT(allocCount == 1U);
    TEST_ASSERT(registeredRelease != NULL);
    registeredRelease(registeredContext);
    TEST_ASSERT(freeCount == 1U);

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE) \
    && !defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    resetStubs();
    TEST_ASSERT(CO_401_device_RTT_autoAttach(&app, &digitalOutputWithoutFailSafe) == RT_EOK);
    TEST_ASSERT(allocCount == 1U);
    TEST_ASSERT(registeredRelease != NULL);
    registeredRelease(registeredContext);
    TEST_ASSERT(freeCount == 1U);
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE && !PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE */

    resetStubs();
    output.outputSupervisionEstablished = outputSupervisionEstablished;
    output.outputSupervisionObject = &io;
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    TEST_ASSERT(CO_401_device_RTT_autoAttach(&app, &output) == -RT_EINVAL);
    TEST_ASSERT(allocCount == 0U);
    TEST_ASSERT(registeredOps == NULL);
    output.outputSupervisionFaultActive = outputSupervisionFaultActive;
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
    TEST_ASSERT(CO_401_device_RTT_autoAttach(&app, &output) == RT_EOK);
    TEST_ASSERT(allocCount == 1U);
    TEST_ASSERT(registeredRelease != NULL);
    registeredRelease(registeredContext);
    TEST_ASSERT(freeCount == 1U);
    resetStubs();
    return true;
}

static bool test_autoattach_supervision_is_visible_to_first_output_write(void)
{
    fixture_t fixture;
    io_t io = {0};
    CANopenNodeRTT app = {0};
    CO_CANmodule_t canModule = {0};
    CO_NMT_t nmt = {0};
    CO_EM_t em = {0};
    CO_t co = {0};
    CO_401_device_RTT_t *runtime;
    CO_401_device_RTT_config_t config = {
        .device = {
            .io = &ioIf,
            .ioObject = &io,
            .analogInputChannels = 1U,
            .analogOutputChannels = 1U,
        },
        .outputSupervisionEstablished = outputSupervisionEstablished,
        .outputSupervisionFaultActive = outputSupervisionFaultActive,
        .outputSupervisionObject = &io,
    };

    resetStubs();
    fixtureInit(&fixture);
    canModule.CANnormal = true;
    co.CANmodule = &canModule;
    co.NMT = &nmt;
    co.em = &em;
    nmt.operatingState = CO_NMT_PRE_OPERATIONAL;

    TEST_ASSERT(CO_401_device_RTT_autoAttach(&app, &config) == RT_EOK);
    TEST_ASSERT(registeredOps != NULL);
    TEST_ASSERT(registeredContext != NULL);
    TEST_ASSERT(registeredRelease != NULL);
    runtime = (CO_401_device_RTT_t *)registeredContext;

    app.canOpenStack = &co;
    TEST_ASSERT(registeredOps->communicationBind(&app, &co, &fixture.od, registeredContext) == RT_EOK);
    TEST_ASSERT(registeredOps->runtimeInit(&app, registeredContext) == RT_EOK);
    TEST_ASSERT(registeredOps->runtimeStart(&app, registeredContext) == RT_EOK);
    registeredOps->communicationReady(&app, registeredContext);
    TEST_ASSERT(!runtime->device.outputSupervisionReady);

    TEST_ASSERT(OD_set_i16(runtime->device.bound.analogOutput16, 1U, 41, false) == ODR_DATA_DEV_STATE);
    TEST_ASSERT(fixture.ao[0] == 0);

    /* Product supervision becomes true; deliberately do not run the co_401 worker. */
    io.supervised = true;
    TEST_ASSERT(!runtime->device.outputSupervisionReady);
    TEST_ASSERT(OD_set_i16(runtime->device.bound.analogOutput16, 1U, 42, false) == ODR_OK);
    TEST_ASSERT(runtime->device.outputSupervisionReady);
    TEST_ASSERT(fixture.ao[0] == 42);

    registeredOps->communicationStop(&app, registeredContext);
    registeredOps->communicationQuiesced(&app, registeredContext);
    registeredOps->runtimeDeinit(&app, registeredContext);
    registeredRelease(registeredContext);
    TEST_ASSERT(freeCount == 1U);
    resetStubs();
    return true;
}
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_AUTOSTART */

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
static bool test_manual_attach_output_fault_classifier_contract(void)
{
    CANopenNodeRTT app = {0};
    io_t io = {0};
    CO_401_device_RTT_t runtime = {0};
    CO_401_device_RTT_config_t output = {
        .device = {
            .io = &ioIf,
            .ioObject = &io,
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
            .analogOutputChannels = 1U,
#else
            .digitalOutputBanks = 1U,
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
        },
    };

    resetStubs();
    TEST_ASSERT(CO_401_device_RTT_attach(&app, &runtime, &output) == -RT_EINVAL);
    TEST_ASSERT(runtime.attached == RT_FALSE);
    TEST_ASSERT(registeredOps == NULL);
    TEST_ASSERT(registeredContext == NULL);

    output.outputSupervisionFaultActive = outputSupervisionFaultActive;
    TEST_ASSERT(CO_401_device_RTT_attach(&app, &runtime, &output) == RT_EOK);
    TEST_ASSERT(runtime.attached == RT_TRUE);
    TEST_ASSERT(registeredOps != NULL);
    TEST_ASSERT(registeredContext == &runtime);
    resetStubs();
    return true;
}
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */

static bool runWorkerPass(CO_401_device_RTT_t *runtime)
{
    if (runtime == NULL || runtime->workerThread == RT_NULL || runtime->workerThread->entry == NULL) {
        return false;
    }

    runtime->cia401Sem.value = 1U;
    workerExitArmed = true;
    if (setjmp(workerExit) == 0) {
        runtime->workerThread->entry(runtime->workerThread->parameter);
        workerExitArmed = false;
        return false;
    }

    workerExitArmed = false;
    return true;
}

static void setEmergencyBit(CO_EM_t *em, uint8_t errorBit, bool active)
{
    const uint8_t index = errorBit >> 3;
    const uint8_t mask = (uint8_t)(1U << (errorBit & 7U));

    if (active) {
        em->errorStatusBits[index] |= mask;
    } else {
        em->errorStatusBits[index] &= (uint8_t)(~mask);
    }
}

static bool test_lifecycle_reset_rebind_and_deinit(void)
{
    fixture_t first, second;
    io_t io = {0};
    CANopenNodeRTT app = {0};
    CO_401_device_RTT_t runtime = {0};
    CO_CANmodule_t canModule = {0};
    CO_CANtx_t sdoTx = {0}, tpdoTx = {0};
    CO_SDOserver_t sdoServer[1] = {{0}};
    CO_TPDO_t tpdo[1];
    CO_config_t coConfig = {0};
    CO_NMT_t nmt = {0};
    CO_EM_t em = {0};
    CO_t co = {0};
    CO_RTT_CANtxSuccessCallback_t capturedCallback;
    void *capturedObject;
    unsigned writesBefore;
    OD_size_t countRead;
    int16_t sdoInputValue;
    uint32_t sdoSourceValue;
    CO_401_device_RTT_config_t config = {
        .device = {
            .io = &ioIf,
            .ioObject = &io,
            .analogInputChannels = 1U,
            .analogOutputChannels = 1U,
        },
        .outputSupervisionEstablished = outputSupervisionEstablished,
        .outputSupervisionFaultActive = outputSupervisionFaultActive,
        .outputSupervisionObject = &io,
    };

    resetStubs();
    (void)memset(tpdo, 0, sizeof(tpdo));
    fixtureInit(&first);
    fixtureInit(&second);
    coConfig.CNT_SDO_SRV = 1U;
    coConfig.CNT_TPDO = 1U;
    co.config = &coConfig;
    canModule.dev = &fakeDevice;
    canModule.CANnormal = true;
    co.CANmodule = &canModule;
    co.NMT = &nmt;
    co.em = &em;
    co.SDOserver = sdoServer;
    co.TPDO = tpdo;
    nmt.operatingState = CO_NMT_PRE_OPERATIONAL;
    sdoServer[0].CANtxBuff = &sdoTx;
    sdoServer[0].OD = &first.od;
    tpdo[0].CANtxBuff = &tpdoTx;

    TEST_ASSERT(CO_401_device_RTT_attach(&app, &runtime, &config) == RT_EOK);
    TEST_ASSERT(runtime.attached == RT_TRUE);
    TEST_ASSERT(registeredOps != NULL);
    TEST_ASSERT(registeredContext == &runtime);

    TEST_ASSERT(registeredOps->communicationBind(&app, &co, &first.od, registeredContext) == RT_EOK);
    TEST_ASSERT(runtime.deviceInitialized == RT_TRUE);
    TEST_ASSERT(runtime.device.odBound);
    TEST_ASSERT(first.entries[1].extension == &runtime.device.analogInput16Extension);
    TEST_ASSERT(first.entries[2].extension == &runtime.device.analogOutput16Extension);
    TEST_ASSERT(first.entries[4].extension == &runtime.device.analogInterruptSourceExtension);

    failThreadCreate = true;
    TEST_ASSERT(registeredOps->runtimeInit(&app, registeredContext) == -RT_ENOMEM);
    TEST_ASSERT(runtime.semInitialized == RT_FALSE);
    TEST_ASSERT(runtime.workerThread == RT_NULL);
    failThreadCreate = false;

    TEST_ASSERT(registeredOps->runtimeInit(&app, registeredContext) == RT_EOK);
    TEST_ASSERT(runtime.semInitialized == RT_TRUE);
    TEST_ASSERT(runtime.workerThread != RT_NULL);
    TEST_ASSERT(registeredOps->runtimeStart(&app, registeredContext) == RT_EOK);
    TEST_ASSERT(threadStartCount == 1U);

    app.canOpenStack = &co;
    app.rtThread = &fakeRealtimeThread;
    currentThread = &fakeRealtimeThread;
    registeredOps->communicationReady(&app, registeredContext);
    TEST_ASSERT(runtime.communicationReady == RT_TRUE);
    TEST_ASSERT(canModule.txSuccessObject == &runtime);
    TEST_ASSERT(canModule.txSuccessCallback != NULL);
    CO_RTT_CANsetTxEnabled(&canModule, true);

#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE)
    /* NMT transition callbacks must preserve both Operational entries even when the lower-priority worker never runs. */
    nmt.operatingState = CO_NMT_OPERATIONAL;
    registeredOps->nmtStateChanged(&app, registeredContext, nmt.operatingState);
    TEST_ASSERT(emcyReportCount == 1U);
    TEST_ASSERT(emcyResetCount == 0U);
    TEST_ASSERT(emcyLastErrorBit == CIA401_TEST_EMCY_STATUS_BIT);
    TEST_ASSERT(emcyLastErrorCode == 0x0080U);
    TEST_ASSERT(runtime.analogWarningReported == RT_TRUE);

    nmt.operatingState = CO_NMT_PRE_OPERATIONAL;
    registeredOps->nmtStateChanged(&app, registeredContext, nmt.operatingState);
    TEST_ASSERT(emcyResetCount == 1U);
    TEST_ASSERT(emcyLastErrorBit == CIA401_TEST_EMCY_STATUS_BIT);
    TEST_ASSERT(emcyLastErrorCode == CO_EMC_NO_ERROR);
    TEST_ASSERT(runtime.analogWarningReported == RT_FALSE);

    nmt.operatingState = CO_NMT_OPERATIONAL;
    registeredOps->nmtStateChanged(&app, registeredContext, nmt.operatingState);
    TEST_ASSERT(emcyReportCount == 2U);
    TEST_ASSERT(emcyLastErrorCode == 0x0080U);
    nmt.operatingState = CO_NMT_PRE_OPERATIONAL;
    registeredOps->nmtStateChanged(&app, registeredContext, nmt.operatingState);
    TEST_ASSERT(emcyResetCount == 2U);
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_EMCY_BRIDGE */

    /* Output commands are ignored until the configured CiA 401 supervision relationship is established. */
    TEST_ASSERT(!runtime.device.outputSupervisionReady);
    TEST_ASSERT(OD_set_i16(runtime.device.bound.analogOutput16, 1U, 123, false) == ODR_DATA_DEV_STATE);
    TEST_ASSERT(first.ao[0] == 0);
    TEST_ASSERT(runWorkerPass(&runtime));
    TEST_ASSERT(io.output == 0);

    io.supervised = true;
    TEST_ASSERT(!runtime.device.outputSupervisionReady);
    /* No worker pass occurs between the qualifying supervision fact and this network write. */
    TEST_ASSERT(OD_set_i16(runtime.device.bound.analogOutput16, 1U, 123, false) == ODR_OK);
    TEST_ASSERT(runtime.device.outputSupervisionReady);
    TEST_ASSERT(runWorkerPass(&runtime));
    TEST_ASSERT(io.output == 123);

    /* An aggregate Heartbeat timeout may belong to an unrelated monitored node and must not drive output fail-safe. */
    first.errorValue[0] = 7 << 16;
    TEST_ASSERT(nmt.operatingState == CO_NMT_PRE_OPERATIONAL);

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    /* A transient Stopped state must force one fail-safe pass even if Operational arrives before the worker runs. */
    nmt.operatingState = CO_NMT_STOPPED;
    registeredOps->nmtStateChanged(&app, registeredContext, nmt.operatingState);
    TEST_ASSERT(runtime.nmtStoppedApplyPending == RT_TRUE);
    nmt.operatingState = CO_NMT_OPERATIONAL;
    registeredOps->nmtStateChanged(&app, registeredContext, nmt.operatingState);
    TEST_ASSERT(runtime.nmtStoppedApplyPending == RT_TRUE);

    /* BUSY/ERROR do not satisfy the sticky Stop obligation because the physical output is unchanged. */
    io.nextWriteResult = CO_401_IO_BUSY;
    TEST_ASSERT(runWorkerPass(&runtime));
    TEST_ASSERT(io.output == 123);
    TEST_ASSERT(runtime.nmtStoppedApplyPending == RT_TRUE);
    TEST_ASSERT(runtime.device.nmtStopped);
    TEST_ASSERT(!runtime.device.failSafeOutputApplyComplete);

    io.nextWriteResult = CO_401_IO_ERROR;
    TEST_ASSERT(runWorkerPass(&runtime));
    TEST_ASSERT(io.output == 123);
    TEST_ASSERT(runtime.nmtStoppedApplyPending == RT_TRUE);
    TEST_ASSERT(runtime.device.nmtStopped);
    TEST_ASSERT(!runtime.device.failSafeOutputApplyComplete);

    io.nextWriteResult = CO_401_IO_OK;
    TEST_ASSERT(runWorkerPass(&runtime));
    TEST_ASSERT(io.output == 7);
    TEST_ASSERT(runtime.nmtStoppedApplyPending == RT_FALSE);
    TEST_ASSERT(!runtime.device.nmtStopped);
    TEST_ASSERT(runtime.device.failSafeOutputApplyComplete);
    TEST_ASSERT(runWorkerPass(&runtime));
    TEST_ASSERT(io.output == 123);
    nmt.operatingState = CO_NMT_PRE_OPERATIONAL;
    registeredOps->nmtStateChanged(&app, registeredContext, nmt.operatingState);
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */

    setEmergencyBit(&em, CO_EM_HEARTBEAT_CONSUMER, true);
    TEST_ASSERT(runWorkerPass(&runtime));
    TEST_ASSERT(!runtime.device.communicationFaultActive);
    TEST_ASSERT(io.output == 123);
    setEmergencyBit(&em, CO_EM_HEARTBEAT_CONSUMER, false);

    io.outputSettingPeerFault = true;
    TEST_ASSERT(runWorkerPass(&runtime));
    TEST_ASSERT(runtime.device.communicationFaultActive);
    TEST_ASSERT(io.output == 7);

    CO_401_device_setAnalogOutputFault(&runtime.device, true);
    io.outputSettingPeerFault = false;
    TEST_ASSERT(runWorkerPass(&runtime));
    TEST_ASSERT(!runtime.device.communicationFaultActive);
    TEST_ASSERT(io.output == 7);

    CO_401_device_setAnalogOutputFault(&runtime.device, false);
    TEST_ASSERT(runWorkerPass(&runtime));
    TEST_ASSERT(io.output == 123);

    setEmergencyBit(&em, CO_EM_CAN_TX_BUS_OFF, true);
    TEST_ASSERT(runWorkerPass(&runtime));
    TEST_ASSERT(runtime.device.communicationFaultActive);
    TEST_ASSERT(io.output == 7);
    TEST_ASSERT(nmt.operatingState == CO_NMT_PRE_OPERATIONAL);
    setEmergencyBit(&em, CO_EM_CAN_TX_BUS_OFF, false);
    TEST_ASSERT(runWorkerPass(&runtime));
    TEST_ASSERT(!runtime.device.communicationFaultActive);
    TEST_ASSERT(io.output == 123);

    /* SDO semantics are committed by the SDO server OD_IO read itself, before any CAN response is sent. */
    first.ai[0] = 7;
    TEST_ASSERT(OD_getSub(runtime.device.bound.analogInput16, 1U, &sdoServer[0].OD_IO, false) == ODR_OK);
    countRead = 0U;
    sdoInputValue = 0;
    TEST_ASSERT(sdoServer[0].OD_IO.read(&sdoServer[0].OD_IO.stream, &sdoInputValue, sizeof(sdoInputValue),
                                       &countRead) == ODR_OK);
    TEST_ASSERT(countRead == sizeof(sdoInputValue));
    TEST_ASSERT(sdoInputValue == 7);
    TEST_ASSERT(runtime.device.analogLastCommunicatedValid[0]);
    TEST_ASSERT(runtime.device.analogLastCommunicated[0] == 7);

    /* The stream pointer alone is insufficient: another logical OD must not commit this runtime. */
    sdoServer[0].OD = &second.od;
    first.ai[0] = 8;
    TEST_ASSERT(OD_getSub(runtime.device.bound.analogInput16, 1U, &sdoServer[0].OD_IO, false) == ODR_OK);
    countRead = 0U;
    TEST_ASSERT(sdoServer[0].OD_IO.read(&sdoServer[0].OD_IO.stream, &sdoInputValue, sizeof(sdoInputValue),
                                       &countRead) == ODR_OK);
    TEST_ASSERT(runtime.device.analogLastCommunicated[0] == 7);
    sdoServer[0].OD = &first.od;

    first.source[0] = 0x03U;
    TEST_ASSERT(OD_getSub(runtime.device.bound.analogInterruptSource, 1U, &sdoServer[0].OD_IO, false) == ODR_OK);
    countRead = 0U;
    sdoSourceValue = 0U;
    TEST_ASSERT(sdoServer[0].OD_IO.read(&sdoServer[0].OD_IO.stream, &sdoSourceValue, sizeof(sdoSourceValue),
                                       &countRead) == ODR_OK);
    TEST_ASSERT(countRead == sizeof(sdoSourceValue));
    TEST_ASSERT(sdoSourceValue == 0x03U);
    TEST_ASSERT(first.source[0] == 0U);

    /* A real successful CAN write for an SDO buffer must not commit CiA 401 TPDO state. */
    first.source[0] = 0x04U;
    TEST_ASSERT(CO_CANsend(&canModule, &sdoTx) == CO_ERROR_NO);
    TEST_ASSERT(first.source[0] == 0x04U);

    prepareTpdoFrame(&tpdo[0], &tpdoTx, 9U, 0x02U);
    first.source[0] = 0x06U;

    /* Failed or transmit-gated sends must not reach the observer; only the later real write may commit. */
    nextDeviceWriteResult = 0;
    TEST_ASSERT(CO_CANsend(&canModule, &tpdoTx) == CO_ERROR_TX_BUSY);
    TEST_ASSERT(runtime.device.analogLastCommunicated[0] == 7);
    TEST_ASSERT(first.source[0] == 0x06U);

    writesBefore = deviceWriteCount;
    CO_RTT_CANsetTxEnabled(&canModule, false);
    nextDeviceWriteResult = (rt_ssize_t)sizeof(struct rt_can_msg);
    TEST_ASSERT(CO_CANsend(&canModule, &tpdoTx) == CO_ERROR_NO);
    TEST_ASSERT(deviceWriteCount == writesBefore);
    TEST_ASSERT(runtime.device.analogLastCommunicated[0] == 7);
    TEST_ASSERT(first.source[0] == 0x06U);

    CO_RTT_CANsetTxEnabled(&canModule, true);
    TEST_ASSERT(CO_CANsend(&canModule, &tpdoTx) == CO_ERROR_NO);
    TEST_ASSERT(runtime.device.analogLastCommunicated[0] == 9);
    TEST_ASSERT(first.source[0] == 0x04U);

#if defined(CIA401_TEST_BITWISE_TPDO)
    /* A short 0x6401 mapping retires the transport retry without manufacturing a partial delta reference. */
    runtime.device.analogInputEventTpdoPending[0] = 0x01U;
    first.source[0] = 0x0101U;
    preparePartialTpdoFrame(&tpdo[0], &tpdoTx, 0xAAU, 0x01U);
    TEST_ASSERT(CO_CANsend(&canModule, &tpdoTx) == CO_ERROR_NO);
    TEST_ASSERT((runtime.device.analogInputEventTpdoPending[0] & 0x01U) == 0U);
    TEST_ASSERT(runtime.device.analogLastCommunicated[0] == 9);
    TEST_ASSERT(first.source[0] == 0x0100U);
    first.source[0] = 0x04U;
#endif /* CIA401_TEST_BITWISE_TPDO */

    capturedCallback = canModule.txSuccessCallback;
    capturedObject = canModule.txSuccessObject;

    for (unsigned tick = 0U; tick < 100U; tick++) {
        registeredOps->realtimeTick(&app, registeredContext);
    }
    TEST_ASSERT(semReleaseCount == 1U);
    TEST_ASSERT(runtime.cia401Sem.value == 1U);
    TEST_ASSERT(rt_atomic_load(&runtime.wakePending) == 1);
    TEST_ASSERT(runWorkerPass(&runtime));
    TEST_ASSERT(runtime.cia401Sem.value == 0U);
    TEST_ASSERT(rt_atomic_load(&runtime.wakePending) == 0);
    registeredOps->realtimeTick(&app, registeredContext);
    TEST_ASSERT(semReleaseCount == 2U);
    TEST_ASSERT(runtime.cia401Sem.value == 1U);

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    nmt.operatingState = CO_NMT_STOPPED;
    registeredOps->nmtStateChanged(&app, registeredContext, nmt.operatingState);
    TEST_ASSERT(runtime.nmtStoppedApplyPending == RT_TRUE);
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */

    registeredOps->communicationStop(&app, registeredContext);
    TEST_ASSERT(runtime.communicationReady == RT_FALSE);
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    TEST_ASSERT(runtime.nmtStoppedApplyPending == RT_FALSE);
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
    TEST_ASSERT(runtime.device.sdoReadMatch == NULL);
    TEST_ASSERT(canModule.txSuccessCallback == NULL);
    TEST_ASSERT(canModule.txSuccessObject == NULL);

    /* Clearing the observer allows later writes but must not commit CiA 401 communication state. */
    prepareTpdoFrame(&tpdo[0], &tpdoTx, 10U, 0x04U);
    writesBefore = deviceWriteCount;
    TEST_ASSERT(CO_CANsend(&canModule, &tpdoTx) == CO_ERROR_NO);
    TEST_ASSERT(deviceWriteCount == writesBefore + 1U);
    TEST_ASSERT(runtime.device.analogLastCommunicated[0] == 9);
    TEST_ASSERT(first.source[0] == 0x04U);

    /* A callback captured just before stop must observe the closed generation gate and leave state untouched. */
    prepareTpdoFrame(&tpdo[0], &tpdoTx, 11U, 0x04U);
    capturedCallback(capturedObject, &tpdoTx);
    TEST_ASSERT(runtime.device.analogLastCommunicated[0] == 9);
    TEST_ASSERT(first.source[0] == 0x04U);

    registeredOps->resetWakeups(&app, registeredContext);
    TEST_ASSERT(semResetCount == 1U);
    TEST_ASSERT(runtime.cia401Sem.value == 0U);
    TEST_ASSERT(rt_atomic_load(&runtime.wakePending) == 0);
    registeredOps->communicationQuiesced(&app, registeredContext);
    TEST_ASSERT(!runtime.device.odBound);
    TEST_ASSERT(runtime.device.od == NULL);
    TEST_ASSERT(runtime.device.bound.deviceType == NULL);
    TEST_ASSERT(runtime.device.bound.analogInput16 == NULL);
    TEST_ASSERT(runtime.device.bound.analogOutput16 == NULL);
    TEST_ASSERT(runtime.device.bound.analogInterruptSource == NULL);
    TEST_ASSERT(first.entries[1].extension == NULL);
    TEST_ASSERT(first.entries[2].extension == NULL);
    TEST_ASSERT(first.entries[4].extension == NULL);

    TEST_ASSERT(registeredOps->communicationBind(&app, &co, &second.od, registeredContext) == RT_EOK);
    sdoServer[0].OD = &second.od;
    TEST_ASSERT(runtime.device.odBound);
    TEST_ASSERT(second.entries[1].extension == &runtime.device.analogInput16Extension);
    TEST_ASSERT(second.entries[2].extension == &runtime.device.analogOutput16Extension);
    TEST_ASSERT(second.entries[4].extension == &runtime.device.analogInterruptSourceExtension);
    TEST_ASSERT(runtime.device.outputSupervisionReady);
    nmt.operatingState = CO_NMT_PRE_OPERATIONAL;
    registeredOps->communicationReady(&app, registeredContext);
    TEST_ASSERT(runtime.communicationReady == RT_TRUE);
    TEST_ASSERT(canModule.txSuccessCallback != NULL);
    TEST_ASSERT(canModule.txSuccessObject == &runtime);

    registeredOps->communicationStop(&app, registeredContext);
    registeredOps->communicationQuiesced(&app, registeredContext);
    registeredOps->runtimeDeinit(&app, registeredContext);
    TEST_ASSERT(runtime.communicationReady == RT_FALSE);
    TEST_ASSERT(runtime.semInitialized == RT_FALSE);
    TEST_ASSERT(runtime.workerThread == RT_NULL);
    TEST_ASSERT(runtime.deviceInitialized == RT_FALSE);
    TEST_ASSERT(threadDeleteCount == 1U);
    TEST_ASSERT(second.entries[1].extension == NULL);
    TEST_ASSERT(second.entries[2].extension == NULL);
    TEST_ASSERT(second.entries[4].extension == NULL);
    return true;
}

int main(void)
{
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    if (!test_manual_attach_output_fault_classifier_contract()) {
        return 1;
    }
    printf("CIA401_RTT_LIFECYCLE_CASE_PASS:manual-attach-fault-classifier-contract\n");
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE || PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_AUTOSTART)
    if (!test_autoattach_output_supervision_contract()) {
        return 1;
    }
    printf("CIA401_RTT_LIFECYCLE_CASE_PASS:autoattach-supervision-contract\n");
    if (!test_autoattach_supervision_is_visible_to_first_output_write()) {
        return 1;
    }
    printf("CIA401_RTT_LIFECYCLE_CASE_PASS:autoattach-supervision-ready-on-write\n");
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_AUTOSTART */
    if (!test_lifecycle_reset_rebind_and_deinit()) {
        return 1;
    }
#if defined(CIA401_TEST_BITWISE_TPDO)
    printf("CIA401_RTT_LIFECYCLE_CASE_PASS:bitwise-nonaligned-tpdo\n");
#else
    printf("CIA401_RTT_LIFECYCLE_CASE_PASS:reset-rebind-deinit\n");
#endif /* CIA401_TEST_BITWISE_TPDO */
#if defined(PKG_CANOPENNODE_CIA401_DEVICE_RTT_AUTOSTART)
    printf("CIA401_RTT_LIFECYCLE_PASS:4/4\n");
#else
    printf("CIA401_RTT_LIFECYCLE_PASS:2/2\n");
#endif /* PKG_CANOPENNODE_CIA401_DEVICE_RTT_AUTOSTART */
    return 0;
}
