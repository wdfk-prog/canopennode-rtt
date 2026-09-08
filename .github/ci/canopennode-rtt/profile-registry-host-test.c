/**
 * @file profile-registry-host-test.c
 * @brief Host-only ownership and fail-closed checks for the common CANopen profile registry.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "CO_profile_registry.h"

#define TEST_ASSERT(expr)                                                                  \
    do {                                                                                   \
        if (!(expr)) {                                                                     \
            fprintf(stderr, "PROFILE_REGISTRY_HOST_FAIL:%s:%d:%s\n", __func__, __LINE__, \
                    #expr);                                                               \
            return false;                                                                  \
        }                                                                                  \
    } while (0)

typedef struct {
    bool present[CO_PROFILE_LOGICAL_DEVICE_COUNT_MAX];
    uint32_t deviceType[CO_PROFILE_LOGICAL_DEVICE_COUNT_MAX];
} test_device_types_t;

static bool readDeviceType(void *object, uint8_t logicalDevice, uint32_t *deviceType)
{
    test_device_types_t *types = object;

    if (types == NULL || deviceType == NULL || !CO_profileLogicalDeviceValid(logicalDevice)
        || !types->present[logicalDevice]) {
        return false;
    }
    *deviceType = types->deviceType[logicalDevice];
    return true;
}

static void setType(test_device_types_t *types, uint8_t logicalDevice, uint32_t deviceType)
{
    types->present[logicalDevice] = true;
    types->deviceType[logicalDevice] = deviceType;
}

static bool test_401_402_non_overlap(void)
{
    const CO_profile_descriptor_t descriptors[] = {{0U, 402U}, {3U, 401U}};
    test_device_types_t types = {0};
    CO_profile_registry_t registry;

    setType(&types, 0U, 0x00000192UL);
    setType(&types, 3U, 0x000F0191UL);
    TEST_ASSERT(CO_profileRegistryBuild(&registry, descriptors, 2U, readDeviceType, &types, NULL)
                == CO_PROFILE_REGISTRY_OK);
    TEST_ASSERT(registry.count == 2U);
    TEST_ASSERT(CO_profileRegistryOwner(&registry, 0U) == 402U);
    TEST_ASSERT(CO_profileRegistryOwner(&registry, 3U) == 401U);
    return true;
}

static bool test_401_402_same_slot_fails_closed(void)
{
    const CO_profile_descriptor_t descriptors[] = {{3U, 401U}, {3U, 402U}};
    test_device_types_t types = {0};
    CO_profile_registry_t registry;
    CO_profile_registry_diag_t diag;

    (void)memset(&registry, 0xA5, sizeof(registry));
    setType(&types, 3U, 0x000F0191UL);
    TEST_ASSERT(CO_profileRegistryBuild(&registry, descriptors, 2U, readDeviceType, &types, &diag)
                == CO_PROFILE_REGISTRY_DUPLICATE_SLOT);
    TEST_ASSERT(diag.descriptorIndex == 1U);
    TEST_ASSERT(diag.logicalDevice == 3U);
    TEST_ASSERT(registry.count == 0U);
    TEST_ASSERT(CO_profileRegistryOwner(&registry, 3U) == 0U);
    return true;
}

static bool test_three_402_one_401(void)
{
    const CO_profile_descriptor_t descriptors[] = {{0U, 402U}, {1U, 402U}, {2U, 402U}, {3U, 401U}};
    test_device_types_t types = {0};
    CO_profile_registry_t registry;
    uint8_t i;

    for (i = 0U; i < 3U; i++) {
        setType(&types, i, 0x00000192UL);
    }
    setType(&types, 3U, 0x000F0191UL);
    TEST_ASSERT(CO_profileRegistryBuild(&registry, descriptors, 4U, readDeviceType, &types, NULL)
                == CO_PROFILE_REGISTRY_OK);
    TEST_ASSERT(registry.count == 4U);
    TEST_ASSERT(CO_profileRegistryOwner(&registry, 0U) == 402U);
    TEST_ASSERT(CO_profileRegistryOwner(&registry, 1U) == 402U);
    TEST_ASSERT(CO_profileRegistryOwner(&registry, 2U) == 402U);
    TEST_ASSERT(CO_profileRegistryOwner(&registry, 3U) == 401U);
    return true;
}

static bool test_invalid_logical_device_fails_closed(void)
{
    const CO_profile_descriptor_t descriptors[] = {{8U, 402U}};
    test_device_types_t types = {0};
    CO_profile_registry_t registry;
    CO_profile_registry_diag_t diag;

    (void)memset(&registry, 0xA5, sizeof(registry));
    TEST_ASSERT(CO_profileRegistryBuild(&registry, descriptors, 1U, readDeviceType, &types, &diag)
                == CO_PROFILE_REGISTRY_INVALID_LOGICAL_DEVICE);
    TEST_ASSERT(diag.logicalDevice == 8U);
    TEST_ASSERT(registry.count == 0U);
    return true;
}

static bool test_registration_order_independent(void)
{
    const CO_profile_descriptor_t forward[] = {{0U, 402U}, {1U, 402U}, {2U, 402U}, {3U, 401U}};
    const CO_profile_descriptor_t reverse[] = {{3U, 401U}, {2U, 402U}, {1U, 402U}, {0U, 402U}};
    test_device_types_t types = {0};
    CO_profile_registry_t first;
    CO_profile_registry_t second;
    uint8_t i;

    for (i = 0U; i < 3U; i++) {
        setType(&types, i, 0x00000192UL);
    }
    setType(&types, 3U, 0x000F0191UL);
    TEST_ASSERT(CO_profileRegistryBuild(&first, forward, 4U, readDeviceType, &types, NULL)
                == CO_PROFILE_REGISTRY_OK);
    TEST_ASSERT(CO_profileRegistryBuild(&second, reverse, 4U, readDeviceType, &types, NULL)
                == CO_PROFILE_REGISTRY_OK);
    TEST_ASSERT(memcmp(&first, &second, sizeof(first)) == 0);
    return true;
}

static bool test_device_type_mismatch_fails_closed(void)
{
    const CO_profile_descriptor_t descriptors[] = {{0U, 402U}, {3U, 401U}};
    test_device_types_t types = {0};
    CO_profile_registry_t registry;
    CO_profile_registry_diag_t diag;

    setType(&types, 0U, 0x00000192UL);
    setType(&types, 3U, 0x00000192UL);
    TEST_ASSERT(CO_profileRegistryBuild(&registry, descriptors, 2U, readDeviceType, &types, &diag)
                == CO_PROFILE_REGISTRY_DEVICE_TYPE_MISMATCH);
    TEST_ASSERT(diag.descriptorIndex == 1U);
    TEST_ASSERT(diag.profileNumber == 401U);
    TEST_ASSERT(diag.deviceType == 0x00000192UL);
    TEST_ASSERT(registry.count == 0U);
    return true;
}

static bool test_missing_device_type_fails_closed(void)
{
    const CO_profile_descriptor_t descriptors[] = {{0U, 402U}};
    test_device_types_t types = {0};
    CO_profile_registry_t registry;

    TEST_ASSERT(CO_profileRegistryBuild(&registry, descriptors, 1U, readDeviceType, &types, NULL)
                == CO_PROFILE_REGISTRY_DEVICE_TYPE_UNAVAILABLE);
    TEST_ASSERT(registry.count == 0U);
    return true;
}

static bool test_zero_profile_rejected(void)
{
    const CO_profile_descriptor_t descriptors[] = {{0U, 0U}};
    test_device_types_t types = {0};
    CO_profile_registry_t registry;

    setType(&types, 0U, 0U);
    TEST_ASSERT(CO_profileRegistryBuild(&registry, descriptors, 1U, readDeviceType, &types, NULL)
                == CO_PROFILE_REGISTRY_INVALID_PROFILE_NUMBER);
    TEST_ASSERT(registry.count == 0U);
    return true;
}

struct test_case {
    const char *name;
    bool (*run)(void);
};

int main(void)
{
    static const struct test_case cases[] = {
        {"401-402-non-overlap", test_401_402_non_overlap},
        {"401-402-same-slot-fails-closed", test_401_402_same_slot_fails_closed},
        {"three-402-one-401", test_three_402_one_401},
        {"invalid-logical-device-fails-closed", test_invalid_logical_device_fails_closed},
        {"registration-order-independent", test_registration_order_independent},
        {"device-type-mismatch-fails-closed", test_device_type_mismatch_fails_closed},
        {"missing-device-type-fails-closed", test_missing_device_type_fails_closed},
        {"zero-profile-rejected", test_zero_profile_rejected},
    };
    size_t i;

    for (i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++) {
        if (!cases[i].run()) {
            return 1;
        }
        printf("PROFILE_REGISTRY_HOST_CASE_PASS:%s\n", cases[i].name);
    }

    printf("PROFILE_REGISTRY_HOST_PASS:%u/%u\n",
           (unsigned int)(sizeof(cases) / sizeof(cases[0])),
           (unsigned int)(sizeof(cases) / sizeof(cases[0])));
    return 0;
}
