/**
 * @file CO_402_controller_client.c
 * @brief Portable transport-backed control-plane implementation for one remote CiA 402 PDS axis.
 */

#include <string.h>

#include "CO_402_controller_client.h"
#include "CO_402_objects.h"
#include "CO_profile_layout.h"

static CO_profile_call_result_t invalidCall(void)
{
    CO_profile_call_result_t result = {CO_PROFILE_CALL_INVALID_ARGUMENT, 0};
    return result;
}

static CO_profile_call_result_t okCall(void)
{
    CO_profile_call_result_t result = {CO_PROFILE_CALL_OK, 0};
    return result;
}

/** Saturating elapsed-time accumulation for retry-only calls which cannot advance the semantic core. */
static uint32_t saturatingAddU32(uint32_t value, uint32_t increment)
{
    return UINT32_MAX - value < increment ? UINT32_MAX : value + increment;
}

/** Commit deferred elapsed time to feedback age; the caller separately decides transition-budget ownership. */
static void commitDeferredFeedbackElapsed(CO_402_controller_client_t *client)
{
    client->controller.feedbackElapsed_us =
        saturatingAddU32(client->controller.feedbackElapsed_us, client->deferredElapsedUs);
    client->deferredElapsedUs = 0U;
}

/** Initialize one transfer-result field to an unattempted local-error state. */
static void clearTransferResult(CO_profile_transfer_result_t *result)
{
    (void)memset(result, 0, sizeof(*result));
    result->status = CO_PROFILE_TRANSFER_LOCAL_ERROR;
}

/** Decode one successful CANopen little-endian UNSIGNED16 transfer. */
static uint16_t decodeU16(const CO_profile_transfer_result_t *result)
{
    return (uint16_t)result->data[0] | ((uint16_t)result->data[1] << 8U);
}

/** Encode one UNSIGNED16 value in CANopen little-endian order. */
static void encodeU16(uint16_t value, uint8_t data[2])
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
}

/** Resolve one canonical CiA 402 scalar in the configured remote logical-device block. */
static uint16_t profileIndex(const CO_402_controller_client_t *client, uint16_t canonicalIndex)
{
    return CO_profileIndex(client->logicalDevice, canonicalIndex);
}

/** Flush one stored PDS update without allowing the semantic core to advance past failed transport. */
static CO_profile_call_result_t flushPendingUpdate(CO_402_controller_client_t *client,
                                                    CO_402_controller_client_step_result_t *step)
{
    CO_profile_transfer_result_t readResult;
    CO_profile_transfer_result_t writeResult;
    CO_profile_call_result_t call;
    uint8_t data[2];
    uint16_t controlword;
    const uint16_t index = profileIndex(client, CO_402_INDEX_CONTROLWORD);

    step->commandPending = true;
    /* The public contract requires one external owner/lock across this non-atomic 0x6040 RMW pair. */
    call = CO_profileTransport_sdoUpload(client->transport, client->nodeId, index, 0U, 2U, &readResult);
    if (call.status != CO_PROFILE_CALL_OK) {
        return call;
    }
    step->controlwordReadTransfer = readResult;
    if (readResult.status != CO_PROFILE_TRANSFER_OK) {
        return okCall();
    }

    controlword = decodeU16(&readResult);
    controlword = CO_402_controller_applyControlwordUpdate(controlword, &client->pendingUpdate);
    step->controlword = controlword;
    encodeU16(controlword, data);

    call = CO_profileTransport_sdoDownload(client->transport, client->nodeId, index, 0U, data,
                                           (uint8_t)sizeof(data), &writeResult);
    if (call.status != CO_PROFILE_CALL_OK) {
        return call;
    }
    step->controlwordWriteTransfer = writeResult;
    if (writeResult.status == CO_PROFILE_TRANSFER_OK) {
        client->pendingUpdateValid = false;
        client->pendingFaultResetEdge = false;
        (void)memset(&client->pendingUpdate, 0, sizeof(client->pendingUpdate));
        step->commandPending = false;
        step->commandAccepted = true;
    }
    return okCall();
}

CO_profile_call_result_t CO_402_controller_client_init(CO_402_controller_client_t *client,
                                                        CO_profile_transport_t *transport,
                                                        const CO_402_controller_client_config_t *config)
{
    if (client == NULL || transport == NULL || transport->ops == NULL || config == NULL
        || config->nodeId == 0U || config->nodeId > 127U
        || !CO_profileLogicalDeviceValid(config->logicalDevice)) {
        return invalidCall();
    }

    (void)memset(client, 0, sizeof(*client));
    if (!CO_402_controller_init(&client->controller, &config->controller)) {
        return invalidCall();
    }
    client->transport = transport;
    client->nodeId = config->nodeId;
    client->logicalDevice = config->logicalDevice;
    client->initialized = true;
    return okCall();
}

bool CO_402_controller_client_setTarget(CO_402_controller_client_t *client,
                                         CO_402_controller_target_t target)
{
    bool sameTarget;
    bool accepted;

    if (client == NULL || !client->initialized || client->pendingFaultResetEdge) {
        return false;
    }
    sameTarget = client->controller.targetValid && client->controller.target == target;
    accepted = CO_402_controller_setTarget(&client->controller, target);
    if (accepted && !sameTarget) {
        /* Retry time predating this target still ages feedback, but must not consume the new transition budget. */
        commitDeferredFeedbackElapsed(client);
        client->pendingUpdateValid = false;
        (void)memset(&client->pendingUpdate, 0, sizeof(client->pendingUpdate));
    }
    return accepted;
}

void CO_402_controller_client_clearTarget(CO_402_controller_client_t *client)
{
    if (client == NULL || !client->initialized) {
        return;
    }
    CO_402_controller_clearTarget(&client->controller);
    /* Clearing target restarts transition intent; preserve only feedback age accumulated by transport retries. */
    commitDeferredFeedbackElapsed(client);
    if (!client->pendingFaultResetEdge) {
        client->pendingUpdateValid = false;
        (void)memset(&client->pendingUpdate, 0, sizeof(client->pendingUpdate));
    }
}

bool CO_402_controller_client_requestFaultReset(CO_402_controller_client_t *client)
{
    bool accepted;

    if (client == NULL || !client->initialized || client->pendingFaultResetEdge) {
        return false;
    }
    accepted = CO_402_controller_requestFaultReset(&client->controller);
    if (accepted) {
        client->pendingUpdateValid = false;
        (void)memset(&client->pendingUpdate, 0, sizeof(client->pendingUpdate));
    }
    return accepted;
}

CO_profile_call_result_t CO_402_controller_client_process(CO_402_controller_client_t *client,
                                                           uint32_t timeDifferenceUs,
                                                           CO_402_controller_client_step_result_t *result)
{
    CO_402_controller_feedback_t feedback;
    CO_402_controller_controlword_update_t update;
    CO_profile_transfer_result_t statusResult;
    CO_profile_call_result_t call;
    uint32_t semanticElapsedUs;
    bool faultResetPulseWasHigh;
    uint16_t statusIndex;

    if (client == NULL || result == NULL || !client->initialized) {
        return invalidCall();
    }

    (void)memset(result, 0, sizeof(*result));
    clearTransferResult(&result->statuswordTransfer);
    clearTransferResult(&result->controlwordReadTransfer);
    clearTransferResult(&result->controlwordWriteTransfer);
    result->controllerResult = CO_402_controller_getResult(&client->controller);

    if (client->pendingUpdateValid) {
        /* A not-yet-accepted Controlword retry ages feedback only; commit it immediately on acceptance so it cannot
         * leak into the next Statusword step's transition budget. */
        client->deferredElapsedUs = saturatingAddU32(client->deferredElapsedUs, timeDifferenceUs);
        call = flushPendingUpdate(client, result);
        if (result->commandAccepted) {
            commitDeferredFeedbackElapsed(client);
        }
        return call;
    }

    statusIndex = profileIndex(client, CO_402_INDEX_STATUSWORD);
    if (statusIndex == CO_PROFILE_INDEX_INVALID) {
        return invalidCall();
    }
    call = CO_profileTransport_sdoUpload(client->transport, client->nodeId, statusIndex, 0U, 2U, &statusResult);
    if (call.status != CO_PROFILE_CALL_OK) {
        client->deferredElapsedUs = saturatingAddU32(client->deferredElapsedUs, timeDifferenceUs);
        return call;
    }
    result->statuswordTransfer = statusResult;
    /* Deferred time here can only be a Statusword call-level outage. Replaying it into this semantic step keeps an
     * active transition on wall-clock time; a newly observed remote state still resets the per-state timer in core. */
    semanticElapsedUs = saturatingAddU32(client->deferredElapsedUs, timeDifferenceUs);
    client->deferredElapsedUs = 0U;

    (void)memset(&feedback, 0, sizeof(feedback));
    if (statusResult.status == CO_PROFILE_TRANSFER_OK) {
        result->statusword = decodeU16(&statusResult);
        result->statuswordValid = true;
        feedback.statuswordValid = true;
        feedback.statusword = result->statusword;
    }

    faultResetPulseWasHigh = client->controller.faultResetPulseHigh;
    result->controllerResult = CO_402_controller_process(&client->controller, &feedback, semanticElapsedUs, &update);
    if (!update.valid) {
        return okCall();
    }

    client->pendingUpdate = update;
    client->pendingUpdateValid = true;
    /* A pulse high before this step emits its low edge; a pulse high after this step emitted the high edge. */
    client->pendingFaultResetEdge = faultResetPulseWasHigh || client->controller.faultResetPulseHigh;
    return flushPendingUpdate(client, result);
}

CO_402_state_t CO_402_controller_client_getRemoteState(const CO_402_controller_client_t *client)
{
    if (client == NULL || !client->initialized) {
        return CO_402_STATE_UNKNOWN;
    }
    return CO_402_controller_getRemoteState(&client->controller);
}
