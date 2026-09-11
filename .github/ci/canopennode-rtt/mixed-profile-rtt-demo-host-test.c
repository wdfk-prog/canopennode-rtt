/**
 * @file mixed-profile-rtt-demo-host-test.c
 * @brief Host contract checks for the package-specific mixed-demo factory glue.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "CO_profile_mixed_RTT.h"
#include "CO_profile_mixed_demo.h"
#include "CO_lifecycle_RTT.h"

#define TEST_ASSERT(expr)                                                                        \
    do {                                                                                         \
        if (!(expr)) {                                                                           \
            fprintf(stderr, "MIXED_PROFILE_RTT_DEMO_HOST_FAIL:%s:%d:%s\n", __func__, __LINE__, \
                    #expr);                                                                      \
            return false;                                                                        \
        }                                                                                        \
    } while (0)

extern int (*const registerFactory_export)(void);

static const CO_RTT_lifecycle_factory_t *registeredFactory;
static CANopenNodeRTT *capturedApp;
static const CO_profile_mixed_RTT_config_t *capturedConfig;

rt_err_t CO_RTT_lifecycleFactoryRegister(const CO_RTT_lifecycle_factory_t *factory)
{
    if (factory == NULL || registeredFactory != NULL) {
        return -RT_EBUSY;
    }
    registeredFactory = factory;
    return RT_EOK;
}

rt_err_t CO_profileMixedRTTAttach(CANopenNodeRTT *app, const CO_profile_mixed_RTT_config_t *config)
{
    capturedApp = app;
    capturedConfig = config;
    return RT_EOK;
}

static bool test_demo_factory_supplies_only_demo_configuration(void)
{
    CANopenNodeRTT *app = (CANopenNodeRTT *)(uintptr_t)1U;

    TEST_ASSERT(registerFactory_export != NULL);
    TEST_ASSERT(registerFactory_export() == RT_EOK);
    TEST_ASSERT(registeredFactory != NULL);
    TEST_ASSERT(registeredFactory->order == 300U);
    TEST_ASSERT(registeredFactory->order < 401U);
    TEST_ASSERT(registeredFactory->order < 402U);
    TEST_ASSERT(registeredFactory->context != NULL);

    TEST_ASSERT(registeredFactory->attach(app, registeredFactory->context) == RT_EOK);
    TEST_ASSERT(capturedApp == app);
    TEST_ASSERT(capturedConfig == registeredFactory->context);
    TEST_ASSERT(capturedConfig->descriptors == CO_profileMixedDemoDescriptors);
    TEST_ASSERT(capturedConfig->descriptorCount == CO_PROFILE_MIXED_DEMO_DESCRIPTOR_COUNT);
    return true;
}

int main(void)
{
    if (!test_demo_factory_supplies_only_demo_configuration()) {
        return 1;
    }

    printf("MIXED_PROFILE_RTT_DEMO_HOST_CASE_PASS:demo-factory-supplies-only-demo-configuration\n");
    printf("MIXED_PROFILE_RTT_DEMO_HOST_PASS:1/1\n");
    return 0;
}
