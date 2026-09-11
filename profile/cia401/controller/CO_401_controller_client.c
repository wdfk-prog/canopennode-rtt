/**
 * @file CO_401_controller_client.c
 * @brief Portable CiA 401 remote I/O client built on the common profile transport contract.
 */

#include <string.h>

#include "CO_401_controller_client.h"

static CO_profile_call_result_t invalidCall(void)
{
    CO_profile_call_result_t result = {CO_PROFILE_CALL_INVALID_ARGUMENT, 0};
    return result;
}

static CO_profile_call_result_t readRef(CO_401_controller_client_t *client,
                                        const CO_profile_object_ref_t *ref,
                                        CO_profile_transfer_result_t *result)
{
    if (client == NULL || ref == NULL || result == NULL || !client->initialized) {
        return invalidCall();
    }
    return CO_profileTransport_sdoUpload(client->transport, client->nodeId, ref->index, ref->subIndex,
                                         ref->size, result);
}

static CO_profile_call_result_t writeRef(CO_401_controller_client_t *client,
                                         const CO_profile_object_ref_t *ref,
                                         const void *data,
                                         CO_profile_transfer_result_t *result)
{
    if (client == NULL || ref == NULL || data == NULL || result == NULL || !client->initialized) {
        return invalidCall();
    }
    return CO_profileTransport_sdoDownload(client->transport, client->nodeId, ref->index, ref->subIndex,
                                           data, ref->size, result);
}

CO_profile_call_result_t CO_401_controller_client_init(CO_401_controller_client_t *client,
                                                        CO_profile_transport_t *transport,
                                                        const CO_401_controller_client_config_t *config)
{
    if (client == NULL || transport == NULL || transport->ops == NULL || config == NULL
        || config->nodeId == 0U || config->nodeId > 127U) {
        return invalidCall();
    }

    (void)memset(client, 0, sizeof(*client));
    if (!CO_401_controller_init(&client->controller, &config->controller)) {
        return invalidCall();
    }
    client->transport = transport;
    client->nodeId = config->nodeId;
    client->initialized = true;
    return (CO_profile_call_result_t){CO_PROFILE_CALL_OK, 0};
}

CO_profile_call_result_t CO_401_controller_client_readDigitalInput(CO_401_controller_client_t *client,
                                                                    uint8_t bank, uint8_t *value,
                                                                    CO_profile_transfer_result_t *result)
{
    CO_profile_object_ref_t ref;
    CO_profile_call_result_t call;

    if (client == NULL || value == NULL || result == NULL
        || !CO_401_controller_digitalInputRef(&client->controller, bank, &ref)) {
        return invalidCall();
    }
    call = readRef(client, &ref, result);
    if (call.status == CO_PROFILE_CALL_OK && result->status == CO_PROFILE_TRANSFER_OK) {
        *value = result->data[0];
    }
    return call;
}

CO_profile_call_result_t CO_401_controller_client_writeDigitalOutput(CO_401_controller_client_t *client,
                                                                      uint8_t bank, uint8_t value,
                                                                      CO_profile_transfer_result_t *result)
{
    CO_profile_object_ref_t ref;

    if (client == NULL || result == NULL
        || !CO_401_controller_digitalOutputRef(&client->controller, bank, &ref)) {
        return invalidCall();
    }
    return writeRef(client, &ref, &value, result);
}

CO_profile_call_result_t CO_401_controller_client_readAnalogInput(CO_401_controller_client_t *client,
                                                                   uint8_t channel, int16_t *value,
                                                                   CO_profile_transfer_result_t *result)
{
    CO_profile_object_ref_t ref;
    CO_profile_call_result_t call;

    if (client == NULL || value == NULL || result == NULL
        || !CO_401_controller_analogInputRef(&client->controller, channel, &ref)) {
        return invalidCall();
    }
    call = readRef(client, &ref, result);
    if (call.status == CO_PROFILE_CALL_OK && result->status == CO_PROFILE_TRANSFER_OK) {
        const uint16_t raw = (uint16_t)result->data[0] | ((uint16_t)result->data[1] << 8U);
        *value = (int16_t)raw;
    }
    return call;
}

CO_profile_call_result_t CO_401_controller_client_writeAnalogOutput(CO_401_controller_client_t *client,
                                                                     uint8_t channel, int16_t value,
                                                                     CO_profile_transfer_result_t *result)
{
    CO_profile_object_ref_t ref;
    uint8_t data[2];
    const uint16_t raw = (uint16_t)value;

    if (client == NULL || result == NULL
        || !CO_401_controller_analogOutputRef(&client->controller, channel, &ref)) {
        return invalidCall();
    }
    data[0] = (uint8_t)raw;
    data[1] = (uint8_t)(raw >> 8U);
    return writeRef(client, &ref, data, result);
}

CO_profile_call_result_t CO_401_controller_client_readProfileObject(CO_401_controller_client_t *client,
                                                                     uint16_t canonicalIndex,
                                                                     uint8_t subIndex, uint8_t size,
                                                                     CO_profile_transfer_result_t *result)
{
    CO_profile_object_ref_t ref;

    if (client == NULL || result == NULL
        || !CO_401_controller_profileRef(&client->controller, canonicalIndex, subIndex, size, &ref)) {
        return invalidCall();
    }
    return readRef(client, &ref, result);
}

CO_profile_call_result_t CO_401_controller_client_writeProfileObject(CO_401_controller_client_t *client,
                                                                      uint16_t canonicalIndex,
                                                                      uint8_t subIndex, const void *data,
                                                                      uint8_t size,
                                                                      CO_profile_transfer_result_t *result)
{
    CO_profile_object_ref_t ref;

    if (client == NULL || data == NULL || result == NULL
        || !CO_401_controller_profileRef(&client->controller, canonicalIndex, subIndex, size, &ref)) {
        return invalidCall();
    }
    return writeRef(client, &ref, data, result);
}
