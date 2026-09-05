/**
 * @file cia401-device-host-test.c
 * @brief Host-only Stage-1 contract and process-image checks for the Pure-C CiA 401 Device core.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define OD_DEFINITION
#include "CO_401_device.h"
#include "CO_401_digital.h"
#include "CO_401_analog.h"

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define TEST_DI_BANKS 2U
#define TEST_DO_BANKS 2U
#define TEST_AI_CHANNELS 4U
#define TEST_AO_CHANNELS 4U

#define TEST_ASSERT(expr)                                                                     \
    do {                                                                                      \
        if (!(expr)) {                                                                        \
            fprintf(stderr, "CIA401_DEVICE_HOST_FAIL:%s:%d:%s\n", __func__, __LINE__, #expr); \
            return false;                                                                     \
        }                                                                                     \
    } while (0)

typedef struct {
    uint32_t deviceType;
    uint8_t digitalInputSub0;
    uint8_t digitalOutputSub0;
    uint8_t analogInputSub0;
    uint8_t analogOutputSub0;
    uint8_t digitalInputs[TEST_DI_BANKS];
    uint8_t digitalOutputs[TEST_DO_BANKS];
    int16_t analogInputs[TEST_AI_CHANNELS];
    int16_t analogOutputs[TEST_AO_CHANNELS];

    OD_obj_var_t deviceTypeObject;
    OD_obj_array_t digitalInputObject;
    OD_obj_array_t digitalOutputObject;
    OD_obj_array_t analogInputObject;
    OD_obj_array_t analogOutputObject;
    OD_entry_t entries[6];
    OD_t od;
} test_od_fixture_t;

typedef struct {
    uint8_t digitalInputs[TEST_DI_BANKS];
    uint8_t digitalOutputs[TEST_DO_BANKS];
    int16_t analogInputs[TEST_AI_CHANNELS];
    int16_t analogOutputs[TEST_AO_CHANNELS];
    unsigned digitalReadCalls;
    unsigned digitalWriteCalls;
    unsigned analogReadCalls;
    unsigned analogWriteCalls;
    CO_401_io_result_t digitalReadResult;
    CO_401_io_result_t digitalWriteResult;
    CO_401_io_result_t analogReadResult;
    CO_401_io_result_t analogWriteResult;
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
        io->digitalOutputs[bank] = value;
    }
    return io->digitalWriteResult;
}

static CO_401_io_result_t readAnalog16(void *object, uint8_t channel, int16_t *value)
{
    test_io_t *io = object;

    io->analogReadCalls++;
    if (channel >= TEST_AI_CHANNELS || value == NULL) {
        return CO_401_IO_ERROR;
    }
    if (io->analogReadResult == CO_401_IO_OK) {
        *value = io->analogInputs[channel];
    }
    return io->analogReadResult;
}

static CO_401_io_result_t writeAnalog16(void *object, uint8_t channel, int16_t value)
{
    test_io_t *io = object;

    io->analogWriteCalls++;
    if (channel >= TEST_AO_CHANNELS) {
        return CO_401_IO_ERROR;
    }
    if (io->analogWriteResult == CO_401_IO_OK) {
        io->analogOutputs[channel] = value;
    }
    return io->analogWriteResult;
}

static const CO_401_io_if_t completeIoIf = {
    .readDigital8 = readDigital8,
    .writeDigital8 = writeDigital8,
    .readAnalog16 = readAnalog16,
    .writeAnalog16 = writeAnalog16,
};

static CO_401_device_config_t configForCapabilities(CO_401_capabilities_t capabilities, test_io_t *io)
{
    CO_401_device_config_t config = {
        .io = &completeIoIf,
        .ioObject = io,
        .digitalInputBanks = (capabilities & CO_401_CAP_DIGITAL_INPUT) != 0U ? TEST_DI_BANKS : 0U,
        .digitalOutputBanks = (capabilities & CO_401_CAP_DIGITAL_OUTPUT) != 0U ? TEST_DO_BANKS : 0U,
        .analogInputChannels = (capabilities & CO_401_CAP_ANALOG_INPUT) != 0U ? TEST_AI_CHANNELS : 0U,
        .analogOutputChannels = (capabilities & CO_401_CAP_ANALOG_OUTPUT) != 0U ? TEST_AO_CHANNELS : 0U,
    };

    return config;
}

static void addEntry(test_od_fixture_t *fixture, uint16_t index, uint8_t count, uint8_t objectType, const void *object)
{
    OD_entry_t *entry = &fixture->entries[fixture->od.size];

    entry->index = index;
    entry->subEntriesCount = count;
    entry->odObjectType = objectType;
    entry->odObject = object;
    entry->extension = NULL;
    fixture->od.size++;
}

static void fixtureInit(test_od_fixture_t *fixture, CO_401_capabilities_t capabilities)
{
    (void)memset(fixture, 0, sizeof(*fixture));

    fixture->deviceType = CO_401_deviceTypeForCapabilities(capabilities);
    fixture->digitalInputSub0 = TEST_DI_BANKS;
    fixture->digitalOutputSub0 = TEST_DO_BANKS;
    fixture->analogInputSub0 = TEST_AI_CHANNELS;
    fixture->analogOutputSub0 = TEST_AO_CHANNELS;

    fixture->deviceTypeObject.dataOrig = &fixture->deviceType;
    fixture->deviceTypeObject.attribute = ODA_SDO_R | ODA_MB;
    fixture->deviceTypeObject.dataLength = 4U;

    fixture->digitalInputObject.dataOrig0 = &fixture->digitalInputSub0;
    fixture->digitalInputObject.dataOrig = fixture->digitalInputs;
    fixture->digitalInputObject.attribute0 = ODA_SDO_R;
    fixture->digitalInputObject.attribute = ODA_SDO_R | ODA_TPDO;
    fixture->digitalInputObject.dataElementLength = 1U;
    fixture->digitalInputObject.dataElementSizeof = sizeof(fixture->digitalInputs[0]);

    fixture->digitalOutputObject.dataOrig0 = &fixture->digitalOutputSub0;
    fixture->digitalOutputObject.dataOrig = fixture->digitalOutputs;
    fixture->digitalOutputObject.attribute0 = ODA_SDO_R;
    fixture->digitalOutputObject.attribute = ODA_SDO_RW | ODA_RPDO;
    fixture->digitalOutputObject.dataElementLength = 1U;
    fixture->digitalOutputObject.dataElementSizeof = sizeof(fixture->digitalOutputs[0]);

    fixture->analogInputObject.dataOrig0 = &fixture->analogInputSub0;
    fixture->analogInputObject.dataOrig = fixture->analogInputs;
    fixture->analogInputObject.attribute0 = ODA_SDO_R;
    fixture->analogInputObject.attribute = ODA_SDO_R | ODA_TPDO | ODA_MB;
    fixture->analogInputObject.dataElementLength = 2U;
    fixture->analogInputObject.dataElementSizeof = sizeof(fixture->analogInputs[0]);

    fixture->analogOutputObject.dataOrig0 = &fixture->analogOutputSub0;
    fixture->analogOutputObject.dataOrig = fixture->analogOutputs;
    fixture->analogOutputObject.attribute0 = ODA_SDO_R;
    fixture->analogOutputObject.attribute = ODA_SDO_RW | ODA_RPDO | ODA_MB;
    fixture->analogOutputObject.dataElementLength = 2U;
    fixture->analogOutputObject.dataElementSizeof = sizeof(fixture->analogOutputs[0]);

    fixture->od.list = fixture->entries;
    fixture->od.size = 0U;
    addEntry(fixture, CO_401_INDEX_DEVICE_TYPE, 1U, ODT_VAR, &fixture->deviceTypeObject);
    if ((capabilities & CO_401_CAP_DIGITAL_INPUT) != 0U) {
        addEntry(fixture, CO_401_INDEX_DIGITAL_INPUT_8, TEST_DI_BANKS + 1U, ODT_ARR,
                 &fixture->digitalInputObject);
    }
    if ((capabilities & CO_401_CAP_DIGITAL_OUTPUT) != 0U) {
        addEntry(fixture, CO_401_INDEX_DIGITAL_OUTPUT_8, TEST_DO_BANKS + 1U, ODT_ARR,
                 &fixture->digitalOutputObject);
    }
    if ((capabilities & CO_401_CAP_ANALOG_INPUT) != 0U) {
        addEntry(fixture, CO_401_INDEX_ANALOG_INPUT_16, TEST_AI_CHANNELS + 1U, ODT_ARR,
                 &fixture->analogInputObject);
    }
    if ((capabilities & CO_401_CAP_ANALOG_OUTPUT) != 0U) {
        addEntry(fixture, CO_401_INDEX_ANALOG_OUTPUT_16, TEST_AO_CHANNELS + 1U, ODT_ARR,
                 &fixture->analogOutputObject);
    }
}

static OD_entry_t *fixtureFind(test_od_fixture_t *fixture, uint16_t index)
{
    return OD_find(&fixture->od, index);
}

static bool fixtureRemove(test_od_fixture_t *fixture, uint16_t index)
{
    uint16_t i;

    for (i = 0U; i < fixture->od.size; i++) {
        if (fixture->entries[i].index == index) {
            uint16_t remaining = (uint16_t)(fixture->od.size - i - 1U);
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

static void ioInit(test_io_t *io)
{
    (void)memset(io, 0, sizeof(*io));
    io->digitalReadResult = CO_401_IO_OK;
    io->digitalWriteResult = CO_401_IO_OK;
    io->analogReadResult = CO_401_IO_OK;
    io->analogWriteResult = CO_401_IO_OK;
}

static bool test_all_capability_combinations(void)
{
    CO_401_capabilities_t capabilities;

    for (capabilities = 1U; capabilities <= CO_401_CAP_ALL; capabilities++) {
        test_od_fixture_t fixture;
        test_io_t io;
        CO_401_device_t device;
        CO_401_init_diag_t diag;
        CO_401_device_config_t config;

        ioInit(&io);
        fixtureInit(&fixture, capabilities);
        config = configForCapabilities(capabilities, &io);
        TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);
        TEST_ASSERT(device.odBound);
        TEST_ASSERT(device.capabilities == capabilities);
    }

    return true;
}

static bool test_missing_object_fails_closed(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;

    ioInit(&io);
    fixtureInit(&fixture, CO_401_CAP_DIGITAL_INPUT);
    TEST_ASSERT(fixtureRemove(&fixture, CO_401_INDEX_DIGITAL_INPUT_8));
    config = configForCapabilities(CO_401_CAP_DIGITAL_INPUT, &io);

    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OD_MISSING);
    TEST_ASSERT(diag.index == CO_401_INDEX_DIGITAL_INPUT_8);
    TEST_ASSERT(!device.odBound);
    return true;
}

static bool test_wrong_object_type_fails_closed(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;
    OD_entry_t *entry;

    ioInit(&io);
    fixtureInit(&fixture, CO_401_CAP_DIGITAL_INPUT);
    entry = fixtureFind(&fixture, CO_401_INDEX_DIGITAL_INPUT_8);
    TEST_ASSERT(entry != NULL);
    entry->odObjectType = ODT_VAR;
    config = configForCapabilities(CO_401_CAP_DIGITAL_INPUT, &io);

    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OD_TYPE);
    TEST_ASSERT(diag.index == CO_401_INDEX_DIGITAL_INPUT_8);
    TEST_ASSERT(!device.odBound);
    return true;
}

static bool test_wrong_width_fails_closed(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;

    ioInit(&io);
    fixtureInit(&fixture, CO_401_CAP_ANALOG_INPUT);
    fixture.analogInputObject.dataElementLength = 4U;
    config = configForCapabilities(CO_401_CAP_ANALOG_INPUT, &io);

    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OD_LENGTH);
    TEST_ASSERT(diag.index == CO_401_INDEX_ANALOG_INPUT_16);
    TEST_ASSERT(diag.subIndex == 1U);
    TEST_ASSERT(!device.odBound);
    return true;
}

static bool test_wrong_digital_stride_fails_closed(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;

    ioInit(&io);
    fixtureInit(&fixture, CO_401_CAP_DIGITAL_INPUT);
    fixture.digitalInputObject.dataElementSizeof = 2U;
    config = configForCapabilities(CO_401_CAP_DIGITAL_INPUT, &io);

    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OD_LENGTH);
    TEST_ASSERT(diag.index == CO_401_INDEX_DIGITAL_INPUT_8);
    TEST_ASSERT(diag.subIndex == 1U);
    TEST_ASSERT(!device.odBound);
    return true;
}

static bool test_wrong_analog_stride_fails_closed(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;

    ioInit(&io);
    fixtureInit(&fixture, CO_401_CAP_ANALOG_INPUT);
    fixture.analogInputObject.dataElementSizeof = 1U;
    config = configForCapabilities(CO_401_CAP_ANALOG_INPUT, &io);

    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OD_LENGTH);
    TEST_ASSERT(diag.index == CO_401_INDEX_ANALOG_INPUT_16);
    TEST_ASSERT(diag.subIndex == 1U);
    TEST_ASSERT(!device.odBound);
    return true;
}

static bool test_wrong_access_fails_closed(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;

    ioInit(&io);
    fixtureInit(&fixture, CO_401_CAP_DIGITAL_OUTPUT);
    fixture.digitalOutputObject.attribute = ODA_SDO_RW | ODA_TPDO;
    config = configForCapabilities(CO_401_CAP_DIGITAL_OUTPUT, &io);

    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OD_ACCESS);
    TEST_ASSERT(diag.index == CO_401_INDEX_DIGITAL_OUTPUT_8);
    TEST_ASSERT(!device.odBound);
    return true;
}

static bool test_sub_index_count_mismatch_fails_closed(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;
    OD_entry_t *entry;

    ioInit(&io);
    fixtureInit(&fixture, CO_401_CAP_DIGITAL_INPUT);
    entry = fixtureFind(&fixture, CO_401_INDEX_DIGITAL_INPUT_8);
    TEST_ASSERT(entry != NULL);
    entry->subEntriesCount++;
    config = configForCapabilities(CO_401_CAP_DIGITAL_INPUT, &io);

    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OD_SUB_COUNT);
    TEST_ASSERT(diag.index == CO_401_INDEX_DIGITAL_INPUT_8);
    TEST_ASSERT(!device.odBound);
    return true;
}

static bool test_sub_index_zero_value_mismatch_fails_closed(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;

    ioInit(&io);
    fixtureInit(&fixture, CO_401_CAP_ANALOG_OUTPUT);
    fixture.analogOutputSub0 = TEST_AO_CHANNELS - 1U;
    config = configForCapabilities(CO_401_CAP_ANALOG_OUTPUT, &io);

    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OD_SUB_VALUE);
    TEST_ASSERT(diag.index == CO_401_INDEX_ANALOG_OUTPUT_16);
    TEST_ASSERT(!device.odBound);
    return true;
}

static bool test_object_1000_mismatch_fails_closed(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;

    ioInit(&io);
    fixtureInit(&fixture, CO_401_CAP_DIGITAL_INPUT);
    fixture.deviceType |= CO_401_DEVICE_TYPE_DEVICE_SPECIFIC_MAPPING;
    config = configForCapabilities(CO_401_CAP_DIGITAL_INPUT, &io);

    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_DEVICE_TYPE);
    TEST_ASSERT(diag.index == CO_401_INDEX_DEVICE_TYPE);
    TEST_ASSERT(!device.odBound);
    return true;
}

static bool test_disabled_capability_object_is_rejected(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;

    ioInit(&io);
    fixtureInit(&fixture, CO_401_CAP_DIGITAL_INPUT | CO_401_CAP_DIGITAL_OUTPUT);
    fixture.deviceType = CO_401_deviceTypeForCapabilities(CO_401_CAP_DIGITAL_INPUT);
    config = configForCapabilities(CO_401_CAP_DIGITAL_INPUT, &io);

    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OD_UNEXPECTED);
    TEST_ASSERT(diag.index == CO_401_INDEX_DIGITAL_OUTPUT_8);
    TEST_ASSERT(!device.odBound);
    return true;
}

static bool test_max_process_image_count_is_accepted(void)
{
    static uint8_t maxDigitalInputs[CO_401_PROCESS_IMAGE_COUNT_MAX];
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;
    OD_entry_t *entry;

    ioInit(&io);
    fixtureInit(&fixture, CO_401_CAP_DIGITAL_INPUT);
    fixture.digitalInputSub0 = CO_401_PROCESS_IMAGE_COUNT_MAX;
    fixture.digitalInputObject.dataOrig = maxDigitalInputs;
    entry = fixtureFind(&fixture, CO_401_INDEX_DIGITAL_INPUT_8);
    TEST_ASSERT(entry != NULL);
    entry->subEntriesCount = (uint8_t)(CO_401_PROCESS_IMAGE_COUNT_MAX + 1U);

    config = configForCapabilities(CO_401_CAP_DIGITAL_INPUT, &io);
    config.digitalInputBanks = CO_401_PROCESS_IMAGE_COUNT_MAX;

    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);
    TEST_ASSERT(device.odBound);
    TEST_ASSERT(device.config.digitalInputBanks == CO_401_PROCESS_IMAGE_COUNT_MAX);
    return true;
}

static bool test_invalid_config_and_missing_callback(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;
    CO_401_io_if_t incompleteIoIf = completeIoIf;

    ioInit(&io);
    fixtureInit(&fixture, CO_401_CAP_DIGITAL_INPUT);
    config = configForCapabilities(CO_401_CAP_DIGITAL_INPUT, &io);
    config.digitalInputBanks = 0xFFU;
    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_CONFIG);

    config = configForCapabilities(CO_401_CAP_DIGITAL_INPUT, &io);
    incompleteIoIf.readDigital8 = NULL;
    config.io = &incompleteIoIf;
    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_IO_IF);

    config = configForCapabilities(0U, &io);
    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_CONFIG);
    return true;
}

static bool test_invalid_reinit_fails_closed(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;

    ioInit(&io);
    fixtureInit(&fixture, CO_401_CAP_ALL);
    config = configForCapabilities(CO_401_CAP_ALL, &io);

    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);
    TEST_ASSERT(device.odBound);

    TEST_ASSERT(CO_401_device_init(&device, NULL, &config, &diag) == CO_401_INIT_BAD_ARGUMENT);
    TEST_ASSERT(!device.odBound);
    TEST_ASSERT(device.od == NULL);
    TEST_ASSERT(device.config.io == NULL);

    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);
    TEST_ASSERT(device.odBound);
    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, NULL, &diag) == CO_401_INIT_BAD_ARGUMENT);
    TEST_ASSERT(!device.odBound);
    TEST_ASSERT(device.od == NULL);
    TEST_ASSERT(device.config.io == NULL);

    return true;
}

static bool test_basic_process_image(void)
{
    const CO_401_capabilities_t capabilities = CO_401_CAP_ALL;
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;
    uint8_t digitalValue;
    int16_t analogValue;

    ioInit(&io);
    fixtureInit(&fixture, capabilities);
    config = configForCapabilities(capabilities, &io);

    io.digitalInputs[0] = 0x5AU;
    io.digitalInputs[1] = 0xA5U;
    io.analogInputs[0] = -1234;
    io.analogInputs[1] = 2345;
    io.analogInputs[2] = INT16_MIN;
    io.analogInputs[3] = INT16_MAX;

    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutput8, 1U, 0x33U, true) == ODR_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutput8, 2U, 0xCCU, true) == ODR_OK);
    TEST_ASSERT(OD_set_i16(device.bound.analogOutput16, 1U, -222, true) == ODR_OK);
    TEST_ASSERT(OD_set_i16(device.bound.analogOutput16, 2U, 333, true) == ODR_OK);
    TEST_ASSERT(OD_set_i16(device.bound.analogOutput16, 3U, INT16_MIN, true) == ODR_OK);
    TEST_ASSERT(OD_set_i16(device.bound.analogOutput16, 4U, INT16_MAX, true) == ODR_OK);

    CO_401_device_process(&device);

    TEST_ASSERT(io.digitalReadCalls == TEST_DI_BANKS);
    TEST_ASSERT(io.digitalWriteCalls == TEST_DO_BANKS);
    TEST_ASSERT(io.analogReadCalls == TEST_AI_CHANNELS);
    TEST_ASSERT(io.analogWriteCalls == TEST_AO_CHANNELS);
    TEST_ASSERT(io.digitalOutputs[0] == 0x33U);
    TEST_ASSERT(io.digitalOutputs[1] == 0xCCU);
    TEST_ASSERT(io.analogOutputs[0] == -222);
    TEST_ASSERT(io.analogOutputs[1] == 333);
    TEST_ASSERT(io.analogOutputs[2] == INT16_MIN);
    TEST_ASSERT(io.analogOutputs[3] == INT16_MAX);

    TEST_ASSERT(OD_get_u8(device.bound.digitalInput8, 1U, &digitalValue, true) == ODR_OK);
    TEST_ASSERT(digitalValue == 0x5AU);
    TEST_ASSERT(OD_get_u8(device.bound.digitalInput8, 2U, &digitalValue, true) == ODR_OK);
    TEST_ASSERT(digitalValue == 0xA5U);
    TEST_ASSERT(OD_get_i16(device.bound.analogInput16, 1U, &analogValue, true) == ODR_OK);
    TEST_ASSERT(analogValue == -1234);
    TEST_ASSERT(OD_get_i16(device.bound.analogInput16, 2U, &analogValue, true) == ODR_OK);
    TEST_ASSERT(analogValue == 2345);
    TEST_ASSERT(OD_get_i16(device.bound.analogInput16, 3U, &analogValue, true) == ODR_OK);
    TEST_ASSERT(analogValue == INT16_MIN);
    TEST_ASSERT(OD_get_i16(device.bound.analogInput16, 4U, &analogValue, true) == ODR_OK);
    TEST_ASSERT(analogValue == INT16_MAX);
    return true;
}

static bool test_failed_input_read_preserves_image(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;
    uint8_t digitalValue;
    int16_t analogValue;

    ioInit(&io);
    fixtureInit(&fixture, CO_401_CAP_DIGITAL_INPUT | CO_401_CAP_ANALOG_INPUT);
    config = configForCapabilities(CO_401_CAP_DIGITAL_INPUT | CO_401_CAP_ANALOG_INPUT, &io);
    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalInput8, 1U, 0x11U, true) == ODR_OK);
    TEST_ASSERT(OD_set_i16(device.bound.analogInput16, 1U, 123, true) == ODR_OK);

    io.digitalReadResult = CO_401_IO_BUSY;
    io.analogReadResult = CO_401_IO_ERROR;
    CO_401_device_process(&device);

    TEST_ASSERT(OD_get_u8(device.bound.digitalInput8, 1U, &digitalValue, true) == ODR_OK);
    TEST_ASSERT(digitalValue == 0x11U);
    TEST_ASSERT(OD_get_i16(device.bound.analogInput16, 1U, &analogValue, true) == ODR_OK);
    TEST_ASSERT(analogValue == 123);
    return true;
}

static bool test_disabled_capability_callbacks_are_optional(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;
    CO_401_io_if_t inputOnlyIoIf = {0};

    ioInit(&io);
    fixtureInit(&fixture, CO_401_CAP_DIGITAL_INPUT);
    inputOnlyIoIf.readDigital8 = readDigital8;
    config = configForCapabilities(CO_401_CAP_DIGITAL_INPUT, &io);
    config.io = &inputOnlyIoIf;

    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);
    TEST_ASSERT(device.odBound);
    return true;
}

static bool test_unbound_helpers_are_noop(void)
{
    CO_401_device_t device;
    test_io_t io;

    (void)memset(&device, 0, sizeof(device));
    ioInit(&io);
    device.config.io = &completeIoIf;
    device.config.ioObject = &io;
    device.config.digitalInputBanks = 1U;
    device.config.digitalOutputBanks = 1U;
    device.config.analogInputChannels = 1U;
    device.config.analogOutputChannels = 1U;

    CO_401_digital_refreshInputs(&device);
    CO_401_digital_applyOutputs(&device);
    CO_401_analog_refreshInputs(&device);
    CO_401_analog_applyOutputs(&device);

    TEST_ASSERT(io.digitalReadCalls == 0U);
    TEST_ASSERT(io.digitalWriteCalls == 0U);
    TEST_ASSERT(io.analogReadCalls == 0U);
    TEST_ASSERT(io.analogWriteCalls == 0U);
    return true;
}

static bool test_output_failure_keeps_command_for_later_pass(void)
{
    test_od_fixture_t fixture;
    test_io_t io;
    CO_401_device_t device;
    CO_401_init_diag_t diag;
    CO_401_device_config_t config;
    uint8_t digitalCommand;
    int16_t analogCommand;

    ioInit(&io);
    fixtureInit(&fixture, CO_401_CAP_DIGITAL_OUTPUT | CO_401_CAP_ANALOG_OUTPUT);
    config = configForCapabilities(CO_401_CAP_DIGITAL_OUTPUT | CO_401_CAP_ANALOG_OUTPUT, &io);
    TEST_ASSERT(CO_401_device_init(&device, &fixture.od, &config, &diag) == CO_401_INIT_OK);
    TEST_ASSERT(OD_set_u8(device.bound.digitalOutput8, 1U, 0x7EU, true) == ODR_OK);
    TEST_ASSERT(OD_set_i16(device.bound.analogOutput16, 1U, -777, true) == ODR_OK);

    io.digitalWriteResult = CO_401_IO_BUSY;
    io.analogWriteResult = CO_401_IO_ERROR;
    CO_401_device_process(&device);
    TEST_ASSERT(OD_get_u8(device.bound.digitalOutput8, 1U, &digitalCommand, true) == ODR_OK);
    TEST_ASSERT(digitalCommand == 0x7EU);
    TEST_ASSERT(OD_get_i16(device.bound.analogOutput16, 1U, &analogCommand, true) == ODR_OK);
    TEST_ASSERT(analogCommand == -777);

    io.digitalWriteResult = CO_401_IO_OK;
    io.analogWriteResult = CO_401_IO_OK;
    CO_401_device_process(&device);
    TEST_ASSERT(io.digitalOutputs[0] == 0x7EU);
    TEST_ASSERT(io.analogOutputs[0] == -777);
    TEST_ASSERT(io.digitalWriteCalls == TEST_DO_BANKS * 2U);
    TEST_ASSERT(io.analogWriteCalls == TEST_AO_CHANNELS * 2U);
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
        {"all-capability-combinations", test_all_capability_combinations},
        {"missing-object", test_missing_object_fails_closed},
        {"wrong-object-type", test_wrong_object_type_fails_closed},
        {"wrong-width", test_wrong_width_fails_closed},
        {"wrong-digital-stride", test_wrong_digital_stride_fails_closed},
        {"wrong-analog-stride", test_wrong_analog_stride_fails_closed},
        {"wrong-access", test_wrong_access_fails_closed},
        {"sub-index-count-mismatch", test_sub_index_count_mismatch_fails_closed},
        {"sub-index-zero-value-mismatch", test_sub_index_zero_value_mismatch_fails_closed},
        {"object-1000-mismatch", test_object_1000_mismatch_fails_closed},
        {"disabled-capability-object", test_disabled_capability_object_is_rejected},
        {"max-process-image-count", test_max_process_image_count_is_accepted},
        {"invalid-config-and-callback", test_invalid_config_and_missing_callback},
        {"invalid-reinit-fails-closed", test_invalid_reinit_fails_closed},
        {"basic-process-image", test_basic_process_image},
        {"failed-input-read-preserves-image", test_failed_input_read_preserves_image},
        {"disabled-capability-callbacks-optional", test_disabled_capability_callbacks_are_optional},
        {"unbound-helpers-noop", test_unbound_helpers_are_noop},
        {"output-failure-keeps-command", test_output_failure_keeps_command_for_later_pass},
    };
    size_t i;

    for (i = 0U; i < ARRAY_COUNT(tests); i++) {
        if (!tests[i].function()) {
            return 1;
        }
        printf("CIA401_DEVICE_HOST_CASE_PASS:%s\n", tests[i].name);
    }

    printf("CIA401_DEVICE_HOST_PASS:%zu/%zu\n", ARRAY_COUNT(tests), ARRAY_COUNT(tests));
    return 0;
}
