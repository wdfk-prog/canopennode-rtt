/**
 * @file CO_profile_transport.c
 * @brief Validation and dispatch for the portable Device Profile Controller transport contract.
 */

#include <string.h>

#include "CO_profile_transport.h"

static CO_profile_call_result_t callResult(CO_profile_call_status_t status, int32_t backendError)
{
    CO_profile_call_result_t result = {status, backendError};
    return result;
}

bool CO_profileTransport_init(CO_profile_transport_t *transport, void *context,
                              const CO_profile_transport_ops_t *ops)
{
    if (transport == NULL || ops == NULL || ops->sdoUpload == NULL || ops->sdoDownload == NULL) {
        if (transport != NULL) {
            (void)memset(transport, 0, sizeof(*transport));
        }
        return false;
    }

    transport->context = context;
    transport->ops = ops;
    return true;
}

CO_profile_call_result_t CO_profileTransport_sdoUpload(CO_profile_transport_t *transport,
                                                        uint8_t remoteNodeId, uint16_t index,
                                                        uint8_t subIndex, uint8_t expectedSize,
                                                        CO_profile_transfer_result_t *result)
{
    if (transport == NULL || transport->ops == NULL || transport->ops->sdoUpload == NULL
        || result == NULL || remoteNodeId == 0U || remoteNodeId > 127U || index == 0U
        || expectedSize == 0U || expectedSize > CO_PROFILE_CONTROLLER_VALUE_SIZE_MAX) {
        return callResult(CO_PROFILE_CALL_INVALID_ARGUMENT, 0);
    }
    return transport->ops->sdoUpload(transport->context, remoteNodeId, index, subIndex, expectedSize, result);
}

CO_profile_call_result_t CO_profileTransport_sdoDownload(CO_profile_transport_t *transport,
                                                          uint8_t remoteNodeId, uint16_t index,
                                                          uint8_t subIndex, const void *data,
                                                          uint8_t size,
                                                          CO_profile_transfer_result_t *result)
{
    if (transport == NULL || transport->ops == NULL || transport->ops->sdoDownload == NULL
        || result == NULL || remoteNodeId == 0U || remoteNodeId > 127U || index == 0U
        || data == NULL || size == 0U || size > CO_PROFILE_CONTROLLER_VALUE_SIZE_MAX) {
        return callResult(CO_PROFILE_CALL_INVALID_ARGUMENT, 0);
    }
    return transport->ops->sdoDownload(transport->context, remoteNodeId, index, subIndex, data, size, result);
}

bool CO_profileTransport_nmtCommandValid(CO_profile_nmt_command_t command)
{
    return command == CO_PROFILE_NMT_ENTER_OPERATIONAL || command == CO_PROFILE_NMT_ENTER_STOPPED
           || command == CO_PROFILE_NMT_ENTER_PRE_OPERATIONAL || command == CO_PROFILE_NMT_RESET_NODE
           || command == CO_PROFILE_NMT_RESET_COMMUNICATION;
}

CO_profile_call_result_t CO_profileTransport_nmtCommand(CO_profile_transport_t *transport,
                                                         CO_profile_nmt_command_t command,
                                                         uint8_t nodeId,
                                                         CO_profile_transfer_result_t *result)
{
    if (transport == NULL || result == NULL || nodeId > 127U || !CO_profileTransport_nmtCommandValid(command)) {
        return callResult(CO_PROFILE_CALL_INVALID_ARGUMENT, 0);
    }
    if (transport->ops == NULL || transport->ops->nmtCommand == NULL) {
        return callResult(CO_PROFILE_CALL_NOT_READY, 0);
    }
    return transport->ops->nmtCommand(transport->context, command, nodeId, result);
}
