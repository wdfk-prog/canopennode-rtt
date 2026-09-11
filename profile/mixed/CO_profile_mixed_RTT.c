/**
 * @file CO_profile_mixed_RTT.c
 * @brief Profile-neutral RT-Thread lifecycle adapter for logical-device ownership validation.
 */

#define LOG_TAG "canopen.profile.mixed"
#define LOG_LVL LOG_LVL_DBG

#include "CO_profile_mixed_RTT.h"
#include "CO_lifecycle_RTT.h"
#include "co_rtt_log.h"

#include <string.h>

/** Lifecycle-owned copy of one product/demo descriptor table and its published registry. */
typedef struct {
    CO_profile_descriptor_t descriptors[CO_PROFILE_LOGICAL_DEVICE_COUNT_MAX];
    uint8_t descriptorCount;
    CO_profile_registry_t registry;
    CO_profile_registry_diag_t diag;
} CO_profile_mixed_RTT_context_t;

static bool readDeviceType(void *object, uint8_t logicalDevice, uint32_t *deviceType)
{
    OD_t *od = object;
    OD_entry_t *entry;

    if (od == NULL || deviceType == NULL) {
        return false;
    }

    entry = OD_find(od, CO_profileDeviceTypeIndex(logicalDevice));
    return entry != NULL && OD_get_u32(entry, 0U, deviceType, true) == ODR_OK;
}

static void onQuiesced(CANopenNodeRTT *app, void *context)
{
    CO_profile_mixed_RTT_context_t *runtime = context;

    (void)app;
    if (runtime != NULL) {
        /*
         * Lifecycle quiesce runs in reverse registration order. When this validator was
         * attached before profile adapters, their old-generation bindings are released
         * before ownership is cleared here, preserving the validated-generation boundary.
         */
        CO_profileRegistryClear(&runtime->registry);
    }
}

static rt_err_t onBind(CANopenNodeRTT *app, CO_t *co, OD_t *od, void *context)
{
    CO_profile_mixed_RTT_context_t *runtime = context;
    CO_profile_registry_result_t result;

    (void)app;
    (void)co;
    if (runtime == NULL || od == NULL) {
        return -RT_EINVAL;
    }

    result = CO_profileRegistryBuild(&runtime->registry, runtime->descriptors, runtime->descriptorCount,
                                     readDeviceType, od, &runtime->diag);
    if (result != CO_PROFILE_REGISTRY_OK) {
        CO_RTT_LOG_E("mixed profile layout rejected: result=%d descriptor=%u slot=%u profile=%u type=0x%08lx",
                     (int)result, (unsigned int)runtime->diag.descriptorIndex,
                     (unsigned int)runtime->diag.logicalDevice, (unsigned int)runtime->diag.profileNumber,
                     (unsigned long)runtime->diag.deviceType);
        return -RT_ERROR;
    }

    return RT_EOK;
}

static void onDeinit(CANopenNodeRTT *app, void *context)
{
    (void)app;
    if (context != NULL) {
        CO_profileRegistryClear(&((CO_profile_mixed_RTT_context_t *)context)->registry);
    }
}

static const CO_RTT_lifecycle_ops_t lifecycleOps = {
    .communicationQuiesced = onQuiesced,
    .communicationBind = onBind,
    .runtimeDeinit = onDeinit,
};

static void releaseContext(void *context)
{
    rt_free(context);
}

rt_err_t CO_profileMixedRTTAttach(CANopenNodeRTT *app, const CO_profile_mixed_RTT_config_t *config)
{
    CO_profile_mixed_RTT_context_t *runtime;
    rt_err_t ret;

    if (app == NULL || config == NULL || config->descriptors == NULL || config->descriptorCount == 0U
        || config->descriptorCount > CO_PROFILE_LOGICAL_DEVICE_COUNT_MAX) {
        return -RT_EINVAL;
    }

    runtime = rt_calloc(1U, sizeof(*runtime));
    if (runtime == NULL) {
        return -RT_ENOMEM;
    }

    runtime->descriptorCount = config->descriptorCount;
    if (runtime->descriptorCount > 0U) {
        (void)memcpy(runtime->descriptors, config->descriptors,
                     (size_t)runtime->descriptorCount * sizeof(runtime->descriptors[0]));
    }

    ret = CO_RTT_lifecycleRegisterEx(app, &lifecycleOps, runtime, releaseContext);
    if (ret != RT_EOK) {
        rt_free(runtime);
    }
    return ret;
}
