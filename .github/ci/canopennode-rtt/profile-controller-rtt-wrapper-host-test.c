/**
 * @file profile-controller-rtt-wrapper-host-test.c
 * @brief Host checks for the thin RT-Thread compatibility facades over portable profile clients.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "CO_401_controller_RTT.h"
#include "CO_402_controller_RTT.h"

#define TEST_ASSERT(expr)                                                           \
    do {                                                                            \
        if (!(expr)) {                                                              \
            fprintf(stderr, "PROFILE_CONTROLLER_RTT_WRAPPER_FAIL:%s:%d:%s\n",     \
                    __func__, __LINE__, #expr);                                     \
            return false;                                                           \
        }                                                                           \
    } while (0)

static CO_profile_call_result_t backendUpload(void *context, uint8_t remoteNodeId, uint16_t index,
                                               uint8_t subIndex, uint8_t expectedSize,
                                               CO_profile_transfer_result_t *result)
{
    (void)context;
    (void)remoteNodeId;
    (void)index;
    (void)subIndex;
    (void)expectedSize;
    (void)result;
    return (CO_profile_call_result_t){CO_PROFILE_CALL_BACKEND_ERROR, -1234};
}

static CO_profile_call_result_t backendDownload(void *context, uint8_t remoteNodeId, uint16_t index,
                                                 uint8_t subIndex, const void *data, uint8_t size,
                                                 CO_profile_transfer_result_t *result)
{
    (void)context;
    (void)remoteNodeId;
    (void)index;
    (void)subIndex;
    (void)data;
    (void)size;
    (void)result;
    return (CO_profile_call_result_t){CO_PROFILE_CALL_BACKEND_ERROR, -1235};
}

static CO_profile_call_result_t backendNmt(void *context, CO_profile_nmt_command_t command,
                                           uint8_t nodeId, CO_profile_transfer_result_t *result)
{
    (void)context;
    (void)command;
    (void)nodeId;
    (void)result;
    return (CO_profile_call_result_t){CO_PROFILE_CALL_BACKEND_ERROR, -1236};
}

static const CO_profile_transport_ops_t backendOps = {
    .sdoUpload = backendUpload,
    .sdoDownload = backendDownload,
    .nmtCommand = backendNmt,
};

CO_profile_transport_t *CO_profileMasterRTT_transport(CO_profile_master_RTT_t *runtime)
{
    if (runtime == NULL || runtime->attached != RT_TRUE) {
        return NULL;
    }
    return &runtime->portableTransport;
}

static bool test_cia401_wrapper_maps_backend_error(void)
{
    CO_profile_master_RTT_t transport;
    CO_401_controller_RTT_t adapter;
    CO_401_controller_RTT_config_t config = {
        .nodeId = 5U,
        .controller = {
            .logicalDevice = 0U,
            .digitalInputBanks = 1U,
            .digitalOutputBanks = 1U,
            .analogInputChannels = 1U,
            .analogOutputChannels = 1U,
        },
    };
    CO_profile_transfer_result_t result;
    uint8_t value = 0x5AU;

    (void)memset(&transport, 0, sizeof(transport));
    TEST_ASSERT(CO_profileTransport_init(&transport.portableTransport, &transport, &backendOps));
    transport.attached = RT_TRUE;
    TEST_ASSERT(CO_401_controller_RTT_init(&adapter, &transport, &config) == RT_EOK);
    TEST_ASSERT(CO_401_controller_RTT_readDigitalInput(&adapter, 1U, &value, &result) == -1234);
    TEST_ASSERT(value == 0x5AU);
    return true;
}

static bool test_cia402_wrapper_maps_backend_error(void)
{
    CO_profile_master_RTT_t transport;
    CO_402_controller_RTT_t adapter;
    CO_402_controller_RTT_config_t config = {
        .nodeId = 9U,
        .logicalDevice = 0U,
        .controller = {.transitionTimeout_us = 10000U, .feedbackTimeout_us = 10000U},
    };
    CO_402_controller_RTT_step_result_t result;

    (void)memset(&transport, 0, sizeof(transport));
    TEST_ASSERT(CO_profileTransport_init(&transport.portableTransport, &transport, &backendOps));
    transport.attached = RT_TRUE;
    TEST_ASSERT(CO_402_controller_RTT_init(&adapter, &transport, &config) == RT_EOK);
    TEST_ASSERT(CO_402_controller_RTT_process(&adapter, 1000U, &result) == -1234);
    TEST_ASSERT(result.statuswordTransfer.status == CO_PROFILE_TRANSFER_LOCAL_ERROR);
    TEST_ASSERT(result.statuswordTransfer.localError == -RT_EBUSY);
    TEST_ASSERT(result.controlwordReadTransfer.localError == -RT_EBUSY);
    TEST_ASSERT(result.controlwordWriteTransfer.localError == -RT_EBUSY);
    return true;
}

static bool test_cia402_invalid_adapter_does_not_touch_result(void)
{
    CO_402_controller_RTT_step_result_t result;
    CO_402_controller_RTT_step_result_t before;

    (void)memset(&result, 0xA5, sizeof(result));
    before = result;
    TEST_ASSERT(CO_402_controller_RTT_process(NULL, 1000U, &result) == -RT_EINVAL);
    TEST_ASSERT(memcmp(&result, &before, sizeof(result)) == 0);
    return true;
}

int main(void)
{
    unsigned passed = 0U;

    passed += test_cia401_wrapper_maps_backend_error() ? 1U : 0U;
    passed += test_cia402_wrapper_maps_backend_error() ? 1U : 0U;
    passed += test_cia402_invalid_adapter_does_not_touch_result() ? 1U : 0U;
    if (passed != 3U) {
        fprintf(stderr, "PROFILE_CONTROLLER_RTT_WRAPPER_SUMMARY:%u/3\n", passed);
        return 1;
    }
    printf("PROFILE_CONTROLLER_RTT_WRAPPER_SUMMARY:3/3\n");
    return 0;
}
