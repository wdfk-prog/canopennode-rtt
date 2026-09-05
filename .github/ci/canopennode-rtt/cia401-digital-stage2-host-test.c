/**
 * @file cia401-digital-stage2-host-test.c
 * @brief Host contract tests for optional CiA 401 Stage-2 digital profile semantics.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define OD_DEFINITION
#include "CO_401_device.h"
#include "CO_401_digital.h"

#if !defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS) \
    || !defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
#error This test requires both CiA 401 Stage-2 digital options.
#endif /* !DIGITAL_EVENTS || !DIGITAL_OUTPUT_FAILSAFE */


#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define TEST_DI_BANKS 2U
#define TEST_DO_BANKS 2U
#define TEST_OD_ENTRIES 13U

#define TEST_ASSERT(expr)                                                                     \
    do {                                                                                      \
        if (!(expr)) {                                                                        \
            fprintf(stderr, "CIA401_DIGITAL_STAGE2_FAIL:%s:%d:%s\n", __func__, __LINE__, #expr); \
            return false;                                                                     \
        }                                                                                     \
    } while (0)

typedef struct {
    uint32_t deviceType;
    uint8_t digitalInputSub0;
    uint8_t digitalOutputSub0;
    uint8_t digitalInterruptEnable;
    uint8_t digitalInputs[TEST_DI_BANKS];
    uint8_t digitalInputPolarity[TEST_DI_BANKS];
    uint8_t digitalInputFilter[TEST_DI_BANKS];
    uint8_t digitalInterruptAny[TEST_DI_BANKS];
    uint8_t digitalInterruptRising[TEST_DI_BANKS];
    uint8_t digitalInterruptFalling[TEST_DI_BANKS];
    uint8_t digitalOutputs[TEST_DO_BANKS];
    uint8_t digitalOutputPolarity[TEST_DO_BANKS];
    uint8_t digitalOutputErrorMode[TEST_DO_BANKS];
    uint8_t digitalOutputErrorValue[TEST_DO_BANKS];
    uint8_t digitalOutputFilter[TEST_DO_BANKS];

    OD_obj_var_t deviceTypeObject;
    OD_obj_var_t digitalInterruptEnableObject;
    OD_obj_array_t digitalInputObject;
    OD_obj_array_t digitalInputPolarityObject;
    OD_obj_array_t digitalInputFilterObject;
    OD_obj_array_t digitalInterruptAnyObject;
    OD_obj_array_t digitalInterruptRisingObject;
    OD_obj_array_t digitalInterruptFallingObject;
    OD_obj_array_t digitalOutputObject;
    OD_obj_array_t digitalOutputPolarityObject;
    OD_obj_array_t digitalOutputErrorModeObject;
    OD_obj_array_t digitalOutputErrorValueObject;
    OD_obj_array_t digitalOutputFilterObject;
    OD_entry_t entries[TEST_OD_ENTRIES];
    OD_t od;
} test_od_fixture_t;

typedef struct {
    uint8_t digitalInputs[TEST_DI_BANKS];
    uint8_t physicalOutputs[TEST_DO_BANKS];
    uint8_t filterMasks[TEST_DI_BANKS];
    unsigned digitalReadCalls;
    unsigned digitalWriteCalls;
    unsigned digitalMaskedWriteCalls;
    unsigned filterCalls;
    CO_401_io_result_t digitalReadResult;
    CO_401_io_result_t digitalWriteResult;
    CO_401_io_result_t filterResult;
} test_io_t;

static CO_401_io_result_t readDigital8(void *object, uint8_t bank, uint8_t *value)
{
    test_io_t *io = object;

    io->digitalReadCalls++;
    if (bank >= TEST_DI_BANKS || value == NULL) {
        return CO_401_IO_ERROR;
    }
    if (io->digitalReadResult == CO_401_IO_OK) {
        *value = io->digitalInputs[bank];
    }
    return io->digitalReadResult;
}

static CO_401_io_result_t writeDigital8(void *object, uint8_t bank, uint8_t value)
{
    test_io_t *io = object;

    io->digitalWriteCalls++;
    if (bank >= TEST_DO_BANKS) {
        return CO_401_IO_ERROR;
    }
    if (io->digitalWriteResult == CO_401_IO_OK) {
        io->physicalOutputs[bank] = value;
    }
    return io->digitalWriteResult;
}

static CO_401_io_result_t writeDigital8Masked(void *object, uint8_t bank, uint8_t value, uint8_t mask)
{
    test_io_t *io = object;

    io->digitalMaskedWriteCalls++;
    if (bank >= TEST_DO_BANKS) {
        return CO_401_IO_ERROR;
    }
    if (io->digitalWriteResult == CO_401_IO_OK) {
        io->physicalOutputs[bank] = (uint8_t)((io->physicalOutputs[bank] & (uint8_t)(~mask)) | (value & mask));
    }
    return io->digitalWriteResult;
}

static CO_401_io_result_t setDigitalInputFilter8(void *object, uint8_t bank, uint8_t enabledMask)
{
    test_io_t *io = object;

    io->filterCalls++;
    if (bank >= TEST_DI_BANKS) {
        return CO_401_IO_ERROR;
    }
    if (io->filterResult == CO_401_IO_OK) {
        io->filterMasks[bank] = enabledMask;
    }
    return io->filterResult;
}

static const CO_401_io_if_t completeIoIf = {
    .readDigital8 = readDigital8,
    .writeDigital8 = writeDigital8,
    .setDigitalInputFilter8 = setDigitalInputFilter8,
    .writeDigital8Masked = writeDigital8Masked,
    .readAnalog16 = NULL,
    .writeAnalog16 = NULL,
};

static void initArray(OD_obj_array_t *object, uint8_t *sub0, uint8_t *data, OD_attr_t attributes)
{
    object->dataOrig0 = sub0;
    object->dataOrig = data;
    object->attribute0 = ODA_SDO_R;
    object->attribute = attributes;
    object->dataElementLength = 1U;
    object->dataElementSizeof = 1U;
}

static void addEntry(test_od_fixture_t *fixture, uint16_t index, uint8_t count, uint8_t type, const void *object)
{
    OD_entry_t *entry = &fixture->entries[fixture->od.size];

    entry->index = index;
    entry->subEntriesCount = count;
    entry->odObjectType = type;
    entry->odObject = object;
    entry->extension = NULL;
    fixture->od.size++;
}

static void fixtureInit(test_od_fixture_t *fixture)
{
    (void)memset(fixture, 0, sizeof(*fixture));

    fixture->deviceType = CO_401_deviceTypeForCapabilities(CO_401_CAP_DIGITAL_INPUT | CO_401_CAP_DIGITAL_OUTPUT);
    fixture->digitalInputSub0 = TEST_DI_BANKS;
    fixture->digitalOutputSub0 = TEST_DO_BANKS;
    fixture->digitalInterruptEnable = 1U;
    (void)memset(fixture->digitalInterruptAny, UINT8_MAX, sizeof(fixture->digitalInterruptAny));
    (void)memset(fixture->digitalOutputErrorMode, UINT8_MAX, sizeof(fixture->digitalOutputErrorMode));
    (void)memset(fixture->digitalOutputFilter, UINT8_MAX, sizeof(fixture->digitalOutputFilter));

    fixture->deviceTypeObject.dataOrig = &fixture->deviceType;
    fixture->deviceTypeObject.attribute = ODA_SDO_R | ODA_MB;
    fixture->deviceTypeObject.dataLength = 4U;

    fixture->digitalInterruptEnableObject.dataOrig = &fixture->digitalInterruptEnable;
    fixture->digitalInterruptEnableObject.attribute = ODA_SDO_RW;
    fixture->digitalInterruptEnableObject.dataLength = 1U;

    initArray(&fixture->digitalInputObject, &fixture->digitalInputSub0, fixture->digitalInputs,
              ODA_SDO_R | ODA_TPDO);
    initArray(&fixture->digitalInputPolarityObject, &fixture->digitalInputSub0,
              fixture->digitalInputPolarity, ODA_SDO_RW);
    initArray(&fixture->digitalInputFilterObject, &fixture->digitalInputSub0,
              fixture->digitalInputFilter, ODA_SDO_RW);
    initArray(&fixture->digitalInterruptAnyObject, &fixture->digitalInputSub0,
              fixture->digitalInterruptAny, ODA_SDO_RW);
    initArray(&fixture->digitalInterruptRisingObject, &fixture->digitalInputSub0,
              fixture->digitalInterruptRising, ODA_SDO_RW);
    initArray(&fixture->digitalInterruptFallingObject, &fixture->digitalInputSub0,
              fixture->digitalInterruptFalling, ODA_SDO_RW);
    initArray(&fixture->digitalOutputObject, &fixture->digitalOutputSub0, fixture->digitalOutputs,
              ODA_SDO_RW | ODA_RPDO);
    initArray(&fixture->digitalOutputPolarityObject, &fixture->digitalOutputSub0,
              fixture->digitalOutputPolarity, ODA_SDO_RW);
    initArray(&fixture->digitalOutputErrorModeObject, &fixture->digitalOutputSub0,
              fixture->digitalOutputErrorMode, ODA_SDO_RW);
    initArray(&fixture->digitalOutputErrorValueObject, &fixture->digitalOutputSub0,
              fixture->digitalOutputErrorValue, ODA_SDO_RW);
    initArray(&fixture->digitalOutputFilterObject, &fixture->digitalOutputSub0,
              fixture->digitalOutputFilter, ODA_SDO_RW);

    fixture->od.list = fixture->entries;
    addEntry(fixture, CO_401_INDEX_DEVICE_TYPE, 1U, ODT_VAR, &fixture->deviceTypeObject);
    addEntry(fixture, CO_401_INDEX_DIGITAL_INPUT_8, TEST_DI_BANKS + 1U, ODT_ARR,
             &fixture->digitalInputObject);
    addEntry(fixture, CO_401_INDEX_DIGITAL_INPUT_POLARITY_8, TEST_DI_BANKS + 1U, ODT_ARR,
             &fixture->digitalInputPolarityObject);
    addEntry(fixture, CO_401_INDEX_DIGITAL_INPUT_FILTER_8, TEST_DI_BANKS + 1U, ODT_ARR,
             &fixture->digitalInputFilterObject);
    addEntry(fixture, CO_401_INDEX_DIGITAL_INTERRUPT_ENABLE, 1U, ODT_VAR,
             &fixture->digitalInterruptEnableObject);
    addEntry(fixture, CO_401_INDEX_DIGITAL_INTERRUPT_ANY_8, TEST_DI_BANKS + 1U, ODT_ARR,
             &fixture->digitalInterruptAnyObject);
    addEntry(fixture, CO_401_INDEX_DIGITAL_INTERRUPT_RISING_8, TEST_DI_BANKS + 1U, ODT_ARR,
             &fixture->digitalInterruptRisingObject);
    addEntry(fixture, CO_401_INDEX_DIGITAL_INTERRUPT_FALLING_8, TEST_DI_BANKS + 1U, ODT_ARR,
             &fixture->digitalInterruptFallingObject);
    addEntry(fixture, CO_401_INDEX_DIGITAL_OUTPUT_8, TEST_DO_BANKS + 1U, ODT_ARR,
             &fixture->digitalOutputObject);
    addEntry(fixture, CO_401_INDEX_DIGITAL_OUTPUT_POLARITY_8, TEST_DO_BANKS + 1U, ODT_ARR,
             &fixture->digitalOutputPolarityObject);
    addEntry(fixture, CO_401_INDEX_DIGITAL_OUTPUT_ERROR_MODE_8, TEST_DO_BANKS + 1U, ODT_ARR,
             &fixture->digitalOutputErrorModeObject);
    addEntry(fixture, CO_401_INDEX_DIGITAL_OUTPUT_ERROR_VALUE_8, TEST_DO_BANKS + 1U, ODT_ARR,
             &fixture->digitalOutputErrorValueObject);
    addEntry(fixture, CO_401_INDEX_DIGITAL_OUTPUT_FILTER_8, TEST_DO_BANKS + 1U, ODT_ARR,
             &fixture->digitalOutputFilterObject);
}

static void ioInit(test_io_t *io)
{
    (void)memset(io, 0, sizeof(*io));
    io->digitalReadResult = CO_401_IO_OK;
    io->digitalWriteResult = CO_401_IO_OK;
    io->filterResult = CO_401_IO_OK;
}

static CO_401_device_config_t makeConfig(test_io_t *io)
{
    CO_401_device_config_t config = {
        .io = &completeIoIf,
        .ioObject = io,
        .digitalInputBanks = TEST_DI_BANKS,
        .digitalOutputBanks = TEST_DO_BANKS,
        .analogInputChannels = 0U,
        .analogOutputChannels = 0U,
    };

    return config;
}

static bool removeEntry(test_od_fixture_t *fixture, uint16_t index)
{
    uint16_t i;

    for (i = 0U; i < fixture->od.size; i++) {
        if (fixture->entries[i].index == index) {
            const uint16_t remaining = (uint16_t)(fixture->od.size - i - 1U);

            if (remaining != 0U) {
                (void)memmove(&fixture->entries[i], &fixture->entries[i + 1U],
                              (size_t)remaining * sizeof(fixture->entries[0]));
            }
            fixture->od.size--;
            return true;
        }
    }
    return false;
}

static bool tpdoRequested(const OD_entry_t *entry, uint8_t subIndex)
{
    const uint8_t mask = (uint8_t)(1U << (subIndex & 0x07U));

    return entry != NULL && entry->extension != NULL
        && (entry->extension->flagsPDO[subIndex >> 3] & mask) == 0U;
}

static void clearTpdoRequest(OD_entry_t *entry, uint8_t subIndex)
{
    const uint8_t mask = (uint8_t)(1U << (subIndex & 0x07U));

    entry->extension->flagsPDO[subIndex >> 3] |= mask;
}

static ODR_t mappedWriteU8(OD_entry_t *entry, uint8_t subIndex, uint8_t value)
{
    OD_IO_t io;
    OD_size_t countWritten = 0U;
    ODR_t result = OD_getSub(entry, subIndex, &io, false);

    if (result != ODR_OK) {
        return result;
    }
    result = io.write(&io.stream, &value, 1U, &countWritten);
    if (result == ODR_OK && countWritten != 1U) {
        return ODR_DEV_INCOMPAT;
    }
    return result;
}

static bool test_stage2_binding_requires_optional_contract(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;

    fixtureInit(&fixture);
    ioInit(&io);
    config = makeConfig(&io);

    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);
    TEST_ASSERT(device.bound.digitalInput8->extension == &device.digitalInput8Extension);
    TEST_ASSERT(device.bound.digitalInputFilter8->extension == &device.digitalInputFilter8Extension);
    TEST_ASSERT(device.bound.digitalOutput8->extension == &device.digitalOutput8Extension);

    fixtureInit(&fixture);
    TEST_ASSERT(removeEntry(&fixture, CO_401_INDEX_DIGITAL_INTERRUPT_FALLING_8));
    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OD_MISSING);
    TEST_ASSERT(diag.index == CO_401_INDEX_DIGITAL_INTERRUPT_FALLING_8);
    return true;
}

static bool test_failed_rebind_detaches_owned_extensions(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;

    fixtureInit(&fixture);
    ioInit(&io);
    config = makeConfig(&io);
    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);
    TEST_ASSERT(device.bound.digitalInput8->extension == &device.digitalInput8Extension);
    TEST_ASSERT(device.bound.digitalInputFilter8->extension == &device.digitalInputFilter8Extension);
    TEST_ASSERT(device.bound.digitalOutput8->extension == &device.digitalOutput8Extension);

    fixture.digitalInterruptFallingObject.dataElementLength = 2U;
    TEST_ASSERT(CO_401_device_bindOD(&device, &diag) == CO_401_INIT_OD_LENGTH);
    TEST_ASSERT(!device.odBound);
    TEST_ASSERT(OD_find(&fixture.od, CO_401_INDEX_DIGITAL_INPUT_8)->extension == NULL);
    TEST_ASSERT(OD_find(&fixture.od, CO_401_INDEX_DIGITAL_INPUT_FILTER_8)->extension == NULL);
    TEST_ASSERT(OD_find(&fixture.od, CO_401_INDEX_DIGITAL_OUTPUT_8)->extension == NULL);

    fixture.digitalInterruptFallingObject.dataElementLength = 1U;
    TEST_ASSERT(CO_401_device_bindOD(&device, &diag) == CO_401_INIT_OK);
    TEST_ASSERT(device.bound.digitalInput8->extension == &device.digitalInput8Extension);
    TEST_ASSERT(device.bound.digitalInputFilter8->extension == &device.digitalInputFilter8Extension);
    TEST_ASSERT(device.bound.digitalOutput8->extension == &device.digitalOutput8Extension);
    return true;
}

static bool test_filter_bridge_forwards_and_retries(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;
    uint8_t logicalValue;
    unsigned readsBeforeFailure;

    fixtureInit(&fixture);
    ioInit(&io);
    config = makeConfig(&io);
    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);

    CO_401_digital_refreshInputs(&device);
    TEST_ASSERT(io.filterCalls == TEST_DI_BANKS);
    TEST_ASSERT(!device.digitalInputFilterDirty);

    TEST_ASSERT(OD_set_u8(device.bound.digitalInput8, 1U, 0x11U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalInputFilter8, 1U, 0xA5U, false) == ODR_OK);
    TEST_ASSERT(device.digitalInputFilterDirty);
    io.digitalInputs[0] = 0x22U;
    clearTpdoRequest(device.bound.digitalInput8, 1U);
    readsBeforeFailure = io.digitalReadCalls;

    io.filterResult = CO_401_IO_BUSY;
    CO_401_digital_refreshInputs(&device);
    TEST_ASSERT(device.digitalInputFilterDirty);
    TEST_ASSERT(io.digitalReadCalls == readsBeforeFailure);
    TEST_ASSERT(OD_get_u8(device.bound.digitalInput8, 1U, &logicalValue, true) == ODR_OK);
    TEST_ASSERT(logicalValue == 0x11U);
    TEST_ASSERT(!tpdoRequested(device.bound.digitalInput8, 1U));

    io.filterResult = CO_401_IO_ERROR;
    CO_401_digital_refreshInputs(&device);
    TEST_ASSERT(device.digitalInputFilterDirty);
    TEST_ASSERT(io.digitalReadCalls == readsBeforeFailure);
    TEST_ASSERT(OD_get_u8(device.bound.digitalInput8, 1U, &logicalValue, true) == ODR_OK);
    TEST_ASSERT(logicalValue == 0x11U);
    TEST_ASSERT(!tpdoRequested(device.bound.digitalInput8, 1U));

    io.filterResult = CO_401_IO_OK;
    CO_401_digital_refreshInputs(&device);
    TEST_ASSERT(!device.digitalInputFilterDirty);
    TEST_ASSERT(io.filterMasks[0] == 0xA5U);
    TEST_ASSERT(OD_get_u8(device.bound.digitalInput8, 1U, &logicalValue, true) == ODR_OK);
    TEST_ASSERT(logicalValue == 0x22U);
    TEST_ASSERT(tpdoRequested(device.bound.digitalInput8, 1U));
    {
        const unsigned completedCalls = io.filterCalls;
        CO_401_digital_refreshInputs(&device);
        TEST_ASSERT(io.filterCalls == completedCalls);
    }
    return true;
}
static bool test_polarity_is_applied_before_logical_falling_edge(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;
    uint8_t logicalValue;

    fixtureInit(&fixture);
    ioInit(&io);
    config = makeConfig(&io);
    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalInput8, 1U, 0x01U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalInputPolarity8, 1U, 0x01U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalInterruptAny8, 1U, 0x00U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalInterruptRising8, 1U, 0x00U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalInterruptFalling8, 1U, 0x01U, true) == ODR_OK);
    io.digitalInputs[0] = 0x01U;
    clearTpdoRequest(device.bound.digitalInput8, 1U);

    CO_401_digital_refreshInputs(&device);

    TEST_ASSERT(OD_get_u8(device.bound.digitalInput8, 1U, &logicalValue, true) == ODR_OK);
    TEST_ASSERT(logicalValue == 0x00U);
    TEST_ASSERT(tpdoRequested(device.bound.digitalInput8, 1U));
    return true;
}

static bool test_any_rising_falling_masks_are_ored(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;

    fixtureInit(&fixture);
    ioInit(&io);
    config = makeConfig(&io);
    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);

    /* Any-change source only. */
    TEST_ASSERT(OD_set_u8(device.bound.digitalInput8, 1U, 0x00U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalInterruptAny8, 1U, 0x01U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalInterruptRising8, 1U, 0x00U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalInterruptFalling8, 1U, 0x00U, true) == ODR_OK);
    io.digitalInputs[0] = 0x01U;
    clearTpdoRequest(device.bound.digitalInput8, 1U);
    CO_401_digital_refreshInputs(&device);
    TEST_ASSERT(tpdoRequested(device.bound.digitalInput8, 1U));

    /* Rising-edge source only. */
    TEST_ASSERT(OD_set_u8(device.bound.digitalInput8, 1U, 0x01U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalInterruptAny8, 1U, 0x00U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalInterruptRising8, 1U, 0x02U, true) == ODR_OK);
    io.digitalInputs[0] = 0x03U;
    clearTpdoRequest(device.bound.digitalInput8, 1U);
    CO_401_digital_refreshInputs(&device);
    TEST_ASSERT(tpdoRequested(device.bound.digitalInput8, 1U));

    /* Falling-edge source only. */
    TEST_ASSERT(OD_set_u8(device.bound.digitalInput8, 1U, 0x03U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalInterruptRising8, 1U, 0x00U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalInterruptFalling8, 1U, 0x02U, true) == ODR_OK);
    io.digitalInputs[0] = 0x01U;
    clearTpdoRequest(device.bound.digitalInput8, 1U);
    CO_401_digital_refreshInputs(&device);
    TEST_ASSERT(tpdoRequested(device.bound.digitalInput8, 1U));

    /* A change outside every enabled mask must not request transmission. */
    TEST_ASSERT(OD_set_u8(device.bound.digitalInput8, 1U, 0x01U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalInterruptFalling8, 1U, 0x80U, true) == ODR_OK);
    io.digitalInputs[0] = 0x00U;
    clearTpdoRequest(device.bound.digitalInput8, 1U);
    CO_401_digital_refreshInputs(&device);
    TEST_ASSERT(!tpdoRequested(device.bound.digitalInput8, 1U));
    return true;
}
static bool test_global_interrupt_disable_suppresses_request(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;
    uint8_t logicalValue;

    fixtureInit(&fixture);
    ioInit(&io);
    config = makeConfig(&io);
    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalInterruptEnable, 0U, 0U, true) == ODR_OK);
    io.digitalInputs[0] = 0x80U;
    clearTpdoRequest(device.bound.digitalInput8, 1U);

    CO_401_digital_refreshInputs(&device);

    TEST_ASSERT(!tpdoRequested(device.bound.digitalInput8, 1U));
    TEST_ASSERT(OD_get_u8(device.bound.digitalInput8, 1U, &logicalValue, true) == ODR_OK);
    TEST_ASSERT(logicalValue == 0x80U);
    return true;
}

static bool test_event_request_uses_6000_subindex_flag(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;

    fixtureInit(&fixture);
    ioInit(&io);
    config = makeConfig(&io);
    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);
    io.digitalInputs[1] = 0x01U;
    clearTpdoRequest(device.bound.digitalInput8, 1U);
    clearTpdoRequest(device.bound.digitalInput8, 2U);

    CO_401_digital_refreshInputs(&device);

    TEST_ASSERT(!tpdoRequested(device.bound.digitalInput8, 1U));
    TEST_ASSERT(tpdoRequested(device.bound.digitalInput8, 2U));
    return true;
}

static bool test_output_filter_masks_final_physical_write(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;
    uint8_t command;

    fixtureInit(&fixture);
    ioInit(&io);
    io.physicalOutputs[0] = 0xA0U;
    config = makeConfig(&io);
    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputPolarity8, 1U, 0x0FU, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputFilter8, 1U, 0x0FU, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputFilter8, 2U, 0x00U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutput8, 1U, 0x55U, false) == ODR_OK);
    TEST_ASSERT(OD_get_u8(device.bound.digitalOutput8, 1U, &command, true) == ODR_OK);
    TEST_ASSERT(command == 0x55U);

    CO_401_digital_applyOutputs(&device);
    TEST_ASSERT(io.digitalWriteCalls == 0U);
    TEST_ASSERT(io.digitalMaskedWriteCalls == 1U);
    TEST_ASSERT(io.physicalOutputs[0] == 0xAAU);

    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputFilter8, 1U, 0x00U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutput8, 1U, 0x00U, false) == ODR_OK);
    TEST_ASSERT(OD_get_u8(device.bound.digitalOutput8, 1U, &command, true) == ODR_OK);
    TEST_ASSERT(command == 0x00U);
    CO_401_digital_applyOutputs(&device);
    TEST_ASSERT(io.digitalWriteCalls == 0U);
    TEST_ASSERT(io.digitalMaskedWriteCalls == 1U);
    TEST_ASSERT(io.physicalOutputs[0] == 0xAAU);
    return true;
}

static bool test_sdo_and_mapped_write_keep_full_6200_command(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;
    uint8_t command;

    fixtureInit(&fixture);
    ioInit(&io);
    io.physicalOutputs[0] = 0xF0U;
    config = makeConfig(&io);
    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputFilter8, 1U, 0x0FU, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputFilter8, 2U, 0x00U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutput8, 1U, 0xA5U, false) == ODR_OK);
    TEST_ASSERT(OD_get_u8(device.bound.digitalOutput8, 1U, &command, true) == ODR_OK);
    TEST_ASSERT(command == 0xA5U);
    CO_401_digital_applyOutputs(&device);
    TEST_ASSERT(io.physicalOutputs[0] == 0xF5U);

    TEST_ASSERT(mappedWriteU8(device.bound.digitalOutput8, 1U, 0x0AU) == ODR_OK);
    TEST_ASSERT(OD_get_u8(device.bound.digitalOutput8, 1U, &command, true) == ODR_OK);
    TEST_ASSERT(command == 0x0AU);
    CO_401_digital_applyOutputs(&device);
    TEST_ASSERT(io.physicalOutputs[0] == 0xFAU);
    return true;
}

static bool test_output_filter_applies_after_fault_and_polarity(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;
    uint8_t command;

    fixtureInit(&fixture);
    ioInit(&io);
    io.physicalOutputs[0] = 0xA5U;
    config = makeConfig(&io);
    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutput8, 1U, 0x3CU, false) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputPolarity8, 1U, 0x0FU, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputErrorMode8, 1U, 0x0FU, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputErrorValue8, 1U, 0x05U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputFilter8, 1U, 0x03U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputFilter8, 2U, 0x00U, true) == ODR_OK);

    CO_401_device_setDigitalOutputFault(&device, true);
    CO_401_digital_applyOutputs(&device);

    TEST_ASSERT(io.digitalWriteCalls == 0U);
    TEST_ASSERT(io.digitalMaskedWriteCalls == 1U);
    TEST_ASSERT(io.physicalOutputs[0] == 0xA6U);
    TEST_ASSERT(OD_get_u8(device.bound.digitalOutput8, 1U, &command, true) == ODR_OK);
    TEST_ASSERT(command == 0x3CU);
    return true;
}

static bool test_output_filter_masked_write_retries_after_backend_failure(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;
    uint8_t command;

    fixtureInit(&fixture);
    ioInit(&io);
    io.physicalOutputs[0] = 0xA0U;
    config = makeConfig(&io);
    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputFilter8, 1U, 0x0FU, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputFilter8, 2U, 0x00U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutput8, 1U, 0x55U, false) == ODR_OK);

    io.digitalWriteResult = CO_401_IO_BUSY;
    CO_401_digital_applyOutputs(&device);
    TEST_ASSERT(io.digitalMaskedWriteCalls == 1U);
    TEST_ASSERT(io.physicalOutputs[0] == 0xA0U);

    io.digitalWriteResult = CO_401_IO_ERROR;
    CO_401_digital_applyOutputs(&device);
    TEST_ASSERT(io.digitalMaskedWriteCalls == 2U);
    TEST_ASSERT(io.physicalOutputs[0] == 0xA0U);
    TEST_ASSERT(OD_get_u8(device.bound.digitalOutput8, 1U, &command, true) == ODR_OK);
    TEST_ASSERT(command == 0x55U);

    io.digitalWriteResult = CO_401_IO_OK;
    CO_401_digital_applyOutputs(&device);
    TEST_ASSERT(io.digitalMaskedWriteCalls == 3U);
    TEST_ASSERT(io.physicalOutputs[0] == 0xA5U);
    TEST_ASSERT(OD_get_u8(device.bound.digitalOutput8, 1U, &command, true) == ODR_OK);
    TEST_ASSERT(command == 0x55U);
    return true;
}

static bool test_output_polarity_and_fault_keep_error_value(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;
    uint8_t command;
    unsigned writesBeforeKeep;
    unsigned maskedWritesBeforeKeep;

    fixtureInit(&fixture);
    ioInit(&io);
    config = makeConfig(&io);
    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutput8, 1U, 0x3CU, false) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputPolarity8, 1U, 0x0FU, true) == ODR_OK);
    CO_401_digital_applyOutputs(&device);
    TEST_ASSERT(io.physicalOutputs[0] == 0x33U);

    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputErrorMode8, 1U, 0x0FU, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputErrorValue8, 1U, 0x05U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutput8, 1U, 0xF0U, false) == ODR_OK);
    CO_401_device_setDigitalOutputFault(&device, true);
    CO_401_digital_applyOutputs(&device);
    TEST_ASSERT(io.physicalOutputs[0] == 0x3AU);
    TEST_ASSERT(OD_get_u8(device.bound.digitalOutput8, 1U, &command, true) == ODR_OK);
    TEST_ASSERT(command == 0xF0U);

    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputErrorMode8, 1U, 0x00U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputErrorMode8, 2U, 0x00U, true) == ODR_OK);
    writesBeforeKeep = io.digitalWriteCalls;
    maskedWritesBeforeKeep = io.digitalMaskedWriteCalls;
    CO_401_digital_applyOutputs(&device);
    TEST_ASSERT(io.digitalWriteCalls == writesBeforeKeep);
    TEST_ASSERT(io.digitalMaskedWriteCalls == maskedWritesBeforeKeep);
    TEST_ASSERT(io.physicalOutputs[0] == 0x3AU);

    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputErrorMode8, 1U, UINT8_MAX, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputErrorValue8, 1U, 0x96U, true) == ODR_OK);
    CO_401_digital_applyOutputs(&device);
    TEST_ASSERT(io.physicalOutputs[0] == 0x99U);

    CO_401_device_setDigitalOutputFault(&device, false);
    CO_401_digital_applyOutputs(&device);
    TEST_ASSERT(io.physicalOutputs[0] == 0xFFU);
    return true;
}
static bool test_mixed_fault_preserves_backend_state_before_first_normal_write(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;
    uint8_t command;

    fixtureInit(&fixture);
    ioInit(&io);
    io.physicalOutputs[0] = 0xA0U;
    config = makeConfig(&io);
    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutput8, 1U, 0x3CU, false) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputPolarity8, 1U, 0x0FU, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputErrorMode8, 1U, 0x0FU, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputErrorValue8, 1U, 0x05U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputErrorMode8, 2U, 0x00U, true) == ODR_OK);

    CO_401_device_setDigitalOutputFault(&device, true);
    CO_401_digital_applyOutputs(&device);

    TEST_ASSERT(io.digitalWriteCalls == 0U);
    TEST_ASSERT(io.digitalMaskedWriteCalls == 1U);
    TEST_ASSERT(io.physicalOutputs[0] == 0xAAU);
    TEST_ASSERT(OD_get_u8(device.bound.digitalOutput8, 1U, &command, true) == ODR_OK);
    TEST_ASSERT(command == 0x3CU);
    return true;
}

static bool test_masked_fault_write_retries_after_backend_failure(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;
    uint8_t command;

    fixtureInit(&fixture);
    ioInit(&io);
    io.physicalOutputs[0] = 0xA0U;
    config = makeConfig(&io);
    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutput8, 1U, 0x3CU, false) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputPolarity8, 1U, 0x0FU, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputErrorMode8, 1U, 0x0FU, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputErrorValue8, 1U, 0x05U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutputErrorMode8, 2U, 0x00U, true) == ODR_OK);
    CO_401_device_setDigitalOutputFault(&device, true);

    io.digitalWriteResult = CO_401_IO_BUSY;
    CO_401_digital_applyOutputs(&device);
    TEST_ASSERT(io.digitalMaskedWriteCalls == 1U);
    TEST_ASSERT(io.physicalOutputs[0] == 0xA0U);

    io.digitalWriteResult = CO_401_IO_ERROR;
    CO_401_digital_applyOutputs(&device);
    TEST_ASSERT(io.digitalMaskedWriteCalls == 2U);
    TEST_ASSERT(io.physicalOutputs[0] == 0xA0U);
    TEST_ASSERT(OD_get_u8(device.bound.digitalOutput8, 1U, &command, true) == ODR_OK);
    TEST_ASSERT(command == 0x3CU);

    io.digitalWriteResult = CO_401_IO_OK;
    CO_401_digital_applyOutputs(&device);
    TEST_ASSERT(io.digitalMaskedWriteCalls == 3U);
    TEST_ASSERT(io.physicalOutputs[0] == 0xAAU);
    TEST_ASSERT(OD_get_u8(device.bound.digitalOutput8, 1U, &command, true) == ODR_OK);
    TEST_ASSERT(command == 0x3CU);
    return true;
}

static bool test_event_request_capacity_is_fail_closed(void)
{
#if OD_FLAGS_PDO_SIZE < 32
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;

    fixtureInit(&fixture);
    ioInit(&io);
    config = makeConfig(&io);
    config.digitalInputBanks = (uint8_t)(OD_FLAGS_PDO_SIZE * 8U);

    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_CONFIG);
    TEST_ASSERT(!device.odBound);
#endif /* OD_FLAGS_PDO_SIZE < 32 */
    return true;
}

static bool test_missing_filter_callback_fails_closed(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;
    CO_401_io_if_t incomplete = completeIoIf;

    fixtureInit(&fixture);
    ioInit(&io);
    incomplete.setDigitalInputFilter8 = NULL;
    config = makeConfig(&io);
    config.io = &incomplete;

    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_IO_IF);
    TEST_ASSERT(!device.odBound);
    return true;
}

static bool test_missing_masked_output_callback_fails_closed(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;
    CO_401_io_if_t incomplete = completeIoIf;

    fixtureInit(&fixture);
    ioInit(&io);
    incomplete.writeDigital8Masked = NULL;
    config = makeConfig(&io);
    config.io = &incomplete;

    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_IO_IF);
    TEST_ASSERT(!device.odBound);
    return true;
}

typedef bool (*test_function_t)(void);

typedef struct {
    const char *name;
    test_function_t function;
} test_case_t;

int main(void)
{
    static const test_case_t tests[] = {
        {"stage2-binding-contract", test_stage2_binding_requires_optional_contract},
        {"failed-rebind-detach", test_failed_rebind_detaches_owned_extensions},
        {"filter-bridge-retry", test_filter_bridge_forwards_and_retries},
        {"polarity-before-logical-edge", test_polarity_is_applied_before_logical_falling_edge},
        {"event-mask-or", test_any_rising_falling_masks_are_ored},
        {"global-interrupt-disable", test_global_interrupt_disable_suppresses_request},
        {"event-request-od-subindex", test_event_request_uses_6000_subindex_flag},
        {"output-filter-final-physical", test_output_filter_masks_final_physical_write},
        {"sdo-rpdo-full-command", test_sdo_and_mapped_write_keep_full_6200_command},
        {"output-filter-after-fault-polarity", test_output_filter_applies_after_fault_and_polarity},
        {"output-filter-write-retry", test_output_filter_masked_write_retries_after_backend_failure},
        {"output-polarity-failsafe", test_output_polarity_and_fault_keep_error_value},
        {"mixed-fault-first-write", test_mixed_fault_preserves_backend_state_before_first_normal_write},
        {"masked-fault-write-retry", test_masked_fault_write_retries_after_backend_failure},
        {"event-request-capacity", test_event_request_capacity_is_fail_closed},
        {"missing-filter-callback", test_missing_filter_callback_fails_closed},
        {"missing-masked-output-callback", test_missing_masked_output_callback_fails_closed},
    };
    size_t i;

    for (i = 0U; i < ARRAY_COUNT(tests); i++) {
        if (!tests[i].function()) {
            return 1;
        }
        printf("CIA401_DIGITAL_STAGE2_CASE_PASS:%s\n", tests[i].name);
    }

    printf("CIA401_DIGITAL_STAGE2_PASS:%zu/%zu\n", ARRAY_COUNT(tests), ARRAY_COUNT(tests));
    return 0;
}
