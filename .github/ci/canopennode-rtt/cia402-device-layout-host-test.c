/**
 * @file cia402-device-layout-host-test.c
 * @brief Host-only logical-device layout regression checks for the Pure-C CiA 402 Device core.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define OD_DEFINITION
#include "CO_402_device.h"

#define TEST_AXIS_MAX 3U
#define TEST_BASE_OBJECT_COUNT 10U
#define TEST_ENTRY_MAX (TEST_AXIS_MAX * TEST_BASE_OBJECT_COUNT)

#define TEST_ASSERT(expr)                                                                \
    do {                                                                                 \
        if (!(expr)) {                                                                   \
            fprintf(stderr, "CIA402_DEVICE_LAYOUT_HOST_FAIL:%s:%d:%s\n", __func__,    \
                    __LINE__, #expr);                                                    \
            return false;                                                                \
        }                                                                                \
    } while (0)

typedef struct {
    uint16_t canonicalIndex;
    OD_size_t length;
    OD_attr_t attribute;
} test_object_contract_t;

static const test_object_contract_t baseObjects[TEST_BASE_OBJECT_COUNT] = {
    {CO_402_INDEX_ERROR_CODE, 2U, ODA_SDO_R | ODA_MB},
    {CO_402_INDEX_CONTROLWORD, 2U, ODA_SDO_RW | ODA_RPDO | ODA_MB},
    {CO_402_INDEX_STATUSWORD, 2U, ODA_SDO_R | ODA_TPDO | ODA_MB},
    {CO_402_INDEX_MODES_OF_OPERATION, 1U, ODA_SDO_RW | ODA_RPDO},
    {CO_402_INDEX_MODES_OF_OPERATION_DISPLAY, 1U, ODA_SDO_R | ODA_TPDO},
    {CO_402_INDEX_POSITION_ACTUAL_VALUE, 4U, ODA_SDO_R | ODA_TPDO | ODA_MB},
    {CO_402_INDEX_VELOCITY_ACTUAL_VALUE, 4U, ODA_SDO_R | ODA_TPDO | ODA_MB},
    {CO_402_INDEX_TARGET_POSITION, 4U, ODA_SDO_RW | ODA_RPDO | ODA_MB},
    {CO_402_INDEX_TARGET_VELOCITY, 4U, ODA_SDO_RW | ODA_RPDO | ODA_MB},
    {CO_402_INDEX_SUPPORTED_DRIVE_MODES, 4U, ODA_SDO_R | ODA_MB},
};

typedef struct {
    uint32_t data[TEST_ENTRY_MAX];
    OD_obj_var_t objects[TEST_ENTRY_MAX];
    OD_entry_t entries[TEST_ENTRY_MAX];
    OD_t od;
} test_od_fixture_t;

static CO_402_drive_result_t driveDone(void *object)
{
    (void)object;
    return CO_402_DRIVE_DONE;
}

static const CO_402_drive_if_t completeDrive = {
    .shutdown = driveDone,
    .switchOn = driveDone,
    .enableOperation = driveDone,
    .disableOperation = driveDone,
    .quickStop = driveDone,
    .faultReaction = driveDone,
    .faultReset = driveDone,
    .disableVoltage = driveDone,
};

static void fixtureInit(test_od_fixture_t *fixture, const uint8_t *logicalDevices, uint8_t axisCount)
{
    uint8_t axisIndex;
    uint8_t objectIndex;
    uint16_t entryIndex = 0U;

    (void)memset(fixture, 0, sizeof(*fixture));
    fixture->od.list = fixture->entries;

    for (axisIndex = 0U; axisIndex < axisCount; axisIndex++) {
        for (objectIndex = 0U; objectIndex < TEST_BASE_OBJECT_COUNT; objectIndex++) {
            const test_object_contract_t *contract = &baseObjects[objectIndex];
            OD_obj_var_t *object = &fixture->objects[entryIndex];
            OD_entry_t *entry = &fixture->entries[entryIndex];

            object->dataOrig = &fixture->data[entryIndex];
            object->attribute = contract->attribute;
            object->dataLength = contract->length;

            entry->index = CO_profileIndex(logicalDevices[axisIndex], contract->canonicalIndex);
            entry->subEntriesCount = 1U;
            entry->odObjectType = ODT_VAR;
            entry->odObject = object;
            entry->extension = NULL;
            entryIndex++;
        }
    }

    fixture->od.size = entryIndex;
}

static CO_402_device_axis_config_t axisConfig(uint8_t logicalDevice)
{
    CO_402_device_axis_config_t config;

    (void)memset(&config, 0, sizeof(config));
    config.logicalDevice = logicalDevice;
    config.drive = &completeDrive;
    return config;
}

static bool test_ld0_ld1_ld2_layout(void)
{
    static const uint8_t logicalDevices[] = {0U, 1U, 2U};
    test_od_fixture_t fixture;
    CO_402_device_manager_t manager;
    CO_402_device_axis_t axes[3];
    CO_402_device_axis_config_t configs[3];
    CO_402_init_diag_t diag;
    uint8_t i;

    fixtureInit(&fixture, logicalDevices, 3U);
    for (i = 0U; i < 3U; i++) {
        configs[i] = axisConfig(logicalDevices[i]);
    }

    TEST_ASSERT(CO_402_device_managerInit(&manager, &fixture.od, axes, configs, 3U, &diag) == CO_402_INIT_OK);
    TEST_ASSERT(manager.odBound);
    TEST_ASSERT(axes[0].odBase == 0x6000U);
    TEST_ASSERT(axes[1].odBase == 0x6800U);
    TEST_ASSERT(axes[2].odBase == 0x7000U);
    TEST_ASSERT(OD_getIndex(axes[1].od.controlword) == 0x6840U);
    TEST_ASSERT(OD_getIndex(axes[2].od.supportedDriveModes) == 0x7502U);
    return true;
}

static bool test_non_contiguous_logical_devices(void)
{
    static const uint8_t logicalDevices[] = {0U, 2U};
    test_od_fixture_t fixture;
    CO_402_device_manager_t manager;
    CO_402_device_axis_t axes[2];
    CO_402_device_axis_config_t configs[2] = {axisConfig(0U), axisConfig(2U)};
    CO_402_init_diag_t diag;

    fixtureInit(&fixture, logicalDevices, 2U);
    TEST_ASSERT(CO_402_device_managerInit(&manager, &fixture.od, axes, configs, 2U, &diag) == CO_402_INIT_OK);
    TEST_ASSERT(manager.odBound);
    TEST_ASSERT(axes[0].odBase == 0x6000U);
    TEST_ASSERT(axes[1].odBase == 0x7000U);
    TEST_ASSERT(OD_getIndex(axes[1].od.controlword) == CO_profileIndex(2U, CO_402_INDEX_CONTROLWORD));
    return true;
}

static bool test_duplicate_logical_device_rejected(void)
{
    test_od_fixture_t fixture;
    CO_402_device_manager_t manager;
    CO_402_device_axis_t axes[2];
    CO_402_device_axis_config_t configs[2] = {axisConfig(1U), axisConfig(1U)};
    CO_402_init_diag_t diag;

    (void)memset(&fixture, 0, sizeof(fixture));
    TEST_ASSERT(CO_402_device_managerInit(&manager, &fixture.od, axes, configs, 2U, &diag)
                == CO_402_INIT_DUPLICATE_AXIS);
    TEST_ASSERT(diag.logicalDevice == 1U);
    TEST_ASSERT(diag.index == 0U);
    return true;
}

static bool test_logical_device_eight_rejected(void)
{
    test_od_fixture_t fixture;
    CO_402_device_manager_t manager;
    CO_402_device_axis_t axis;
    CO_402_device_axis_config_t config = axisConfig(8U);
    CO_402_init_diag_t diag;

    (void)memset(&fixture, 0, sizeof(fixture));
    TEST_ASSERT(CO_402_device_managerInit(&manager, &fixture.od, &axis, &config, 1U, &diag)
                == CO_402_INIT_BAD_AXIS);
    TEST_ASSERT(diag.logicalDevice == 8U);
    TEST_ASSERT(diag.index == 0U);
    return true;
}

static bool test_logical_device_seven_upper_boundary(void)
{
    static const uint8_t logicalDevices[] = {7U};
    test_od_fixture_t fixture;
    CO_402_device_manager_t manager;
    CO_402_device_axis_t axis;
    CO_402_device_axis_config_t config = axisConfig(7U);
    CO_402_init_diag_t diag;

    fixtureInit(&fixture, logicalDevices, 1U);
    TEST_ASSERT(CO_402_device_managerInit(&manager, &fixture.od, &axis, &config, 1U, &diag) == CO_402_INIT_OK);
    TEST_ASSERT(axis.odBase == 0x9800U);
    TEST_ASSERT(OD_getIndex(axis.od.controlword) == 0x9840U);
    TEST_ASSERT(OD_getIndex(axis.od.supportedDriveModes) == 0x9D02U);
    return true;
}

struct test_case {
    const char *name;
    bool (*run)(void);
};

int main(void)
{
    static const struct test_case cases[] = {
        {"ld0-ld1-ld2-layout", test_ld0_ld1_ld2_layout},
        {"non-contiguous-logical-devices", test_non_contiguous_logical_devices},
        {"duplicate-logical-device-rejected", test_duplicate_logical_device_rejected},
        {"logical-device-eight-rejected", test_logical_device_eight_rejected},
        {"logical-device-seven-upper-boundary", test_logical_device_seven_upper_boundary},
    };
    size_t i;

    for (i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++) {
        if (!cases[i].run()) {
            return 1;
        }
        printf("CIA402_DEVICE_LAYOUT_HOST_CASE_PASS:%s\n", cases[i].name);
    }

    printf("CIA402_DEVICE_LAYOUT_HOST_PASS:%u\n", (unsigned int)(sizeof(cases) / sizeof(cases[0])));
    return 0;
}
