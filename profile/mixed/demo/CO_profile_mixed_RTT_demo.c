/**
 * @file CO_profile_mixed_RTT_demo.c
 * @brief Package mixed-demo factory for the profile-neutral RT-Thread ownership validator.
 */

#include "CO_profile_mixed_RTT.h"
#include "CO_profile_mixed_demo.h"
#include "CO_lifecycle_RTT.h"

/* Register before the 401/402 factories so ownership validation runs before profile OD binding. */
#define CO_PROFILE_MIXED_DEMO_FACTORY_ORDER 300U

static const CO_profile_mixed_RTT_config_t demoConfig = {
    .descriptors = CO_profileMixedDemoDescriptors,
    .descriptorCount = CO_PROFILE_MIXED_DEMO_DESCRIPTOR_COUNT,
};

static rt_err_t autoAttach(CANopenNodeRTT *app, const void *factoryContext)
{
    const CO_profile_mixed_RTT_config_t *config = factoryContext;

    return CO_profileMixedRTTAttach(app, config);
}

static const CO_RTT_lifecycle_factory_t mixedFactory = {
    .name = "mixed-profile-layout",
    .order = CO_PROFILE_MIXED_DEMO_FACTORY_ORDER,
    .attach = autoAttach,
    .context = &demoConfig,
};

static int registerFactory(void)
{
    return (int)CO_RTT_lifecycleFactoryRegister(&mixedFactory);
}
INIT_COMPONENT_EXPORT(registerFactory);
