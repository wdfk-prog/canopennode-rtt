/**
 * @file cia401-analog-stage345-host-test.c
 * @brief Host contract tests for CiA 401 analogue event, fail-safe and integer conditioning semantics.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define OD_DEFINITION
#include "CO_401_device.h"
#include "CO_401_analog.h"
#if defined(CIA401_TEST_TPDO)
#include "301/CO_PDO.h"
#endif

#define CH 2U
#define ENTRY_COUNT 21U
#ifndef CIA401_ANALOG_VARIANT
#define CIA401_ANALOG_VARIANT "all"
#endif
#define TEST_ASSERT(x) \
    do { \
        if (!(x)) { \
            fprintf(stderr, "CIA401_ANALOG_FAIL:%s:%d:%s\n", __func__, __LINE__, #x); \
            return false; \
        } \
    } while (0)

typedef struct {
    uint32_t deviceType;
#if defined(CIA401_TEST_TPDO)
    uint8_t tpdoCommSub0, tpdoTransmissionType, tpdoMapSub0;
    uint32_t tpdoCobId, tpdoMap1, tpdoMap2;
#endif
    uint8_t subAi, subAo, subTrigger, subSource, subUpper, subLower, subDelta, subNeg, subPos;
    uint8_t subErrMode, subErrValue, subAiUnit, subAiOffset, subAiScale;
    uint8_t subAoOffset, subAoScale, subAoUnit;
    uint8_t interruptEnable;
    int16_t ai[CH], ao[CH];
    uint8_t trigger[CH], errorMode[CH];
    uint32_t source[1], delta[CH], negDelta[CH], posDelta[CH];
    int32_t upper[CH], lower[CH], errorValue[CH], aiOffset[CH], aiScale[CH];
    int32_t aoOffset[CH], aoScale[CH];
    uint32_t aiUnit[CH], aoUnit[CH];
    OD_obj_var_t deviceTypeObj, interruptEnableObj;
    OD_obj_array_t aiObj, aoObj, triggerObj, sourceObj, upperObj, lowerObj, deltaObj, negObj, posObj;
    OD_obj_array_t errorModeObj, errorValueObj, aiUnitObj, aiOffsetObj, aiScaleObj;
    OD_obj_array_t aoOffsetObj, aoScaleObj, aoUnitObj;
#if defined(CIA401_TEST_TPDO)
    OD_obj_record_t tpdoCommObj[3], tpdoMapObj[3];
#endif
    OD_entry_t entries[ENTRY_COUNT];
    OD_t od;
} fixture_t;

typedef struct {
    int16_t ai[CH], ao[CH];
    unsigned reads, writes;
    bool supervised;
} io_t;

static CO_401_io_result_t readAnalog16(void *object, uint8_t channel, int16_t *value)
{
    io_t *io = object;
    io->reads++;
    if (channel >= CH || value == NULL) return CO_401_IO_ERROR;
    *value = io->ai[channel];
    return CO_401_IO_OK;
}

static CO_401_io_result_t writeAnalog16(void *object, uint8_t channel, int16_t value)
{
    io_t *io = object;
    io->writes++;
    if (channel >= CH) return CO_401_IO_ERROR;
    io->ao[channel] = value;
    return CO_401_IO_OK;
}

static const CO_401_io_if_t ioIf = {
    .readDigital8 = NULL,
    .writeDigital8 = NULL,
    .readAnalog16 = readAnalog16,
    .writeAnalog16 = writeAnalog16,
};

#if defined(CIA401_TEST_TPDO)
static bool outputSupervisionProbe(void *object)
{
    const io_t *io = object;

    return io != NULL && io->supervised;
}

static uint8_t transmittedData[CO_PDO_MAX_SIZE];
static uint8_t transmittedDlc;
static unsigned transmittedCount;
static unsigned reportedErrors;
static CO_ReturnError_t nextSendResult = CO_ERROR_NO;
CO_CANtx_t *CO_CANtxBufferInit(CO_CANmodule_t *CANmodule, uint16_t index, uint16_t ident, bool_t rtr,
                               uint8_t noOfBytes, bool_t syncFlag)
{
    CO_CANtx_t *buffer;

    if (CANmodule == NULL || CANmodule->txArray == NULL || index >= CANmodule->txSize) {
        return NULL;
    }
    buffer = &CANmodule->txArray[index];
    buffer->ident = ident;
    buffer->DLC = noOfBytes;
    buffer->bufferFull = false;
    buffer->syncFlag = syncFlag;
    (void)rtr;
    return buffer;
}

CO_ReturnError_t CO_CANrxBufferInit(CO_CANmodule_t *CANmodule, uint16_t index, uint16_t ident, uint16_t mask,
                                      bool_t rtr, void *object,
                                      void (*CANrx_callback)(void *object, void *message))
{
    (void)CANmodule;
    (void)index;
    (void)ident;
    (void)mask;
    (void)rtr;
    (void)object;
    (void)CANrx_callback;
    return CO_ERROR_NO;
}

CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer)
{
    CO_ReturnError_t result;

    if (CANmodule == NULL || buffer == NULL || buffer->DLC > CO_PDO_MAX_SIZE) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }
    result = nextSendResult;
    nextSendResult = CO_ERROR_NO;
    if (result != CO_ERROR_NO) {
        return result;
    }

    transmittedDlc = buffer->DLC;
    (void)memcpy(transmittedData, buffer->data, transmittedDlc);
    transmittedCount++;
    return CO_ERROR_NO;
}

void CO_error(CO_EM_t *em, bool_t setError, const uint8_t errorBit, uint16_t errorCode, uint32_t infoCode)
{
    if (setError) {
        reportedErrors++;
    }
    (void)em;
    (void)errorBit;
    (void)errorCode;
    (void)infoCode;
}
#endif /* CIA401_TEST_TPDO */

static void initArray(OD_obj_array_t *obj, uint8_t *sub0, void *data, OD_size_t size, OD_attr_t attr)
{
    obj->dataOrig0 = sub0;
    obj->dataOrig = data;
    obj->attribute0 = ODA_SDO_R;
    obj->attribute = attr;
    obj->dataElementLength = size;
    obj->dataElementSizeof = size;
}

#if defined(CIA401_TEST_TPDO)
static void initRecord(OD_obj_record_t *obj, void *data, uint8_t subIndex, OD_size_t size, OD_attr_t attr)
{
    obj->dataOrig = data;
    obj->subIndex = subIndex;
    obj->attribute = attr;
    obj->dataLength = size;
}
#endif /* CIA401_TEST_TPDO */

static void addEntry(fixture_t *f, uint16_t index, uint8_t count, uint8_t type, const void *obj)
{
    OD_entry_t *e = &f->entries[f->od.size++];
    e->index = index;
    e->subEntriesCount = count;
    e->odObjectType = type;
    e->odObject = obj;
    e->extension = NULL;
}

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING)
static bool removeEntry(fixture_t *f, uint16_t index)
{
    OD_size_t i;

    for (i = 0U; i < f->od.size; i++) {
        if (f->entries[i].index == index) {
            if (i + 1U < f->od.size) {
                (void)memmove(&f->entries[i], &f->entries[i + 1U],
                              (f->od.size - i - 1U) * sizeof(f->entries[0]));
            }
            f->od.size--;
            return true;
        }
    }
    return false;
}
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING */

static void fixtureInit(fixture_t *f)
{
    memset(f, 0, sizeof(*f));
    f->deviceType = CO_401_deviceTypeForCapabilities(CO_401_CAP_ANALOG_INPUT | CO_401_CAP_ANALOG_OUTPUT);
#if defined(CIA401_TEST_TPDO)
    f->tpdoCommSub0 = 2U;
    f->tpdoTransmissionType = 254U;
    f->tpdoCobId = 0x181U;
    f->tpdoMapSub0 = 2U;
    f->tpdoMap1 = ((uint32_t)CO_401_INDEX_ANALOG_INPUT_16 << 16) | (1UL << 8) | 16U;
    f->tpdoMap2 = ((uint32_t)CO_401_INDEX_ANALOG_INTERRUPT_SOURCE << 16) | (1UL << 8) | 32U;
#endif
    f->subAi = f->subAo = f->subTrigger = f->subUpper = f->subLower = f->subDelta = f->subNeg = f->subPos = CH;
    f->subErrMode = f->subErrValue = f->subAiUnit = f->subAiOffset = f->subAiScale = CH;
    f->subAoOffset = f->subAoScale = f->subAoUnit = CH;
    f->subSource = 1U;
    f->errorMode[0] = f->errorMode[1] = 1U;
    f->aiScale[0] = f->aiScale[1] = 1;
    f->aoScale[0] = f->aoScale[1] = 1;

    f->deviceTypeObj.dataOrig = &f->deviceType;
    f->deviceTypeObj.attribute = ODA_SDO_R | ODA_MB;
    f->deviceTypeObj.dataLength = 4U;
    f->interruptEnableObj.dataOrig = &f->interruptEnable;
    f->interruptEnableObj.attribute = ODA_SDO_RW;
    f->interruptEnableObj.dataLength = 1U;
    initArray(&f->aiObj, &f->subAi, f->ai, 2U, ODA_SDO_R | ODA_TPDO | ODA_MB);
    initArray(&f->aoObj, &f->subAo, f->ao, 2U, ODA_SDO_RW | ODA_RPDO | ODA_MB);
    initArray(&f->triggerObj, &f->subTrigger, f->trigger, 1U, ODA_SDO_RW);
    initArray(&f->sourceObj, &f->subSource, f->source, 4U, ODA_SDO_R | ODA_TPDO | ODA_MB);
    initArray(&f->upperObj, &f->subUpper, f->upper, 4U, ODA_SDO_RW | ODA_MB);
    initArray(&f->lowerObj, &f->subLower, f->lower, 4U, ODA_SDO_RW | ODA_MB);
    initArray(&f->deltaObj, &f->subDelta, f->delta, 4U, ODA_SDO_RW | ODA_MB);
    initArray(&f->negObj, &f->subNeg, f->negDelta, 4U, ODA_SDO_RW | ODA_MB);
    initArray(&f->posObj, &f->subPos, f->posDelta, 4U, ODA_SDO_RW | ODA_MB);
    initArray(&f->errorModeObj, &f->subErrMode, f->errorMode, 1U, ODA_SDO_RW);
    initArray(&f->errorValueObj, &f->subErrValue, f->errorValue, 4U, ODA_SDO_RW | ODA_MB);
    initArray(&f->aiUnitObj, &f->subAiUnit, f->aiUnit, 4U, ODA_SDO_RW | ODA_MB);
    initArray(&f->aiOffsetObj, &f->subAiOffset, f->aiOffset, 4U, ODA_SDO_RW | ODA_MB);
    initArray(&f->aiScaleObj, &f->subAiScale, f->aiScale, 4U, ODA_SDO_RW | ODA_MB);
    initArray(&f->aoOffsetObj, &f->subAoOffset, f->aoOffset, 4U, ODA_SDO_RW | ODA_MB);
    initArray(&f->aoScaleObj, &f->subAoScale, f->aoScale, 4U, ODA_SDO_RW | ODA_MB);
    initArray(&f->aoUnitObj, &f->subAoUnit, f->aoUnit, 4U, ODA_SDO_RW | ODA_MB);
#if defined(CIA401_TEST_TPDO)
    initRecord(&f->tpdoCommObj[0], &f->tpdoCommSub0, 0U, 1U, ODA_SDO_R);
    initRecord(&f->tpdoCommObj[1], &f->tpdoCobId, 1U, 4U, ODA_SDO_RW | ODA_MB);
    initRecord(&f->tpdoCommObj[2], &f->tpdoTransmissionType, 2U, 1U, ODA_SDO_RW);
    initRecord(&f->tpdoMapObj[0], &f->tpdoMapSub0, 0U, 1U, ODA_SDO_RW);
    initRecord(&f->tpdoMapObj[1], &f->tpdoMap1, 1U, 4U, ODA_SDO_RW | ODA_MB);
    initRecord(&f->tpdoMapObj[2], &f->tpdoMap2, 2U, 4U, ODA_SDO_RW | ODA_MB);
#endif
    f->od.list = f->entries;
    addEntry(f, CO_401_INDEX_DEVICE_TYPE, 1U, ODT_VAR, &f->deviceTypeObj);
#if defined(CIA401_TEST_TPDO)
    addEntry(f, OD_H1800_TXPDO_1_PARAM, 3U, ODT_REC, f->tpdoCommObj);
    addEntry(f, OD_H1A00_TXPDO_1_MAPPING, 3U, ODT_REC, f->tpdoMapObj);
#endif
    addEntry(f, CO_401_INDEX_ANALOG_INPUT_16, CH + 1U, ODT_ARR, &f->aiObj);
    addEntry(f, CO_401_INDEX_ANALOG_OUTPUT_16, CH + 1U, ODT_ARR, &f->aoObj);
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
    addEntry(f, CO_401_INDEX_ANALOG_INTERRUPT_TRIGGER, CH + 1U, ODT_ARR, &f->triggerObj);
    addEntry(f, CO_401_INDEX_ANALOG_INTERRUPT_SOURCE, 2U, ODT_ARR, &f->sourceObj);
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */
    addEntry(f, CO_401_INDEX_ANALOG_INTERRUPT_ENABLE, 1U, ODT_VAR, &f->interruptEnableObj);
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
    addEntry(f, CO_401_INDEX_ANALOG_INTERRUPT_UPPER_32, CH + 1U, ODT_ARR, &f->upperObj);
    addEntry(f, CO_401_INDEX_ANALOG_INTERRUPT_LOWER_32, CH + 1U, ODT_ARR, &f->lowerObj);
    addEntry(f, CO_401_INDEX_ANALOG_INTERRUPT_DELTA_U32, CH + 1U, ODT_ARR, &f->deltaObj);
    addEntry(f, CO_401_INDEX_ANALOG_INTERRUPT_NEG_DELTA_U32, CH + 1U, ODT_ARR, &f->negObj);
    addEntry(f, CO_401_INDEX_ANALOG_INTERRUPT_POS_DELTA_U32, CH + 1U, ODT_ARR, &f->posObj);
#endif
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING)
    addEntry(f, CO_401_INDEX_ANALOG_INPUT_SI_UNIT, CH + 1U, ODT_ARR, &f->aiUnitObj);
    addEntry(f, CO_401_INDEX_ANALOG_INPUT_OFFSET_32, CH + 1U, ODT_ARR, &f->aiOffsetObj);
    addEntry(f, CO_401_INDEX_ANALOG_INPUT_PRESCALING_32, CH + 1U, ODT_ARR, &f->aiScaleObj);
#endif
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    addEntry(f, CO_401_INDEX_ANALOG_OUTPUT_ERROR_MODE, CH + 1U, ODT_ARR, &f->errorModeObj);
    addEntry(f, CO_401_INDEX_ANALOG_OUTPUT_ERROR_VALUE_32, CH + 1U, ODT_ARR, &f->errorValueObj);
#endif
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING)
    addEntry(f, CO_401_INDEX_ANALOG_OUTPUT_OFFSET_32, CH + 1U, ODT_ARR, &f->aoOffsetObj);
    addEntry(f, CO_401_INDEX_ANALOG_OUTPUT_SCALING_32, CH + 1U, ODT_ARR, &f->aoScaleObj);
    addEntry(f, CO_401_INDEX_ANALOG_OUTPUT_SI_UNIT, CH + 1U, ODT_ARR, &f->aoUnitObj);
#endif
}

static bool makeDevice(fixture_t *f, io_t *io, CO_401_device_t *d)
{
    CO_401_device_config_t c = {
        .io = &ioIf,
        .ioObject = io,
        .analogInputChannels = CH,
        .analogOutputChannels = CH,
    };
    CO_401_init_diag_t diag;
    CO_401_init_error_t r = CO_401_device_init(d, &f->od, &c, &diag);
    if (r != CO_401_INIT_OK) {
        fprintf(stderr, "bind err=%d index=0x%04x sub=%u\n", (int)r, diag.index, diag.subIndex);
    } else {
        CO_401_device_notifyOutputSupervision(d);
    }
    return r == CO_401_INIT_OK;
}

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
static bool sourceAndCommit(CO_401_device_t *device, uint32_t expected)
{
    uint32_t communicated = 0U, after = UINT32_MAX;

    TEST_ASSERT(OD_get_u32(device->bound.analogInterruptSource, 1U, &communicated, false) == ODR_OK);
    TEST_ASSERT(communicated == expected);
    CO_401_device_commitAnalogSourceCommunication(device, 1U, communicated);
    TEST_ASSERT(OD_get_u32(device->bound.analogInterruptSource, 1U, &after, true) == ODR_OK);
    TEST_ASSERT(after == 0U);
    return true;
}

static bool commitCommunicatedInput(CO_401_device_t *device)
{
    int16_t communicated = 0;

    TEST_ASSERT(OD_get_i16(device->bound.analogInput16, 1U, &communicated, false) == ODR_OK);
    CO_401_device_commitAnalogInputCommunication(device, 1U, communicated);
    return true;
}

static bool test_local_read_does_not_consume_network_state(void)
{
    fixture_t f;
    io_t io = {0};
    CO_401_device_t d;
    uint32_t source = 0U, sourceAfter = 0U;
    int16_t input = 0;

    fixtureInit(&f);
    TEST_ASSERT(makeDevice(&f, &io, &d));
    f.interruptEnable = 1U;
    f.trigger[0] = 0x01U;
    f.upper[0] = 1 << 16;
    io.ai[0] = 1;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(f.source[0] == 1U);
    TEST_ASSERT(!d.analogLastCommunicatedValid[0]);

    TEST_ASSERT(OD_get_u32(d.bound.analogInterruptSource, 1U, &source, false) == ODR_OK);
    TEST_ASSERT(source == 1U);
    TEST_ASSERT(OD_get_u32(d.bound.analogInterruptSource, 1U, &sourceAfter, true) == ODR_OK);
    TEST_ASSERT(sourceAfter == 1U);
    TEST_ASSERT(OD_get_i16(d.bound.analogInput16, 1U, &input, false) == ODR_OK);
    TEST_ASSERT(input == 1);
    TEST_ASSERT(!d.analogLastCommunicatedValid[0]);

    CO_401_device_commitAnalogInputCommunication(&d, 1U, input);
    TEST_ASSERT(d.analogLastCommunicatedValid[0]);
    TEST_ASSERT(d.analogLastCommunicated[0] == 1);
    CO_401_device_commitAnalogSourceCommunication(&d, 1U, source);
    TEST_ASSERT(OD_get_u32(d.bound.analogInterruptSource, 1U, &sourceAfter, true) == ODR_OK);
    TEST_ASSERT(sourceAfter == 0U);
    return true;
}

static bool test_source_commit_preserves_later_bits(void)
{
    fixture_t f;
    io_t io = {0};
    CO_401_device_t d;
    uint32_t source = 0U;

    fixtureInit(&f);
    TEST_ASSERT(makeDevice(&f, &io, &d));
    f.source[0] = 0x03U;

    /* A successful transfer of bit 0 must not erase bit 1 latched after its snapshot. */
    CO_401_device_commitAnalogSourceCommunication(&d, 1U, 0x01U);
    TEST_ASSERT(OD_get_u32(d.bound.analogInterruptSource, 1U, &source, true) == ODR_OK);
    TEST_ASSERT(source == 0x02U);
    return true;
}

static bool test_zero_trigger_uses_unconditional_change_event(void)
{
    fixture_t f;
    io_t io = {0};
    CO_401_device_t d;

    fixtureInit(&f);
    TEST_ASSERT(makeDevice(&f, &io, &d));
    f.interruptEnable = 1U;
    f.trigger[0] = 0U;

    io.ai[0] = 1;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(f.source[0] == 1U);
    TEST_ASSERT((d.analogInputEventTpdoPending[0] & 0x01U) != 0U);
    TEST_ASSERT(sourceAndCommit(&d, 1U));
    CO_401_device_commitAnalogInputTpdoCommunication(&d, 1U, 1);
    TEST_ASSERT((d.analogInputEventTpdoPending[0] & 0x01U) == 0U);

    /* An unchanged process value must not manufacture another unconditional event. */
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(sourceAndCommit(&d, 0U));
    TEST_ASSERT((d.analogInputEventTpdoPending[0] & 0x01U) == 0U);
    return true;
}

static bool test_limits_and_source_clear(void)
{
    fixture_t f;
    io_t io = {0};
    CO_401_device_t d;

    fixtureInit(&f);
    TEST_ASSERT(makeDevice(&f, &io, &d));
    f.interruptEnable = 1U;
    f.trigger[0] = 0x01U;
    f.upper[0] = 10 << 16;
    io.ai[0] = 10;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(sourceAndCommit(&d, 1U));

    f.trigger[0] = 0x02U;
    f.lower[0] = 10 << 16;
    io.ai[0] = 11;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(sourceAndCommit(&d, 0U));
    io.ai[0] = 10;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(sourceAndCommit(&d, 0U));
    io.ai[0] = 9;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(sourceAndCommit(&d, 1U));
    return true;
}

static bool test_delta_requires_network_communication(void)
{
    fixture_t f;
    io_t io = {0};
    CO_401_device_t d;

    fixtureInit(&f);
    TEST_ASSERT(makeDevice(&f, &io, &d));
    f.interruptEnable = 1U;
    f.trigger[0] = 0x10U;
    f.posDelta[0] = 0U;

    /* Local refreshes alone never create the CiA 401 "last communicated value" reference. */
    io.ai[0] = 0;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(sourceAndCommit(&d, 0U));
    TEST_ASSERT(!d.analogLastCommunicatedValid[0]);

    io.ai[0] = 2;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(sourceAndCommit(&d, 0U));
    TEST_ASSERT(!d.analogLastCommunicatedValid[0]);

    TEST_ASSERT(commitCommunicatedInput(&d));
    TEST_ASSERT(d.analogLastCommunicatedValid[0]);
    TEST_ASSERT(d.analogLastCommunicated[0] == 2);

    io.ai[0] = 3;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(sourceAndCommit(&d, 1U));
    return true;
}

static bool test_delta_and_or(void)
{
    fixture_t f;
    io_t io = {0};
    CO_401_device_t d;

    fixtureInit(&f);
    TEST_ASSERT(makeDevice(&f, &io, &d));
    f.interruptEnable = 1U;
    f.trigger[0] = 0x10U;
    f.posDelta[0] = 1U << 16;
    io.ai[0] = 0;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(sourceAndCommit(&d, 0U));
    TEST_ASSERT(commitCommunicatedInput(&d));

    io.ai[0] = 1;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(sourceAndCommit(&d, 0U));
    io.ai[0] = 2;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(sourceAndCommit(&d, 1U));
    TEST_ASSERT(commitCommunicatedInput(&d));

    f.trigger[0] = 0x08U;
    f.negDelta[0] = 1U << 16;
    io.ai[0] = 0;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(sourceAndCommit(&d, 1U));
    TEST_ASSERT(commitCommunicatedInput(&d));

    f.trigger[0] = 0x11U;
    f.upper[0] = 100 << 16;
    f.posDelta[0] = 0U;
    io.ai[0] = 1;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(sourceAndCommit(&d, 1U));
    return true;
}

static bool test_global_disable_and_operational_warning(void)
{
    fixture_t f;
    io_t io = {0};
    CO_401_error_event_t event;
    CO_401_device_t d;

    fixtureInit(&f);
    TEST_ASSERT(makeDevice(&f, &io, &d));
    f.trigger[0] = 0x01U;
    f.upper[0] = 0;
    io.ai[0] = 10;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(sourceAndCommit(&d, 0U));

    CO_401_device_setNmtOperational(&d, true);
    TEST_ASSERT(CO_401_device_takeErrorEvent(&d, &event));
    TEST_ASSERT(event.errorCode == 0x0080U);
    TEST_ASSERT(!CO_401_device_takeErrorEvent(&d, &event));
    CO_401_device_setNmtOperational(&d, true);
    TEST_ASSERT(!CO_401_device_takeErrorEvent(&d, &event));
    CO_401_device_setNmtOperational(&d, false);
    CO_401_device_setNmtOperational(&d, true);
    TEST_ASSERT(CO_401_device_takeErrorEvent(&d, &event));
    return true;
}

static bool test_communication_reset_rearms_operational_warning(void)
{
    fixture_t f;
    io_t io = {0};
    CO_401_error_event_t event;
    CO_401_device_t d;

    fixtureInit(&f);
    TEST_ASSERT(makeDevice(&f, &io, &d));

    CO_401_device_setNmtOperational(&d, true);
    TEST_ASSERT(CO_401_device_takeErrorEvent(&d, &event));
    TEST_ASSERT(event.errorCode == 0x0080U);

    CO_401_device_setNmtOperational(&d, false);
    CO_401_device_setNmtOperational(&d, true);
    CO_401_device_resetCommunicationState(&d);
    TEST_ASSERT(!CO_401_device_takeErrorEvent(&d, &event));

    CO_401_device_setNmtOperational(&d, true);
    TEST_ASSERT(CO_401_device_takeErrorEvent(&d, &event));
    TEST_ASSERT(event.errorCode == 0x0080U);
    return true;
}

#if defined(CIA401_TEST_TPDO)
static bool test_source_clear_on_tpdo_transmit(void)
{
    fixture_t f;
    io_t io = {0};
    CO_401_device_t d;
    CO_TPDO_t tpdo;
    CO_CANmodule_t canModule = {0};
    CO_CANtx_t txArray[1] = {{0}};
    CO_EM_t em = {0};
    uint32_t errInfo = 0U;

    fixtureInit(&f);
    TEST_ASSERT(makeDevice(&f, &io, &d));
    canModule.txArray = txArray;
    canModule.txSize = 1U;
    transmittedCount = 0U;
    transmittedDlc = 0U;
    reportedErrors = 0U;
    (void)memset(transmittedData, 0, sizeof(transmittedData));

    TEST_ASSERT(CO_TPDO_init(&tpdo, &f.od, &em, 0x181U,
                             OD_find(&f.od, OD_H1800_TXPDO_1_PARAM),
                             OD_find(&f.od, OD_H1A00_TXPDO_1_MAPPING),
                             &canModule, 0U, &errInfo) == CO_ERROR_NO);
    TEST_ASSERT(reportedErrors == 0U);

    /* Ignore CO_TPDO_init()'s initial request; the analogue event below must request this mapped TPDO. */
    tpdo.sendRequest = false;
    f.interruptEnable = 1U;
    f.trigger[0] = 0x01U;
    f.upper[0] = 1 << 16;
    io.ai[0] = 1;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(f.source[0] == 1U);
    TEST_ASSERT(!d.analogLastCommunicatedValid[0]);

    nextSendResult = CO_ERROR_TX_OVERFLOW;
    CO_TPDO_process(&tpdo, true, false);
    TEST_ASSERT(transmittedCount == 0U);
    TEST_ASSERT(f.source[0] == 1U);
    TEST_ASSERT(!d.analogLastCommunicatedValid[0]);

    /* The unchanged input must re-arm the event TPDO while the failed event remains pending. */
    CO_401_analog_refreshInputs(&d);
    CO_TPDO_process(&tpdo, true, false);
    TEST_ASSERT(transmittedCount == 1U);
    TEST_ASSERT(transmittedDlc == 6U);
    TEST_ASSERT(CO_getUint16(&transmittedData[0]) == 1U);
    TEST_ASSERT(CO_getUint32(&transmittedData[2]) == 1U);
    TEST_ASSERT(f.source[0] == 1U);
    TEST_ASSERT(!d.analogLastCommunicatedValid[0]);

    /* The transport adapter commits exactly the payload only after this successful send. */
    CO_401_device_commitAnalogInputTpdoCommunication(&d, 1U, (int16_t)CO_getUint16(&transmittedData[0]));
    CO_401_device_commitAnalogSourceCommunication(&d, 1U, CO_getUint32(&transmittedData[2]));
    TEST_ASSERT(f.source[0] == 0U);
    TEST_ASSERT(d.analogLastCommunicatedValid[0]);
    TEST_ASSERT(d.analogLastCommunicated[0] == 1);

    io.ai[0] = 2;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(f.source[0] == 1U);
    CO_TPDO_process(&tpdo, true, false);
    TEST_ASSERT(transmittedCount == 2U);
    TEST_ASSERT(CO_getUint16(&transmittedData[0]) == 2U);
    TEST_ASSERT(CO_getUint32(&transmittedData[2]) == 1U);
    TEST_ASSERT(f.source[0] == 1U);
    TEST_ASSERT(d.analogLastCommunicated[0] == 1);
    CO_401_device_commitAnalogInputTpdoCommunication(&d, 1U, (int16_t)CO_getUint16(&transmittedData[0]));
    CO_401_device_commitAnalogSourceCommunication(&d, 1U, CO_getUint32(&transmittedData[2]));
    TEST_ASSERT(f.source[0] == 0U);
    TEST_ASSERT(d.analogLastCommunicated[0] == 2);
    return true;
}

static bool test_source_only_tpdo_retries_after_send_failure(void)
{
    fixture_t f;
    io_t io = {0};
    CO_401_device_t d;
    CO_TPDO_t tpdo;
    CO_CANmodule_t canModule = {0};
    CO_CANtx_t txArray[1] = {{0}};
    CO_EM_t em = {0};
    uint32_t errInfo = 0U;

    fixtureInit(&f);
    f.tpdoMapSub0 = 1U;
    f.tpdoMap1 = ((uint32_t)CO_401_INDEX_ANALOG_INTERRUPT_SOURCE << 16) | (1UL << 8) | 32U;
    TEST_ASSERT(makeDevice(&f, &io, &d));
    canModule.txArray = txArray;
    canModule.txSize = 1U;
    transmittedCount = 0U;
    transmittedDlc = 0U;
    reportedErrors = 0U;
    (void)memset(transmittedData, 0, sizeof(transmittedData));

    TEST_ASSERT(CO_TPDO_init(&tpdo, &f.od, &em, 0x181U,
                             OD_find(&f.od, OD_H1800_TXPDO_1_PARAM),
                             OD_find(&f.od, OD_H1A00_TXPDO_1_MAPPING),
                             &canModule, 0U, &errInfo) == CO_ERROR_NO);
    TEST_ASSERT(reportedErrors == 0U);

    tpdo.sendRequest = false;
    f.interruptEnable = 1U;
    f.trigger[0] = 0x01U;
    f.upper[0] = 1 << 16;
    io.ai[0] = 1;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(f.source[0] == 1U);
    TEST_ASSERT(!d.analogLastCommunicatedValid[0]);

    nextSendResult = CO_ERROR_TX_OVERFLOW;
    CO_TPDO_process(&tpdo, true, false);
    TEST_ASSERT(transmittedCount == 0U);
    TEST_ASSERT(f.source[0] == 1U);

    CO_401_analog_refreshInputs(&d);
    CO_TPDO_process(&tpdo, true, false);
    TEST_ASSERT(transmittedCount == 1U);
    TEST_ASSERT(transmittedDlc == 4U);
    TEST_ASSERT(CO_getUint32(&transmittedData[0]) == 1U);
    TEST_ASSERT(!d.analogLastCommunicatedValid[0]);

    CO_401_device_commitAnalogSourceCommunication(&d, 1U, CO_getUint32(&transmittedData[0]));
    TEST_ASSERT(f.source[0] == 0U);
    CO_401_analog_refreshInputs(&d);
    CO_TPDO_process(&tpdo, true, false);
    TEST_ASSERT(transmittedCount == 1U);
    return true;
}

static bool test_input_only_tpdo_retries_once_after_send_failure(void)
{
    fixture_t f;
    io_t io = {0};
    CO_401_device_t d;
    CO_TPDO_t tpdo;
    CO_CANmodule_t canModule = {0};
    CO_CANtx_t txArray[1] = {{0}};
    CO_EM_t em = {0};
    uint32_t errInfo = 0U;

    fixtureInit(&f);
    f.tpdoMapSub0 = 1U;
    f.tpdoMap1 = ((uint32_t)CO_401_INDEX_ANALOG_INPUT_16 << 16) | (1UL << 8) | 16U;
    TEST_ASSERT(makeDevice(&f, &io, &d));
    canModule.txArray = txArray;
    canModule.txSize = 1U;
    transmittedCount = 0U;
    transmittedDlc = 0U;
    reportedErrors = 0U;
    (void)memset(transmittedData, 0, sizeof(transmittedData));

    TEST_ASSERT(CO_TPDO_init(&tpdo, &f.od, &em, 0x181U,
                             OD_find(&f.od, OD_H1800_TXPDO_1_PARAM),
                             OD_find(&f.od, OD_H1A00_TXPDO_1_MAPPING),
                             &canModule, 0U, &errInfo) == CO_ERROR_NO);
    TEST_ASSERT(reportedErrors == 0U);

    tpdo.sendRequest = false;
    f.interruptEnable = 1U;
    f.trigger[0] = 0x01U;
    f.upper[0] = 1 << 16;
    io.ai[0] = 1;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(f.source[0] == 1U);

    nextSendResult = CO_ERROR_TX_OVERFLOW;
    CO_TPDO_process(&tpdo, true, false);
    TEST_ASSERT(transmittedCount == 0U);

    CO_401_analog_refreshInputs(&d);
    CO_TPDO_process(&tpdo, true, false);
    TEST_ASSERT(transmittedCount == 1U);
    TEST_ASSERT(transmittedDlc == 2U);
    TEST_ASSERT(CO_getUint16(&transmittedData[0]) == 1U);
    CO_401_device_commitAnalogInputTpdoCommunication(&d, 1U, (int16_t)CO_getUint16(&transmittedData[0]));
    TEST_ASSERT(f.source[0] == 1U);

    CO_401_analog_refreshInputs(&d);
    CO_TPDO_process(&tpdo, true, false);
    TEST_ASSERT(transmittedCount == 1U);
    CO_401_device_commitAnalogSourceCommunication(&d, 1U, 1U);
    TEST_ASSERT(f.source[0] == 0U);
    return true;
}

static bool test_output_supervision_gate_on_sdo_and_rpdo_paths(void)
{
    fixture_t f;
    io_t io = {0};
    CO_401_device_t d;
    CO_401_device_config_t c = {
        .io = &ioIf,
        .ioObject = &io,
        .analogInputChannels = CH,
        .analogOutputChannels = CH,
    };
    CO_401_init_diag_t diag;
    OD_IO_t sdoIo;
    CO_RPDO_t rpdo = {0};
    OD_size_t countWritten;
    int16_t sdoValue = 11;

    fixtureInit(&f);
    TEST_ASSERT(CO_401_device_init(&d, &f.od, &c, &diag) == CO_401_INIT_OK);
    CO_401_device_setOutputSupervisionProbe(&d, &io, outputSupervisionProbe);
    TEST_ASSERT(!d.outputSupervisionReady);

    TEST_ASSERT(OD_getSub(d.bound.analogOutput16, 1U, &sdoIo, false) == ODR_OK);
    countWritten = 99U;
    TEST_ASSERT(sdoIo.write(&sdoIo.stream, &sdoValue, sizeof(sdoValue), &countWritten) == ODR_DATA_DEV_STATE);
    TEST_ASSERT(countWritten == 0U);
    TEST_ASSERT(f.ao[0] == 0);

    TEST_ASSERT(OD_getSub(d.bound.analogOutput16, 1U, &rpdo.PDO_common.OD_IO[0], false) == ODR_OK);
    rpdo.PDO_common.valid = true;
    rpdo.PDO_common.dataLength = 2U;
    rpdo.PDO_common.mappedObjectsCount = 1U;
    rpdo.PDO_common.OD_IO[0].stream.dataOffset = 2U;
    CO_setUint16(&rpdo.CANrxData[0][0], 22U);
    CO_FLAG_SET(rpdo.CANrxNew[0]);
    CO_RPDO_process(&rpdo, true, false);
    TEST_ASSERT(f.ao[0] == 0);

    /* The qualifying event is already visible to the product, but no profile worker/notifier has run. */
    io.supervised = true;
    TEST_ASSERT(!d.outputSupervisionReady);

    /* RPDO may be the first network write after supervision and must open the latch synchronously. */
    CO_setUint16(&rpdo.CANrxData[0][0], 22U);
    CO_FLAG_SET(rpdo.CANrxNew[0]);
    CO_RPDO_process(&rpdo, true, false);
    TEST_ASSERT(d.outputSupervisionReady);
    TEST_ASSERT(f.ao[0] == 22);

    sdoIo.stream.dataOffset = 0U;
    countWritten = 0U;
    TEST_ASSERT(sdoIo.write(&sdoIo.stream, &sdoValue, sizeof(sdoValue), &countWritten) == ODR_OK);
    TEST_ASSERT(countWritten == sizeof(sdoValue));
    TEST_ASSERT(f.ao[0] == 11);
    CO_401_analog_applyOutputs(&d);
    TEST_ASSERT(io.ao[0] == 11);
    return true;
}
#endif /* CIA401_TEST_TPDO */
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
static bool test_output_fault_bypasses_conditioning(void)
{
    fixture_t f;
    io_t io = {0};
    CO_401_device_t d;

    fixtureInit(&f);
    TEST_ASSERT(makeDevice(&f, &io, &d));
    f.ao[0] = 3;
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING)
    f.aoOffset[0] = 2;
    f.aoScale[0] = 2;
#endif
    CO_401_analog_applyOutputs(&d);
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING)
    TEST_ASSERT(io.ao[0] == 8);
#else
    TEST_ASSERT(io.ao[0] == 3);
#endif

    f.errorMode[0] = 1U;
    f.errorValue[0] = 7 << 16;
    CO_401_device_setAnalogOutputFault(&d, true);
    CO_401_analog_applyOutputs(&d);
    TEST_ASSERT(io.ao[0] == 7);

    f.errorMode[0] = 0U;
    io.ao[0] = 44;
    CO_401_analog_applyOutputs(&d);
    TEST_ASSERT(io.ao[0] == 44);
    return true;
}

static bool test_output_fault_sources_are_ored(void)
{
    fixture_t f;
    io_t io = {0};
    CO_401_device_t d;

    fixtureInit(&f);
    TEST_ASSERT(makeDevice(&f, &io, &d));
    f.ao[0] = 3;
    f.errorMode[0] = 1U;
    f.errorValue[0] = 7 << 16;

    CO_401_device_setAnalogOutputFault(&d, true);
    CO_401_device_setNmtStopped(&d, false);
    CO_401_analog_applyOutputs(&d);
    TEST_ASSERT(io.ao[0] == 7);

    CO_401_device_setAnalogOutputFault(&d, false);
    CO_401_device_setNmtStopped(&d, true);
    CO_401_analog_applyOutputs(&d);
    TEST_ASSERT(io.ao[0] == 7);

    CO_401_device_setAnalogOutputFault(&d, true);
    CO_401_device_setNmtStopped(&d, true);
    CO_401_analog_applyOutputs(&d);
    TEST_ASSERT(io.ao[0] == 7);

    CO_401_device_setNmtStopped(&d, false);
    CO_401_analog_applyOutputs(&d);
    TEST_ASSERT(io.ao[0] == 7);

    CO_401_device_setAnalogOutputFault(&d, false);
    CO_401_device_setCommunicationFault(&d, true);
    CO_401_analog_applyOutputs(&d);
    TEST_ASSERT(io.ao[0] == 7);

    CO_401_device_setAnalogOutputFault(&d, true);
    CO_401_device_setCommunicationFault(&d, false);
    CO_401_analog_applyOutputs(&d);
    TEST_ASSERT(io.ao[0] == 7);

    CO_401_device_setAnalogOutputFault(&d, false);
    CO_401_analog_applyOutputs(&d);
    TEST_ASSERT(io.ao[0] == 3);
    return true;
}
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING)
static bool test_conditioning_applies_scaling_before_offset(void)
{
    fixture_t f;
    io_t io = {0};
    CO_401_device_t d;

    fixtureInit(&f);
    TEST_ASSERT(makeDevice(&f, &io, &d));

    /* Non-commutative vectors pin the CiA 401 scale-then-offset contract on both directions. */
    f.aiOffset[0] = 2;
    f.aiScale[0] = 2;
    io.ai[0] = 3;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(f.ai[0] == 8);

    f.aiOffset[0] = -2;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(f.ai[0] == 4);

    f.ao[0] = 3;
    f.aoOffset[0] = 2;
    f.aoScale[0] = 2;
    CO_401_analog_applyOutputs(&d);
    TEST_ASSERT(io.ao[0] == 8);

    f.aoScale[0] = -2;
    CO_401_analog_applyOutputs(&d);
    TEST_ASSERT(io.ao[0] == -4);
    return true;
}

static bool test_optional_si_units_may_be_absent(void)
{
    fixture_t f;
    io_t io = {0};
    CO_401_device_t d;

    fixtureInit(&f);
    TEST_ASSERT(removeEntry(&f, CO_401_INDEX_ANALOG_INPUT_SI_UNIT));
    TEST_ASSERT(removeEntry(&f, CO_401_INDEX_ANALOG_OUTPUT_SI_UNIT));
    TEST_ASSERT(makeDevice(&f, &io, &d));
    TEST_ASSERT(d.bound.analogInputSiUnit == NULL);
    TEST_ASSERT(d.bound.analogOutputSiUnit == NULL);
    TEST_ASSERT(d.bound.analogInputOffset32 != NULL);
    TEST_ASSERT(d.bound.analogInputPrescaling32 != NULL);
    TEST_ASSERT(d.bound.analogOutputOffset32 != NULL);
    TEST_ASSERT(d.bound.analogOutputScaling32 != NULL);

    io.ai[0] = 4;
    f.ao[0] = 5;
    CO_401_analog_refreshInputs(&d);
    CO_401_analog_applyOutputs(&d);
    TEST_ASSERT(f.ai[0] == 4);
    TEST_ASSERT(io.ao[0] == 5);
    return true;
}

static bool test_conditioning_overflow_is_rejected_without_commit(void)
{
    fixture_t f;
    io_t io = {0};
    int16_t value = 12;
    unsigned writesBefore;
    CO_401_device_t d;

    fixtureInit(&f);
    TEST_ASSERT(makeDevice(&f, &io, &d));
    f.ai[0] = 12;
    f.aiOffset[0] = INT32_MIN;
    f.aiScale[0] = INT32_MIN;
    io.ai[0] = INT16_MIN;
    CO_401_analog_refreshInputs(&d);
    TEST_ASSERT(OD_get_i16(d.bound.analogInput16, 1U, &value, true) == ODR_OK);
    TEST_ASSERT(value == 12);

    f.ao[0] = INT16_MIN;
    f.ao[1] = INT16_MIN;
    f.aoOffset[0] = INT32_MIN;
    f.aoOffset[1] = INT32_MIN;
    f.aoScale[0] = INT32_MIN;
    f.aoScale[1] = INT32_MIN;
    io.ao[0] = 55;
    io.ao[1] = 66;
    writesBefore = io.writes;
    CO_401_analog_applyOutputs(&d);
    TEST_ASSERT(io.writes == writesBefore);
    TEST_ASSERT(io.ao[0] == 55);
    TEST_ASSERT(io.ao[1] == 66);
    return true;
}
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING */

int main(void)
{
    struct {
        const char *name;
        bool (*fn)(void);
    } cases[] = {
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
        {"local-read-no-network-commit", test_local_read_does_not_consume_network_state},
        {"source-commit-preserves-later-bits", test_source_commit_preserves_later_bits},
        {"zero-trigger-change-event", test_zero_trigger_uses_unconditional_change_event},
        {"limits-source-clear", test_limits_and_source_clear},
        {"delta-or", test_delta_and_or},
        {"delta-needs-network-reference", test_delta_requires_network_communication},
        {"global-warning", test_global_disable_and_operational_warning},
        {"generation-warning-rearm", test_communication_reset_rearms_operational_warning},
#if defined(CIA401_TEST_TPDO)
        {"source-clear-on-tpdo", test_source_clear_on_tpdo_transmit},
        {"source-only-tpdo-retry", test_source_only_tpdo_retries_after_send_failure},
        {"input-only-tpdo-retry", test_input_only_tpdo_retries_once_after_send_failure},
        {"output-supervision-sdo-rpdo", test_output_supervision_gate_on_sdo_and_rpdo_paths},
#endif
#endif
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
        {"ao-fault-bypass", test_output_fault_bypasses_conditioning},
        {"fault-source-or", test_output_fault_sources_are_ored},
#endif
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING)
        {"conditioning-scale-before-offset", test_conditioning_applies_scaling_before_offset},
        {"conditioning-optional-si-unit", test_optional_si_units_may_be_absent},
        {"conditioning-overflow", test_conditioning_overflow_is_rejected_without_commit},
#endif
    };
    unsigned i;

    printf("CIA401_ANALOG_VARIANT:%s\n", CIA401_ANALOG_VARIANT);
    for (i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++) {
        if (!cases[i].fn()) {
            return 1;
        }
        printf("CIA401_ANALOG_CASE_PASS:%s\n", cases[i].name);
    }
    printf("CIA401_ANALOG_PASS:%u/%u\n", i, (unsigned)(sizeof(cases) / sizeof(cases[0])));
    return 0;
}
