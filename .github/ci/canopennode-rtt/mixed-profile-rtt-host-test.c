/**
 * @file mixed-profile-rtt-host-test.c
 * @brief Host contract checks for the profile-neutral mixed-profile RT-Thread lifecycle adapter.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OD_DEFINITION
#include "CO_profile_mixed_RTT.h"
#include "CO_app_RTT.h"
#include "CO_lifecycle_RTT.h"

#define TEST_ASSERT(expr)                                                                   \
    do {                                                                                    \
        if (!(expr)) {                                                                      \
            fprintf(stderr, "MIXED_PROFILE_RTT_HOST_FAIL:%s:%d:%s\n", __func__, __LINE__, \
                    #expr);                                                                 \
            return false;                                                                   \
        }                                                                                   \
    } while (0)

static const CO_RTT_lifecycle_ops_t *registeredOps;
static void *registeredContext;
static CO_RTT_lifecycle_context_release_t registeredRelease;
static unsigned allocCount;
static unsigned freeCount;
static bool failNextAlloc;
static rt_err_t lifecycleRegisterResult = RT_EOK;

typedef struct {
    uint32_t values[CO_PROFILE_LOGICAL_DEVICE_COUNT_MAX];
    OD_obj_var_t objects[CO_PROFILE_LOGICAL_DEVICE_COUNT_MAX];
    OD_entry_t entries[CO_PROFILE_LOGICAL_DEVICE_COUNT_MAX];
    OD_t od;
} test_od_t;

rt_err_t CO_RTT_lifecycleRegisterEx(CANopenNodeRTT *app, const CO_RTT_lifecycle_ops_t *ops, void *context,
                                    CO_RTT_lifecycle_context_release_t release)
{
    if (app == NULL || ops == NULL || context == NULL || release == NULL || registeredOps != NULL) {
        return -RT_EBUSY;
    }
    if (lifecycleRegisterResult != RT_EOK) {
        return lifecycleRegisterResult;
    }
    registeredOps = ops;
    registeredContext = context;
    registeredRelease = release;
    return RT_EOK;
}

void *rt_calloc(rt_size_t count, rt_size_t size)
{
    void *ptr;

    if (failNextAlloc) {
        failNextAlloc = false;
        return NULL;
    }

    ptr = calloc(count, size);
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

static void releaseCapturedContext(void)
{
    if (registeredRelease != NULL && registeredContext != NULL) {
        registeredRelease(registeredContext);
    }
    registeredOps = NULL;
    registeredContext = NULL;
    registeredRelease = NULL;
}

static void fixtureInit(test_od_t *fixture, const CO_profile_descriptor_t *descriptors, uint8_t descriptorCount)
{
    uint8_t slot;
    uint8_t entryIndex = 0U;

    (void)memset(fixture, 0, sizeof(*fixture));
    fixture->od.list = fixture->entries;
    fixture->od.size = descriptorCount;

    /* OD_find() uses binary search, so Device Type entries must be emitted in index order. */
    for (slot = 0U; slot < CO_PROFILE_LOGICAL_DEVICE_COUNT_MAX; slot++) {
        uint8_t descriptorIndex;

        for (descriptorIndex = 0U; descriptorIndex < descriptorCount; descriptorIndex++) {
            const CO_profile_descriptor_t *descriptor = &descriptors[descriptorIndex];

            if (descriptor->logicalDevice != slot) {
                continue;
            }

            fixture->values[entryIndex] = descriptor->profileNumber;
            fixture->objects[entryIndex].dataOrig = &fixture->values[entryIndex];
            fixture->objects[entryIndex].attribute = ODA_SDO_R | ODA_MB;
            fixture->objects[entryIndex].dataLength = sizeof(fixture->values[entryIndex]);
            fixture->entries[entryIndex].index = CO_profileDeviceTypeIndex(descriptor->logicalDevice);
            fixture->entries[entryIndex].subEntriesCount = 1U;
            fixture->entries[entryIndex].odObjectType = ODT_VAR;
            fixture->entries[entryIndex].odObject = &fixture->objects[entryIndex];
            entryIndex++;
        }
    }
}

static bool test_product_descriptor_table_is_not_demo_specific(void)
{
    CANopenNodeRTT app = {0};
    CO_profile_descriptor_t descriptors[] = {
        {5U, 401U},
        {1U, 402U},
    };
    const CO_profile_mixed_RTT_config_t config = {
        .descriptors = descriptors,
        .descriptorCount = (uint8_t)(sizeof(descriptors) / sizeof(descriptors[0])),
    };
    test_od_t fixture;

    fixtureInit(&fixture, descriptors, config.descriptorCount);
    TEST_ASSERT(CO_profileMixedRTTAttach(&app, &config) == RT_EOK);
    TEST_ASSERT(registeredOps != NULL);
    TEST_ASSERT(registeredRelease != NULL);

    /* Attach owns a copy, so product/demo configuration storage may be released or reused afterwards. */
    (void)memset(descriptors, 0, sizeof(descriptors));
    TEST_ASSERT(registeredOps->communicationBind(&app, NULL, &fixture.od, registeredContext) == RT_EOK);
    registeredOps->communicationQuiesced(&app, registeredContext);
    releaseCapturedContext();
    return true;
}

static bool test_device_type_mismatch_fails_closed(void)
{
    CANopenNodeRTT app = {0};
    const CO_profile_descriptor_t descriptors[] = {
        {2U, 402U},
        {6U, 401U},
    };
    const CO_profile_mixed_RTT_config_t config = {
        .descriptors = descriptors,
        .descriptorCount = (uint8_t)(sizeof(descriptors) / sizeof(descriptors[0])),
    };
    test_od_t fixture;

    fixtureInit(&fixture, descriptors, config.descriptorCount);
    TEST_ASSERT(CO_profileMixedRTTAttach(&app, &config) == RT_EOK);
    fixture.values[1] = 402U;
    TEST_ASSERT(registeredOps->communicationBind(&app, NULL, &fixture.od, registeredContext) == -RT_ERROR);

    fixture.values[1] = 401U;
    TEST_ASSERT(registeredOps->communicationBind(&app, NULL, &fixture.od, registeredContext) == RT_EOK);
    registeredOps->communicationQuiesced(&app, registeredContext);
    releaseCapturedContext();
    return true;
}

static bool test_empty_descriptor_table_is_rejected(void)
{
    CANopenNodeRTT app = {0};
    const CO_profile_mixed_RTT_config_t config = {
        .descriptors = NULL,
        .descriptorCount = 0U,
    };
    unsigned beforeAlloc = allocCount;

    TEST_ASSERT(CO_profileMixedRTTAttach(&app, &config) == -RT_EINVAL);
    TEST_ASSERT(registeredOps == NULL);
    TEST_ASSERT(allocCount == beforeAlloc);
    return true;
}

static bool test_invalid_descriptor_config_is_rejected_before_allocation(void)
{
    CANopenNodeRTT app = {0};
    const CO_profile_descriptor_t descriptor = {0U, 402U};
    const CO_profile_mixed_RTT_config_t oversized = {
        .descriptors = &descriptor,
        .descriptorCount = (uint8_t)(CO_PROFILE_LOGICAL_DEVICE_COUNT_MAX + 1U),
    };
    const CO_profile_mixed_RTT_config_t missingTable = {
        .descriptors = NULL,
        .descriptorCount = 1U,
    };
    unsigned beforeAlloc = allocCount;

    TEST_ASSERT(CO_profileMixedRTTAttach(&app, &oversized) == -RT_EINVAL);
    TEST_ASSERT(CO_profileMixedRTTAttach(&app, &missingTable) == -RT_EINVAL);
    TEST_ASSERT(registeredOps == NULL);
    TEST_ASSERT(allocCount == beforeAlloc);
    return true;
}


static bool test_allocation_failure_is_reported_without_registration(void)
{
    CANopenNodeRTT app = {0};
    const CO_profile_descriptor_t descriptor = {0U, 402U};
    const CO_profile_mixed_RTT_config_t config = {
        .descriptors = &descriptor,
        .descriptorCount = 1U,
    };
    unsigned beforeAlloc = allocCount;

    failNextAlloc = true;
    TEST_ASSERT(CO_profileMixedRTTAttach(&app, &config) == -RT_ENOMEM);
    TEST_ASSERT(registeredOps == NULL);
    TEST_ASSERT(allocCount == beforeAlloc);
    return true;
}

static bool test_registration_failure_releases_context_and_preserves_error(void)
{
    CANopenNodeRTT app = {0};
    const CO_profile_descriptor_t descriptor = {4U, 401U};
    const CO_profile_mixed_RTT_config_t config = {
        .descriptors = &descriptor,
        .descriptorCount = 1U,
    };
    unsigned beforeAlloc = allocCount;
    unsigned beforeFree = freeCount;

    lifecycleRegisterResult = -RT_EBUSY;
    TEST_ASSERT(CO_profileMixedRTTAttach(&app, &config) == -RT_EBUSY);
    lifecycleRegisterResult = RT_EOK;
    TEST_ASSERT(registeredOps == NULL);
    TEST_ASSERT(allocCount == beforeAlloc + 1U);
    TEST_ASSERT(freeCount == beforeFree + 1U);
    return true;
}

struct test_case {
    const char *name;
    bool (*run)(void);
};

int main(void)
{
    static const struct test_case cases[] = {
        {"product-descriptor-table-not-demo-specific", test_product_descriptor_table_is_not_demo_specific},
        {"device-type-mismatch-fails-closed", test_device_type_mismatch_fails_closed},
        {"empty-descriptor-table-rejected", test_empty_descriptor_table_is_rejected},
        {"invalid-config-rejected-before-allocation", test_invalid_descriptor_config_is_rejected_before_allocation},
        {"allocation-failure-reported", test_allocation_failure_is_reported_without_registration},
        {"registration-failure-releases-context", test_registration_failure_releases_context_and_preserves_error},
    };
    size_t i;

    for (i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++) {
        if (!cases[i].run()) {
            releaseCapturedContext();
            return 1;
        }
        printf("MIXED_PROFILE_RTT_HOST_CASE_PASS:%s\n", cases[i].name);
    }

    releaseCapturedContext();
    if (allocCount != freeCount) {
        fprintf(stderr, "MIXED_PROFILE_RTT_HOST_FAIL:allocation-balance:%u:%u\n", allocCount, freeCount);
        return 1;
    }

    printf("MIXED_PROFILE_RTT_HOST_PASS:%u/%u\n",
           (unsigned int)(sizeof(cases) / sizeof(cases[0])),
           (unsigned int)(sizeof(cases) / sizeof(cases[0])));
    return 0;
}
