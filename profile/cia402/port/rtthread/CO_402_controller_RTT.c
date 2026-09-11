/**
 * @file CO_402_controller_RTT.c
 * @brief RT-Thread compatibility facade for the portable CiA 402 Controller client.
 */

#include "CO_402_controller_RTT.h"

static rt_err_t toRtError(CO_profile_call_result_t call)
{
    if (call.status == CO_PROFILE_CALL_OK) {
        return RT_EOK;
    }
    if (call.backendError != 0) {
        return (rt_err_t)call.backendError;
    }
    if (call.status == CO_PROFILE_CALL_INVALID_ARGUMENT) {
        return -RT_EINVAL;
    }
    if (call.status == CO_PROFILE_CALL_NOT_READY || call.status == CO_PROFILE_CALL_BUSY
        || call.status == CO_PROFILE_CALL_CONTEXT_ERROR) {
        return -RT_EBUSY;
    }
    return -RT_ERROR;
}

/** Preserve the legacy RT-Thread sentinel for transfer-result fields which were not attempted. */
static void preserveUnattemptedTransferSentinel(CO_profile_transfer_result_t *result)
{
    if (result->status == CO_PROFILE_TRANSFER_LOCAL_ERROR && result->localError == 0
        && result->abortCode == 0U && result->size == 0U) {
        result->localError = (int32_t)-RT_EBUSY;
    }
}

rt_err_t CO_402_controller_RTT_init(CO_402_controller_RTT_t *adapter, CO_profile_master_RTT_t *transport,
                                    const CO_402_controller_RTT_config_t *config)
{
    CO_profile_transport_t *portable = CO_profileMasterRTT_transport(transport);

    if (portable == NULL) {
        return -RT_EINVAL;
    }
    return toRtError(CO_402_controller_client_init(adapter, portable, config));
}

bool CO_402_controller_RTT_setTarget(CO_402_controller_RTT_t *adapter, CO_402_controller_target_t target)
{
    return CO_402_controller_client_setTarget(adapter, target);
}

void CO_402_controller_RTT_clearTarget(CO_402_controller_RTT_t *adapter)
{
    CO_402_controller_client_clearTarget(adapter);
}

bool CO_402_controller_RTT_requestFaultReset(CO_402_controller_RTT_t *adapter)
{
    return CO_402_controller_client_requestFaultReset(adapter);
}

rt_err_t CO_402_controller_RTT_process(CO_402_controller_RTT_t *adapter, uint32_t timeDifferenceUs,
                                       CO_402_controller_RTT_step_result_t *result)
{
    const bool resultInitialized = adapter != NULL && adapter->initialized && result != NULL;
    CO_profile_call_result_t call = CO_402_controller_client_process(adapter, timeDifferenceUs, result);

    if (resultInitialized) {
        /* Keep the historical RTT facade result shape while the portable core remains OS-neutral. */
        preserveUnattemptedTransferSentinel(&result->statuswordTransfer);
        preserveUnattemptedTransferSentinel(&result->controlwordReadTransfer);
        preserveUnattemptedTransferSentinel(&result->controlwordWriteTransfer);
    }
    return toRtError(call);
}

CO_402_state_t CO_402_controller_RTT_getRemoteState(const CO_402_controller_RTT_t *adapter)
{
    return CO_402_controller_client_getRemoteState(adapter);
}
