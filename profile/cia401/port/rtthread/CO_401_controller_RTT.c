/**
 * @file CO_401_controller_RTT.c
 * @brief RT-Thread compatibility facade for the portable CiA 401 Controller client.
 */

#include "CO_401_controller_RTT.h"

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

rt_err_t CO_401_controller_RTT_init(CO_401_controller_RTT_t *adapter, CO_profile_master_RTT_t *transport,
                                    const CO_401_controller_RTT_config_t *config)
{
    CO_profile_transport_t *portable = CO_profileMasterRTT_transport(transport);

    if (portable == NULL) {
        return -RT_EINVAL;
    }
    return toRtError(CO_401_controller_client_init(adapter, portable, config));
}

rt_err_t CO_401_controller_RTT_readDigitalInput(CO_401_controller_RTT_t *adapter, uint8_t bank,
                                                uint8_t *value, CO_profile_transfer_result_t *result)
{
    return toRtError(CO_401_controller_client_readDigitalInput(adapter, bank, value, result));
}

rt_err_t CO_401_controller_RTT_writeDigitalOutput(CO_401_controller_RTT_t *adapter, uint8_t bank,
                                                  uint8_t value, CO_profile_transfer_result_t *result)
{
    return toRtError(CO_401_controller_client_writeDigitalOutput(adapter, bank, value, result));
}

rt_err_t CO_401_controller_RTT_readAnalogInput(CO_401_controller_RTT_t *adapter, uint8_t channel,
                                               int16_t *value, CO_profile_transfer_result_t *result)
{
    return toRtError(CO_401_controller_client_readAnalogInput(adapter, channel, value, result));
}

rt_err_t CO_401_controller_RTT_writeAnalogOutput(CO_401_controller_RTT_t *adapter, uint8_t channel,
                                                 int16_t value, CO_profile_transfer_result_t *result)
{
    return toRtError(CO_401_controller_client_writeAnalogOutput(adapter, channel, value, result));
}

rt_err_t CO_401_controller_RTT_readProfileObject(CO_401_controller_RTT_t *adapter, uint16_t canonicalIndex,
                                                 uint8_t subIndex, uint8_t size,
                                                 CO_profile_transfer_result_t *result)
{
    return toRtError(CO_401_controller_client_readProfileObject(adapter, canonicalIndex, subIndex, size, result));
}

rt_err_t CO_401_controller_RTT_writeProfileObject(CO_401_controller_RTT_t *adapter, uint16_t canonicalIndex,
                                                  uint8_t subIndex, const void *data, uint8_t size,
                                                  CO_profile_transfer_result_t *result)
{
    return toRtError(CO_401_controller_client_writeProfileObject(adapter, canonicalIndex, subIndex,
                                                                  data, size, result));
}
