/**
 * @file profile-layout-host-test.c
 * @brief Host-only checks for the CiA 301 multi-logical-device profile layout helpers.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "CO_profile_layout.h"

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

#define TEST_ASSERT(expr)                                                                    \
    do {                                                                                     \
        if (!(expr)) {                                                                       \
            fprintf(stderr, "PROFILE_LAYOUT_HOST_FAIL:%s:%d:%s\n", __func__, __LINE__, #expr); \
            return false;                                                                    \
        }                                                                                    \
    } while (0)

static bool test_logical_device_zero_base(void)
{
    TEST_ASSERT(CO_profileIndex(0U, 0x6000U) == 0x6000U);
    return true;
}

static bool test_logical_device_one_base(void)
{
    TEST_ASSERT(CO_profileIndex(1U, 0x6000U) == 0x6800U);
    return true;
}

static bool test_logical_device_three_analog_index(void)
{
    TEST_ASSERT(CO_profileIndex(3U, 0x6401U) == 0x7C01U);
    return true;
}

static bool test_logical_device_four_base(void)
{
    TEST_ASSERT(CO_profileIndex(4U, 0x6000U) == 0x8000U);
    return true;
}

static bool test_logical_device_seven_bounds(void)
{
    TEST_ASSERT(CO_profileIndex(7U, 0x6000U) == 0x9800U);
    TEST_ASSERT(CO_profileIndex(7U, 0x67FFU) == 0x9FFFU);
    return true;
}

static bool test_invalid_logical_device_fails_closed(void)
{
    TEST_ASSERT(!CO_profileLogicalDeviceValid(8U));
    TEST_ASSERT(CO_profileIndex(8U, 0x6000U) == CO_PROFILE_INDEX_INVALID);
    TEST_ASSERT(CO_profileDeviceTypeIndex(8U) == CO_PROFILE_INDEX_INVALID);
    TEST_ASSERT(CO_profilePdoNumber(8U, 1U) == CO_PROFILE_PDO_NUMBER_INVALID);
    return true;
}

static bool test_device_type_slot_calculation(void)
{
    TEST_ASSERT(CO_profileDeviceTypeIndex(0U) == 0x67FFU);
    TEST_ASSERT(CO_profileDeviceTypeIndex(1U) == 0x6FFFU);
    TEST_ASSERT(CO_profileDeviceTypeIndex(3U) == 0x7FFFU);
    TEST_ASSERT(CO_profileDeviceTypeIndex(7U) == 0x9FFFU);
    return true;
}

static bool test_pdo_number_translation(void)
{
    TEST_ASSERT(CO_profilePdoNumber(0U, 1U) == 1U);
    TEST_ASSERT(CO_profilePdoNumber(0U, 64U) == 64U);
    TEST_ASSERT(CO_profilePdoNumber(1U, 1U) == 65U);
    TEST_ASSERT(CO_profilePdoNumber(3U, 1U) == 193U);
    TEST_ASSERT(CO_profilePdoNumber(7U, 64U) == 512U);
    TEST_ASSERT(CO_profilePdoNumber(0U, 0U) == CO_PROFILE_PDO_NUMBER_INVALID);
    TEST_ASSERT(CO_profilePdoNumber(0U, 65U) == CO_PROFILE_PDO_NUMBER_INVALID);
    return true;
}

static bool test_invalid_canonical_index_fails_closed(void)
{
    TEST_ASSERT(!CO_profileCanonicalIndexValid(0x5FFFU));
    TEST_ASSERT(!CO_profileCanonicalIndexValid(0x6800U));
    TEST_ASSERT(CO_profileIndex(0U, 0x5FFFU) == CO_PROFILE_INDEX_INVALID);
    TEST_ASSERT(CO_profileIndex(0U, 0x6800U) == CO_PROFILE_INDEX_INVALID);
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
        {"logical-device-zero-base", test_logical_device_zero_base},
        {"logical-device-one-base", test_logical_device_one_base},
        {"logical-device-three-analog-index", test_logical_device_three_analog_index},
        {"logical-device-four-base", test_logical_device_four_base},
        {"logical-device-seven-bounds", test_logical_device_seven_bounds},
        {"invalid-logical-device", test_invalid_logical_device_fails_closed},
        {"device-type-slot", test_device_type_slot_calculation},
        {"pdo-number-translation", test_pdo_number_translation},
        {"invalid-canonical-index", test_invalid_canonical_index_fails_closed},
    };
    size_t i;

    for (i = 0U; i < ARRAY_COUNT(tests); i++) {
        if (!tests[i].function()) {
            return 1;
        }
        printf("PROFILE_LAYOUT_HOST_CASE_PASS:%s\n", tests[i].name);
    }

    printf("PROFILE_LAYOUT_HOST_PASS:%zu/%zu\n", ARRAY_COUNT(tests), ARRAY_COUNT(tests));
    return 0;
}
