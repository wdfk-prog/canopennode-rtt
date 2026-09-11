/**
 * @file profile-controller-adapter-host-test.c
 * @brief Host checks for portable CiA 401/402 Controller clients with a fake stack-neutral transport.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "CO_401_controller_client.h"
#include "CO_402_controller_client.h"
#include "CO_profile_transport.h"
#include "CO_401_objects.h"
#include "CO_402_objects.h"

#define TEST_ASSERT(expr)                                                                    \
    do {                                                                                     \
        if (!(expr)) {                                                                       \
            fprintf(stderr, "PROFILE_CONTROLLER_ADAPTER_HOST_FAIL:%s:%d:%s\n",              \
                    __func__, __LINE__, #expr);                                               \
            return false;                                                                    \
        }                                                                                    \
    } while (0)

typedef enum {
    OP_UPLOAD = 0,
    OP_DOWNLOAD
} expected_operation_t;

typedef struct {
    expected_operation_t operation;
    uint8_t nodeId;
    uint16_t index;
    uint8_t subIndex;
    uint8_t size;
    CO_profile_transfer_status_t status;
    CO_profile_call_result_t expectedCall;
    uint32_t abortCode;
    uint8_t data[CO_PROFILE_CONTROLLER_VALUE_SIZE_MAX];
} expected_transfer_t;

static expected_transfer_t transferQueue[32];
static size_t transferCount;
static size_t transferCursor;
static bool transferMismatch;
static unsigned nmtCallCount;
static CO_profile_nmt_command_t lastNmtCommand;
static uint8_t lastNmtNodeId;

static void resetTransfers(void)
{
    (void)memset(transferQueue, 0, sizeof(transferQueue));
    transferCount = 0U;
    transferCursor = 0U;
    transferMismatch = false;
    nmtCallCount = 0U;
    lastNmtCommand = (CO_profile_nmt_command_t)0;
    lastNmtNodeId = 0U;
}

static void expectUpload(uint8_t nodeId, uint16_t index, uint8_t subIndex, uint8_t size,
                         CO_profile_transfer_status_t status, const void *data, uint32_t abortCode)
{
    expected_transfer_t *transfer = &transferQueue[transferCount++];

    transfer->operation = OP_UPLOAD;
    transfer->nodeId = nodeId;
    transfer->index = index;
    transfer->subIndex = subIndex;
    transfer->size = size;
    transfer->status = status;
    transfer->abortCode = abortCode;
    if (data != NULL && size > 0U) {
        (void)memcpy(transfer->data, data, size);
    }
}

static void expectDownload(uint8_t nodeId, uint16_t index, uint8_t subIndex, const void *data, uint8_t size,
                           CO_profile_transfer_status_t status, uint32_t abortCode)
{
    expected_transfer_t *transfer = &transferQueue[transferCount++];

    transfer->operation = OP_DOWNLOAD;
    transfer->nodeId = nodeId;
    transfer->index = index;
    transfer->subIndex = subIndex;
    transfer->size = size;
    transfer->status = status;
    transfer->abortCode = abortCode;
    if (data != NULL && size > 0U) {
        (void)memcpy(transfer->data, data, size);
    }
}

static void expectUploadCallFailure(uint8_t nodeId, uint16_t index, uint8_t subIndex, uint8_t size,
                                    CO_profile_call_status_t status, int32_t backendError)
{
    expected_transfer_t *transfer = &transferQueue[transferCount++];

    transfer->operation = OP_UPLOAD;
    transfer->nodeId = nodeId;
    transfer->index = index;
    transfer->subIndex = subIndex;
    transfer->size = size;
    transfer->expectedCall.status = status;
    transfer->expectedCall.backendError = backendError;
}

static CO_profile_call_result_t consumeTransfer(expected_operation_t operation, uint8_t nodeId,
                                                uint16_t index, uint8_t subIndex, uint8_t size,
                                                const void *downloadData,
                                                CO_profile_transfer_result_t *result)
{
    expected_transfer_t *expected;
    CO_profile_call_result_t call = {CO_PROFILE_CALL_OK, 0};

    if (transferCursor >= transferCount) {
        transferMismatch = true;
        call.status = CO_PROFILE_CALL_BACKEND_ERROR;
        call.backendError = -1001;
        return call;
    }
    expected = &transferQueue[transferCursor++];
    if (expected->operation != operation || expected->nodeId != nodeId || expected->index != index
        || expected->subIndex != subIndex || expected->size != size) {
        transferMismatch = true;
        call.status = CO_PROFILE_CALL_BACKEND_ERROR;
        call.backendError = -1002;
        return call;
    }
    if (operation == OP_DOWNLOAD && memcmp(expected->data, downloadData, size) != 0) {
        transferMismatch = true;
        call.status = CO_PROFILE_CALL_BACKEND_ERROR;
        call.backendError = -1003;
        return call;
    }
    if (expected->expectedCall.status != CO_PROFILE_CALL_OK) {
        return expected->expectedCall;
    }

    (void)memset(result, 0, sizeof(*result));
    result->status = expected->status;
    result->abortCode = expected->abortCode;
    if (operation == OP_UPLOAD && expected->status == CO_PROFILE_TRANSFER_OK) {
        result->size = size;
        (void)memcpy(result->data, expected->data, size);
    }
    return call;
}

static CO_profile_call_result_t fakeUpload(void *context, uint8_t remoteNodeId, uint16_t index,
                                           uint8_t subIndex, uint8_t expectedSize,
                                           CO_profile_transfer_result_t *result)
{
    (void)context;
    return consumeTransfer(OP_UPLOAD, remoteNodeId, index, subIndex, expectedSize, NULL, result);
}

static CO_profile_call_result_t fakeDownload(void *context, uint8_t remoteNodeId, uint16_t index,
                                             uint8_t subIndex, const void *data, uint8_t size,
                                             CO_profile_transfer_result_t *result)
{
    (void)context;
    return consumeTransfer(OP_DOWNLOAD, remoteNodeId, index, subIndex, size, data, result);
}

static CO_profile_call_result_t fakeNmt(void *context, CO_profile_nmt_command_t command,
                                        uint8_t nodeId, CO_profile_transfer_result_t *result)
{
    (void)context;
    nmtCallCount++;
    lastNmtCommand = command;
    lastNmtNodeId = nodeId;
    (void)memset(result, 0, sizeof(*result));
    result->status = CO_PROFILE_TRANSFER_OK;
    return (CO_profile_call_result_t){CO_PROFILE_CALL_OK, 0};
}

static const CO_profile_transport_ops_t fakeTransportOps = {
    .sdoUpload = fakeUpload,
    .sdoDownload = fakeDownload,
    .nmtCommand = fakeNmt,
};

static bool transfersComplete(void)
{
    TEST_ASSERT(!transferMismatch);
    TEST_ASSERT(transferCursor == transferCount);
    return true;
}


static bool test_common_transport_contract(void)
{
    CO_profile_transport_t transport;
    CO_profile_transport_ops_t incomplete = fakeTransportOps;
    CO_profile_transfer_result_t result;
    CO_profile_call_result_t call;
    uint8_t one = 0x11U;
    uint8_t eight[8] = {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U};

    (void)memset(&transport, 0xA5, sizeof(transport));
    incomplete.sdoDownload = NULL;
    TEST_ASSERT(!CO_profileTransport_init(&transport, NULL, &incomplete));
    TEST_ASSERT(transport.context == NULL && transport.ops == NULL);

    incomplete = fakeTransportOps;
    incomplete.nmtCommand = NULL;
    TEST_ASSERT(CO_profileTransport_init(&transport, NULL, &incomplete));
    call = CO_profileTransport_nmtCommand(&transport, CO_PROFILE_NMT_ENTER_OPERATIONAL, 1U, &result);
    TEST_ASSERT(call.status == CO_PROFILE_CALL_NOT_READY && call.backendError == 0);

    TEST_ASSERT(CO_profileTransport_init(&transport, NULL, &fakeTransportOps));
    resetTransfers();
    expectUpload(1U, 0x0001U, 0U, 1U, CO_PROFILE_TRANSFER_OK, &one, 0U);
    call = CO_profileTransport_sdoUpload(&transport, 1U, 0x0001U, 0U, 1U, &result);
    TEST_ASSERT(call.status == CO_PROFILE_CALL_OK && result.status == CO_PROFILE_TRANSFER_OK);
    expectUpload(127U, 0xFFFFU, 0xFFU, 8U, CO_PROFILE_TRANSFER_OK, eight, 0U);
    call = CO_profileTransport_sdoUpload(&transport, 127U, 0xFFFFU, 0xFFU, 8U, &result);
    TEST_ASSERT(call.status == CO_PROFILE_CALL_OK && result.size == 8U);
    TEST_ASSERT(CO_profileTransport_sdoUpload(&transport, 0U, 0x6041U, 0U, 2U, &result).status
                == CO_PROFILE_CALL_INVALID_ARGUMENT);
    TEST_ASSERT(CO_profileTransport_sdoUpload(&transport, 128U, 0x6041U, 0U, 2U, &result).status
                == CO_PROFILE_CALL_INVALID_ARGUMENT);
    TEST_ASSERT(CO_profileTransport_sdoUpload(&transport, 1U, 0U, 0U, 2U, &result).status
                == CO_PROFILE_CALL_INVALID_ARGUMENT);
    TEST_ASSERT(CO_profileTransport_sdoUpload(&transport, 1U, 0x6041U, 0U, 0U, &result).status
                == CO_PROFILE_CALL_INVALID_ARGUMENT);
    TEST_ASSERT(CO_profileTransport_sdoUpload(&transport, 1U, 0x6041U, 0U, 9U, &result).status
                == CO_PROFILE_CALL_INVALID_ARGUMENT);

    expectDownload(127U, 0x0001U, 0xFFU, eight, 8U, CO_PROFILE_TRANSFER_OK, 0U);
    call = CO_profileTransport_sdoDownload(&transport, 127U, 0x0001U, 0xFFU, eight, 8U, &result);
    TEST_ASSERT(call.status == CO_PROFILE_CALL_OK && result.status == CO_PROFILE_TRANSFER_OK);
    TEST_ASSERT(CO_profileTransport_sdoDownload(&transport, 0U, 0x6040U, 0U, &one, 1U, &result).status
                == CO_PROFILE_CALL_INVALID_ARGUMENT);
    TEST_ASSERT(CO_profileTransport_sdoDownload(&transport, 128U, 0x6040U, 0U, &one, 1U, &result).status
                == CO_PROFILE_CALL_INVALID_ARGUMENT);
    TEST_ASSERT(CO_profileTransport_sdoDownload(&transport, 1U, 0U, 0U, &one, 1U, &result).status
                == CO_PROFILE_CALL_INVALID_ARGUMENT);
    TEST_ASSERT(CO_profileTransport_sdoDownload(&transport, 1U, 0x6040U, 0U, NULL, 1U, &result).status
                == CO_PROFILE_CALL_INVALID_ARGUMENT);
    TEST_ASSERT(CO_profileTransport_sdoDownload(&transport, 1U, 0x6040U, 0U, &one, 0U, &result).status
                == CO_PROFILE_CALL_INVALID_ARGUMENT);
    TEST_ASSERT(CO_profileTransport_sdoDownload(&transport, 1U, 0x6040U, 0U, eight, 9U, &result).status
                == CO_PROFILE_CALL_INVALID_ARGUMENT);
    TEST_ASSERT(transfersComplete());

    call = CO_profileTransport_nmtCommand(&transport, CO_PROFILE_NMT_ENTER_OPERATIONAL, 0U, &result);
    TEST_ASSERT(call.status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(nmtCallCount == 1U && lastNmtNodeId == 0U
                && lastNmtCommand == CO_PROFILE_NMT_ENTER_OPERATIONAL);
    call = CO_profileTransport_nmtCommand(&transport, CO_PROFILE_NMT_RESET_COMMUNICATION, 127U, &result);
    TEST_ASSERT(call.status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(nmtCallCount == 2U && lastNmtNodeId == 127U
                && lastNmtCommand == CO_PROFILE_NMT_RESET_COMMUNICATION);
    call = CO_profileTransport_nmtCommand(&transport, CO_PROFILE_NMT_ENTER_OPERATIONAL, 128U, &result);
    TEST_ASSERT(call.status == CO_PROFILE_CALL_INVALID_ARGUMENT && nmtCallCount == 2U);
    call = CO_profileTransport_nmtCommand(&transport, (CO_profile_nmt_command_t)0x55U, 1U, &result);
    TEST_ASSERT(call.status == CO_PROFILE_CALL_INVALID_ARGUMENT && nmtCallCount == 2U);
    return true;
}

static bool test_cia401_core_translation_and_bounds(void)
{
    CO_401_controller_t controller;
    CO_401_controller_config_t config = {
        .logicalDevice = 3U,
        .digitalInputBanks = 2U,
        .digitalOutputBanks = 2U,
        .analogInputChannels = 2U,
        .analogOutputChannels = 2U,
    };
    CO_profile_object_ref_t ref;

    TEST_ASSERT(CO_401_controller_init(&controller, &config));
    TEST_ASSERT(CO_401_controller_digitalInputRef(&controller, 1U, &ref));
    TEST_ASSERT(ref.index == 0x7800U && ref.subIndex == 1U && ref.size == 1U);
    TEST_ASSERT(CO_401_controller_digitalOutputRef(&controller, 2U, &ref));
    TEST_ASSERT(ref.index == 0x7A00U && ref.subIndex == 2U && ref.size == 1U);
    TEST_ASSERT(CO_401_controller_analogInputRef(&controller, 1U, &ref));
    TEST_ASSERT(ref.index == 0x7C01U && ref.subIndex == 1U && ref.size == 2U);
    TEST_ASSERT(CO_401_controller_analogOutputRef(&controller, 2U, &ref));
    TEST_ASSERT(ref.index == 0x7C11U && ref.subIndex == 2U && ref.size == 2U);
    TEST_ASSERT(!CO_401_controller_digitalInputRef(&controller, 0U, &ref));
    TEST_ASSERT(!CO_401_controller_digitalInputRef(&controller, 3U, &ref));
    TEST_ASSERT(!CO_401_controller_profileRef(&controller, 0x5FFFU, 0U, 1U, &ref));
    TEST_ASSERT(!CO_401_controller_profileRef(&controller, 0x6000U, 0U, 0U, &ref));

    config.logicalDevice = 8U;
    TEST_ASSERT(!CO_401_controller_init(&controller, &config));
    config.logicalDevice = 0U;
    config.digitalInputBanks = 0U;
    config.digitalOutputBanks = 0U;
    config.analogInputChannels = 0U;
    config.analogOutputChannels = 0U;
    TEST_ASSERT(!CO_401_controller_init(&controller, &config));
    return true;
}

static bool test_cia401_adapter_typed_io_and_failure_preservation(void)
{
    CO_profile_transport_t transport;
    CO_401_controller_client_t adapter;
    CO_401_controller_client_config_t config = {
        .nodeId = 5U,
        .controller = {
            .logicalDevice = 3U,
            .digitalInputBanks = 2U,
            .digitalOutputBanks = 2U,
            .analogInputChannels = 2U,
            .analogOutputChannels = 2U,
        },
    };
    CO_profile_transfer_result_t result;
    uint8_t input = 0U;
    int16_t analog = 1234;
    uint8_t u8 = 0xA5U;
    uint8_t analogData[2] = {0x2EU, 0xFBU};
    uint8_t outputData = 0x5AU;
    uint8_t rawData[4] = {1U, 2U, 3U, 4U};

    TEST_ASSERT(CO_profileTransport_init(&transport, NULL, &fakeTransportOps));
    TEST_ASSERT(CO_401_controller_client_init(&adapter, &transport, &config).status == CO_PROFILE_CALL_OK);

    resetTransfers();
    expectUpload(5U, 0x7800U, 1U, 1U, CO_PROFILE_TRANSFER_OK, &u8, 0U);
    TEST_ASSERT(CO_401_controller_client_readDigitalInput(&adapter, 1U, &input, &result).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(input == 0xA5U);
    TEST_ASSERT(transfersComplete());

    resetTransfers();
    expectDownload(5U, 0x7A00U, 2U, &outputData, 1U, CO_PROFILE_TRANSFER_OK, 0U);
    TEST_ASSERT(CO_401_controller_client_writeDigitalOutput(&adapter, 2U, outputData, &result).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(result.status == CO_PROFILE_TRANSFER_OK);
    TEST_ASSERT(transfersComplete());

    resetTransfers();
    expectUpload(5U, 0x7C01U, 2U, 2U, CO_PROFILE_TRANSFER_ABORT, NULL, 0x06020000UL);
    TEST_ASSERT(CO_401_controller_client_readAnalogInput(&adapter, 2U, &analog, &result).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(result.status == CO_PROFILE_TRANSFER_ABORT);
    TEST_ASSERT(analog == 1234);
    TEST_ASSERT(transfersComplete());

    resetTransfers();
    expectDownload(5U, 0x7C11U, 1U, analogData, 2U, CO_PROFILE_TRANSFER_OK, 0U);
    TEST_ASSERT(CO_401_controller_client_writeAnalogOutput(&adapter, 1U, (int16_t)-1234, &result).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(transfersComplete());

    resetTransfers();
    expectDownload(5U, 0x7C23U, 0U, rawData, 4U, CO_PROFILE_TRANSFER_OK, 0U);
    TEST_ASSERT(CO_401_controller_client_writeProfileObject(&adapter, CO_401_INDEX_ANALOG_INTERRUPT_ENABLE,
                                                         0U, rawData, sizeof(rawData), &result).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(transfersComplete());
    return true;
}

static uint16_t mergedControlword(uint16_t current, uint16_t pdsValue)
{
    return (uint16_t)((current & (uint16_t)(~CO_402_CONTROLLER_PDS_CONTROLWORD_MASK))
                      | (pdsValue & CO_402_CONTROLLER_PDS_CONTROLWORD_MASK));
}

static void encodeU16(uint16_t value, uint8_t data[2])
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8U);
}

static bool test_cia402_adapter_translation_merge_and_retry(void)
{
    CO_profile_transport_t transport;
    CO_402_controller_client_t adapter;
    CO_402_controller_client_config_t config = {
        .nodeId = 9U,
        .logicalDevice = 2U,
        .controller = {.transitionTimeout_us = 10000U, .feedbackTimeout_us = 10000U},
    };
    CO_402_controller_client_step_result_t step;
    uint8_t statusData[2];
    uint8_t controlData[2];
    uint8_t mergedData[2];
    uint16_t currentControlword = 0x1230U;
    uint16_t merged;

    TEST_ASSERT(CO_profileTransport_init(&transport, NULL, &fakeTransportOps));
    TEST_ASSERT(CO_402_controller_client_init(&adapter, &transport, &config).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(CO_402_controller_client_setTarget(&adapter, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));

    encodeU16(CO_402_statuswordForState(CO_402_STATE_SWITCH_ON_DISABLED), statusData);
    encodeU16(currentControlword, controlData);
    merged = mergedControlword(currentControlword, 0x0006U);
    encodeU16(merged, mergedData);

    resetTransfers();
    expectUpload(9U, 0x7041U, 0U, 2U, CO_PROFILE_TRANSFER_OK, statusData, 0U);
    expectUpload(9U, 0x7040U, 0U, 2U, CO_PROFILE_TRANSFER_OK, controlData, 0U);
    expectDownload(9U, 0x7040U, 0U, mergedData, 2U, CO_PROFILE_TRANSFER_ABORT, 0x08000020UL);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 1000U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(step.controllerResult == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(step.commandPending);
    TEST_ASSERT(!step.commandAccepted);
    TEST_ASSERT(adapter.pendingUpdateValid);
    TEST_ASSERT(transfersComplete());

    resetTransfers();
    expectUpload(9U, 0x7040U, 0U, 2U, CO_PROFILE_TRANSFER_OK, controlData, 0U);
    expectDownload(9U, 0x7040U, 0U, mergedData, 2U, CO_PROFILE_TRANSFER_OK, 0U);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 9999U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(step.commandAccepted);
    TEST_ASSERT(adapter.pendingUpdateValid == false);
    TEST_ASSERT(transfersComplete());

    /* A fresh status step must not inherit retry time from a command that was not yet accepted remotely. */
    resetTransfers();
    expectUpload(9U, 0x7041U, 0U, 2U, CO_PROFILE_TRANSFER_OK, statusData, 0U);
    expectUpload(9U, 0x7040U, 0U, 2U, CO_PROFILE_TRANSFER_OK, mergedData, 0U);
    expectDownload(9U, 0x7040U, 0U, mergedData, 2U, CO_PROFILE_TRANSFER_OK, 0U);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 1U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(step.controllerResult == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(step.commandAccepted);
    TEST_ASSERT(transfersComplete());
    return true;
}

static bool test_cia402_retry_elapsed_still_ages_feedback(void)
{
    CO_profile_transport_t transport;
    CO_402_controller_client_t adapter;
    CO_402_controller_client_config_t config = {
        .nodeId = 11U,
        .logicalDevice = 0U,
        .controller = {.transitionTimeout_us = 20000U, .feedbackTimeout_us = 10000U},
    };
    CO_402_controller_client_step_result_t step;
    uint8_t statusData[2];
    uint8_t controlData[2];
    uint8_t mergedData[2];
    uint16_t currentControlword = 0x2200U;

    TEST_ASSERT(CO_profileTransport_init(&transport, NULL, &fakeTransportOps));
    TEST_ASSERT(CO_402_controller_client_init(&adapter, &transport, &config).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(CO_402_controller_client_setTarget(&adapter, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));
    encodeU16(CO_402_statuswordForState(CO_402_STATE_SWITCH_ON_DISABLED), statusData);
    encodeU16(currentControlword, controlData);
    encodeU16(mergedControlword(currentControlword, 0x0006U), mergedData);

    resetTransfers();
    expectUpload(11U, 0x6041U, 0U, 2U, CO_PROFILE_TRANSFER_OK, statusData, 0U);
    expectUpload(11U, 0x6040U, 0U, 2U, CO_PROFILE_TRANSFER_OK, controlData, 0U);
    expectDownload(11U, 0x6040U, 0U, mergedData, 2U, CO_PROFILE_TRANSFER_ABORT, 0x08000020UL);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 1000U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(step.commandPending);
    TEST_ASSERT(transfersComplete());

    resetTransfers();
    expectUpload(11U, 0x6040U, 0U, 2U, CO_PROFILE_TRANSFER_OK, controlData, 0U);
    expectDownload(11U, 0x6040U, 0U, mergedData, 2U, CO_PROFILE_TRANSFER_OK, 0U);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 9999U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(step.commandAccepted);
    TEST_ASSERT(transfersComplete());

    /* The same retry interval still counts toward feedback staleness when no fresh Statusword arrives. */
    resetTransfers();
    expectUpload(11U, 0x6041U, 0U, 2U, CO_PROFILE_TRANSFER_ABORT, NULL, 0x08000020UL);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 1U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(step.controllerResult == CO_402_CONTROLLER_RESULT_FEEDBACK_TIMEOUT);
    TEST_ASSERT(adapter.controller.feedbackElapsed_us == 10000U);
    TEST_ASSERT(transfersComplete());
    return true;
}

static bool test_cia402_status_call_failure_then_transfer_abort_ages_feedback_once(void)
{
    CO_profile_transport_t transport;
    CO_402_controller_client_t adapter;
    CO_402_controller_client_config_t config = {
        .nodeId = 15U,
        .logicalDevice = 0U,
        .controller = {.transitionTimeout_us = 0U, .feedbackTimeout_us = 10000U},
    };
    CO_402_controller_client_step_result_t step;
    uint8_t statusData[2];

    TEST_ASSERT(CO_profileTransport_init(&transport, NULL, &fakeTransportOps));
    TEST_ASSERT(CO_402_controller_client_init(&adapter, &transport, &config).status == CO_PROFILE_CALL_OK);
    encodeU16(CO_402_statuswordForState(CO_402_STATE_SWITCH_ON_DISABLED), statusData);

    resetTransfers();
    expectUpload(15U, 0x6041U, 0U, 2U, CO_PROFILE_TRANSFER_OK, statusData, 0U);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 1U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(step.controllerResult == CO_402_CONTROLLER_RESULT_IDLE);
    TEST_ASSERT(adapter.controller.feedbackElapsed_us == 0U);
    TEST_ASSERT(transfersComplete());

    resetTransfers();
    expectUploadCallFailure(15U, 0x6041U, 0U, 2U, CO_PROFILE_CALL_BUSY, 0);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 4000U, &step).status == CO_PROFILE_CALL_BUSY);
    TEST_ASSERT(adapter.deferredElapsedUs == 4000U);
    TEST_ASSERT(transfersComplete());

    resetTransfers();
    expectUpload(15U, 0x6041U, 0U, 2U, CO_PROFILE_TRANSFER_ABORT, NULL, 0x08000020UL);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 5999U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(step.controllerResult == CO_402_CONTROLLER_RESULT_IDLE);
    TEST_ASSERT(adapter.controller.feedbackElapsed_us == 9999U);
    TEST_ASSERT(adapter.deferredElapsedUs == 0U);
    TEST_ASSERT(transfersComplete());

    resetTransfers();
    expectUpload(15U, 0x6041U, 0U, 2U, CO_PROFILE_TRANSFER_ABORT, NULL, 0x08000020UL);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 1U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(step.controllerResult == CO_402_CONTROLLER_RESULT_FEEDBACK_TIMEOUT);
    TEST_ASSERT(adapter.controller.feedbackElapsed_us == 10000U);
    TEST_ASSERT(transfersComplete());
    return true;
}

static bool test_cia402_status_call_failure_ages_active_transition(void)
{
    CO_profile_transport_t transport;
    CO_402_controller_client_t adapter;
    CO_402_controller_client_config_t config = {
        .nodeId = 12U,
        .logicalDevice = 0U,
        .controller = {.transitionTimeout_us = 10000U, .feedbackTimeout_us = 20000U},
    };
    CO_402_controller_client_step_result_t step;
    CO_profile_call_result_t call;
    uint8_t statusData[2];
    uint8_t controlData[2];
    uint8_t shutdownData[2];
    uint16_t currentControlword = 0x3300U;

    TEST_ASSERT(CO_profileTransport_init(&transport, NULL, &fakeTransportOps));
    TEST_ASSERT(CO_402_controller_client_init(&adapter, &transport, &config).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(CO_402_controller_client_setTarget(&adapter, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));
    encodeU16(CO_402_statuswordForState(CO_402_STATE_SWITCH_ON_DISABLED), statusData);
    encodeU16(currentControlword, controlData);
    encodeU16(mergedControlword(currentControlword, 0x0006U), shutdownData);

    resetTransfers();
    expectUpload(12U, 0x6041U, 0U, 2U, CO_PROFILE_TRANSFER_OK, statusData, 0U);
    expectUpload(12U, 0x6040U, 0U, 2U, CO_PROFILE_TRANSFER_OK, controlData, 0U);
    expectDownload(12U, 0x6040U, 0U, shutdownData, 2U, CO_PROFILE_TRANSFER_OK, 0U);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 1000U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(step.controllerResult == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(step.commandAccepted);
    TEST_ASSERT(adapter.controller.transitionElapsed_us == 0U);
    TEST_ASSERT(transfersComplete());

    resetTransfers();
    expectUploadCallFailure(12U, 0x6041U, 0U, 2U, CO_PROFILE_CALL_BUSY, 0);
    call = CO_402_controller_client_process(&adapter, 4000U, &step);
    TEST_ASSERT(call.status == CO_PROFILE_CALL_BUSY && call.backendError == 0);
    TEST_ASSERT(step.statuswordTransfer.status == CO_PROFILE_TRANSFER_LOCAL_ERROR);
    TEST_ASSERT(adapter.deferredElapsedUs == 4000U);
    TEST_ASSERT(transfersComplete());

    resetTransfers();
    expectUploadCallFailure(12U, 0x6041U, 0U, 2U, CO_PROFILE_CALL_BACKEND_ERROR, -4321);
    call = CO_402_controller_client_process(&adapter, 5000U, &step);
    TEST_ASSERT(call.status == CO_PROFILE_CALL_BACKEND_ERROR && call.backendError == -4321);
    TEST_ASSERT(adapter.deferredElapsedUs == 9000U);
    TEST_ASSERT(transfersComplete());

    resetTransfers();
    expectUpload(12U, 0x6041U, 0U, 2U, CO_PROFILE_TRANSFER_OK, statusData, 0U);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 1000U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(step.controllerResult == CO_402_CONTROLLER_RESULT_TRANSITION_TIMEOUT);
    TEST_ASSERT(adapter.controller.transitionElapsed_us == 10000U);
    TEST_ASSERT(adapter.deferredElapsedUs == 0U);
    TEST_ASSERT(!step.commandPending && !step.commandAccepted);
    TEST_ASSERT(transfersComplete());
    return true;
}

static bool test_cia402_status_call_failure_accepts_observed_state_progress(void)
{
    CO_profile_transport_t transport;
    CO_402_controller_client_t adapter;
    CO_402_controller_client_config_t config = {
        .nodeId = 13U,
        .logicalDevice = 0U,
        .controller = {.transitionTimeout_us = 5000U, .feedbackTimeout_us = 20000U},
    };
    CO_402_controller_client_step_result_t step;
    uint8_t disabledStatus[2];
    uint8_t readyStatus[2];
    uint8_t controlData[2];
    uint8_t shutdownData[2];
    uint8_t switchOnData[2];
    uint16_t currentControlword = 0x5500U;
    uint16_t shutdownControlword;

    TEST_ASSERT(CO_profileTransport_init(&transport, NULL, &fakeTransportOps));
    TEST_ASSERT(CO_402_controller_client_init(&adapter, &transport, &config).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(CO_402_controller_client_setTarget(&adapter, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));
    encodeU16(CO_402_statuswordForState(CO_402_STATE_SWITCH_ON_DISABLED), disabledStatus);
    encodeU16(CO_402_statuswordForState(CO_402_STATE_READY_TO_SWITCH_ON), readyStatus);
    encodeU16(currentControlword, controlData);
    shutdownControlword = mergedControlword(currentControlword, 0x0006U);
    encodeU16(shutdownControlword, shutdownData);
    encodeU16(mergedControlword(shutdownControlword, 0x0007U), switchOnData);

    resetTransfers();
    expectUpload(13U, 0x6041U, 0U, 2U, CO_PROFILE_TRANSFER_OK, disabledStatus, 0U);
    expectUpload(13U, 0x6040U, 0U, 2U, CO_PROFILE_TRANSFER_OK, controlData, 0U);
    expectDownload(13U, 0x6040U, 0U, shutdownData, 2U, CO_PROFILE_TRANSFER_OK, 0U);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 1U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(step.commandAccepted);
    TEST_ASSERT(transfersComplete());

    resetTransfers();
    expectUploadCallFailure(13U, 0x6041U, 0U, 2U, CO_PROFILE_CALL_BUSY, 0);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 6000U, &step).status == CO_PROFILE_CALL_BUSY);
    TEST_ASSERT(adapter.deferredElapsedUs == 6000U);
    TEST_ASSERT(transfersComplete());

    resetTransfers();
    expectUpload(13U, 0x6041U, 0U, 2U, CO_PROFILE_TRANSFER_OK, readyStatus, 0U);
    expectUpload(13U, 0x6040U, 0U, 2U, CO_PROFILE_TRANSFER_OK, shutdownData, 0U);
    expectDownload(13U, 0x6040U, 0U, switchOnData, 2U, CO_PROFILE_TRANSFER_OK, 0U);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 1U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(step.controllerResult == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(step.commandAccepted);
    TEST_ASSERT(adapter.controller.transitionElapsed_us == 0U);
    TEST_ASSERT(adapter.deferredElapsedUs == 0U);
    TEST_ASSERT(transfersComplete());
    return true;
}

static bool test_cia402_status_call_failure_does_not_leak_across_target_rebuild(void)
{
    CO_profile_transport_t transport;
    CO_402_controller_client_t adapter;
    CO_402_controller_client_config_t config = {
        .nodeId = 14U,
        .logicalDevice = 0U,
        .controller = {.transitionTimeout_us = 5000U, .feedbackTimeout_us = 20000U},
    };
    CO_402_controller_client_step_result_t step;
    uint8_t statusData[2];
    uint8_t controlData[2];
    uint8_t shutdownData[2];
    uint16_t currentControlword = 0x6600U;

    TEST_ASSERT(CO_profileTransport_init(&transport, NULL, &fakeTransportOps));
    TEST_ASSERT(CO_402_controller_client_init(&adapter, &transport, &config).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(CO_402_controller_client_setTarget(&adapter, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));
    encodeU16(CO_402_statuswordForState(CO_402_STATE_SWITCH_ON_DISABLED), statusData);
    encodeU16(currentControlword, controlData);
    encodeU16(mergedControlword(currentControlword, 0x0006U), shutdownData);

    resetTransfers();
    expectUpload(14U, 0x6041U, 0U, 2U, CO_PROFILE_TRANSFER_OK, statusData, 0U);
    expectUpload(14U, 0x6040U, 0U, 2U, CO_PROFILE_TRANSFER_OK, controlData, 0U);
    expectDownload(14U, 0x6040U, 0U, shutdownData, 2U, CO_PROFILE_TRANSFER_OK, 0U);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 1U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(step.commandAccepted);
    TEST_ASSERT(transfersComplete());

    resetTransfers();
    expectUploadCallFailure(14U, 0x6041U, 0U, 2U, CO_PROFILE_CALL_BUSY, 0);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 6000U, &step).status == CO_PROFILE_CALL_BUSY);
    TEST_ASSERT(adapter.deferredElapsedUs == 6000U);
    TEST_ASSERT(transfersComplete());

    TEST_ASSERT(CO_402_controller_client_setTarget(&adapter, CO_402_CONTROLLER_TARGET_READY_TO_SWITCH_ON));
    TEST_ASSERT(adapter.deferredElapsedUs == 0U);
    TEST_ASSERT(adapter.controller.feedbackElapsed_us == 6000U);
    TEST_ASSERT(adapter.controller.transitionElapsed_us == 0U);

    resetTransfers();
    expectUpload(14U, 0x6041U, 0U, 2U, CO_PROFILE_TRANSFER_OK, statusData, 0U);
    expectUpload(14U, 0x6040U, 0U, 2U, CO_PROFILE_TRANSFER_OK, shutdownData, 0U);
    expectDownload(14U, 0x6040U, 0U, shutdownData, 2U, CO_PROFILE_TRANSFER_OK, 0U);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 1U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(step.controllerResult == CO_402_CONTROLLER_RESULT_IN_PROGRESS);
    TEST_ASSERT(step.commandAccepted);
    TEST_ASSERT(adapter.controller.transitionElapsed_us == 1U);
    TEST_ASSERT(transfersComplete());

    resetTransfers();
    expectUploadCallFailure(14U, 0x6041U, 0U, 2U, CO_PROFILE_CALL_BACKEND_ERROR, -77);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 6000U, &step).status == CO_PROFILE_CALL_BACKEND_ERROR);
    TEST_ASSERT(adapter.deferredElapsedUs == 6000U);
    TEST_ASSERT(transfersComplete());

    CO_402_controller_client_clearTarget(&adapter);
    TEST_ASSERT(adapter.deferredElapsedUs == 0U);
    TEST_ASSERT(adapter.controller.transitionElapsed_us == 0U);

    resetTransfers();
    expectUpload(14U, 0x6041U, 0U, 2U, CO_PROFILE_TRANSFER_OK, statusData, 0U);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 1U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(step.controllerResult == CO_402_CONTROLLER_RESULT_IDLE);
    TEST_ASSERT(adapter.controller.transitionElapsed_us == 0U);
    TEST_ASSERT(transfersComplete());
    return true;
}

static bool test_cia402_fault_reset_edges_survive_transport_failure(void)
{
    CO_profile_transport_t transport;
    CO_402_controller_client_t adapter;
    CO_402_controller_client_config_t config = {
        .nodeId = 10U,
        .logicalDevice = 0U,
        .controller = {.transitionTimeout_us = 10000U, .feedbackTimeout_us = 10000U},
    };
    CO_402_controller_client_step_result_t step;
    uint8_t faultStatus[2];
    uint8_t disabledStatus[2];
    uint8_t currentData[2];
    uint8_t highData[2];
    uint8_t lowData[2];
    uint8_t shutdownData[2];
    uint16_t current = 0x4400U;

    TEST_ASSERT(CO_profileTransport_init(&transport, NULL, &fakeTransportOps));
    TEST_ASSERT(CO_402_controller_client_init(&adapter, &transport, &config).status == CO_PROFILE_CALL_OK);
    encodeU16(CO_402_statuswordForState(CO_402_STATE_FAULT), faultStatus);
    encodeU16(CO_402_statuswordForState(CO_402_STATE_SWITCH_ON_DISABLED), disabledStatus);
    encodeU16(current, currentData);
    encodeU16(mergedControlword(current, 0x0080U), highData);
    encodeU16(mergedControlword(current, 0x0000U), lowData);
    encodeU16(mergedControlword(current, 0x0006U), shutdownData);

    resetTransfers();
    expectUpload(10U, 0x6041U, 0U, 2U, CO_PROFILE_TRANSFER_OK, faultStatus, 0U);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 1000U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(step.controllerResult == CO_402_CONTROLLER_RESULT_REMOTE_FAULT);
    TEST_ASSERT(transfersComplete());
    TEST_ASSERT(CO_402_controller_client_requestFaultReset(&adapter));

    resetTransfers();
    expectUpload(10U, 0x6041U, 0U, 2U, CO_PROFILE_TRANSFER_OK, faultStatus, 0U);
    expectUpload(10U, 0x6040U, 0U, 2U, CO_PROFILE_TRANSFER_OK, currentData, 0U);
    expectDownload(10U, 0x6040U, 0U, highData, 2U, CO_PROFILE_TRANSFER_ABORT, 0x08000020UL);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 1000U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(adapter.pendingFaultResetEdge);
    TEST_ASSERT(transfersComplete());

    CO_402_controller_client_clearTarget(&adapter);
    TEST_ASSERT(adapter.pendingUpdateValid);
    resetTransfers();
    expectUpload(10U, 0x6040U, 0U, 2U, CO_PROFILE_TRANSFER_OK, currentData, 0U);
    expectDownload(10U, 0x6040U, 0U, highData, 2U, CO_PROFILE_TRANSFER_OK, 0U);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 9000U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(step.commandAccepted);
    TEST_ASSERT(transfersComplete());

    resetTransfers();
    expectUpload(10U, 0x6041U, 0U, 2U, CO_PROFILE_TRANSFER_OK, disabledStatus, 0U);
    expectUpload(10U, 0x6040U, 0U, 2U, CO_PROFILE_TRANSFER_OK, currentData, 0U);
    expectDownload(10U, 0x6040U, 0U, lowData, 2U, CO_PROFILE_TRANSFER_ABORT, 0x08000020UL);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 1000U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(adapter.pendingFaultResetEdge);
    TEST_ASSERT(!CO_402_controller_client_requestFaultReset(&adapter));
    TEST_ASSERT(!CO_402_controller_client_setTarget(&adapter, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));
    TEST_ASSERT(adapter.pendingUpdateValid);
    TEST_ASSERT(transfersComplete());

    resetTransfers();
    expectUpload(10U, 0x6040U, 0U, 2U, CO_PROFILE_TRANSFER_OK, currentData, 0U);
    expectDownload(10U, 0x6040U, 0U, lowData, 2U, CO_PROFILE_TRANSFER_OK, 0U);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 9000U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(adapter.pendingUpdateValid == false);
    TEST_ASSERT(transfersComplete());
    TEST_ASSERT(CO_402_controller_client_setTarget(&adapter, CO_402_CONTROLLER_TARGET_OPERATION_ENABLED));

    resetTransfers();
    expectUpload(10U, 0x6041U, 0U, 2U, CO_PROFILE_TRANSFER_OK, disabledStatus, 0U);
    expectUpload(10U, 0x6040U, 0U, 2U, CO_PROFILE_TRANSFER_OK, currentData, 0U);
    expectDownload(10U, 0x6040U, 0U, shutdownData, 2U, CO_PROFILE_TRANSFER_OK, 0U);
    TEST_ASSERT(CO_402_controller_client_process(&adapter, 1000U, &step).status == CO_PROFILE_CALL_OK);
    TEST_ASSERT(step.commandAccepted);
    TEST_ASSERT(transfersComplete());
    return true;
}


int main(void)
{
    unsigned passed = 0U;

    passed += test_common_transport_contract() ? 1U : 0U;
    passed += test_cia401_core_translation_and_bounds() ? 1U : 0U;
    passed += test_cia401_adapter_typed_io_and_failure_preservation() ? 1U : 0U;
    passed += test_cia402_adapter_translation_merge_and_retry() ? 1U : 0U;
    passed += test_cia402_retry_elapsed_still_ages_feedback() ? 1U : 0U;
    passed += test_cia402_status_call_failure_then_transfer_abort_ages_feedback_once() ? 1U : 0U;
    passed += test_cia402_status_call_failure_ages_active_transition() ? 1U : 0U;
    passed += test_cia402_status_call_failure_accepts_observed_state_progress() ? 1U : 0U;
    passed += test_cia402_status_call_failure_does_not_leak_across_target_rebuild() ? 1U : 0U;
    passed += test_cia402_fault_reset_edges_survive_transport_failure() ? 1U : 0U;

    if (passed != 10U) {
        fprintf(stderr, "PROFILE_CONTROLLER_ADAPTER_HOST_SUMMARY:%u/10\n", passed);
        return 1;
    }
    printf("PROFILE_CONTROLLER_ADAPTER_HOST_SUMMARY:10/10\n");
    return 0;
}
