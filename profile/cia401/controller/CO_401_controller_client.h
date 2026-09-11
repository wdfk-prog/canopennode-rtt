/**
 * @file CO_401_controller_client.h
 * @brief Portable remote CiA 401 Controller transport adapter.
 */

#ifndef CO_401_CONTROLLER_CLIENT_H_
#define CO_401_CONTROLLER_CLIENT_H_

#include <stdbool.h>
#include <stdint.h>

#include "CO_401_controller.h"
#include "CO_profile_transport.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Remote-node declaration for one portable CiA 401 Controller client. */
typedef struct {
    uint8_t nodeId; /**< Remote CANopen Node-ID in range 1..127. */
    CO_401_controller_config_t controller; /**< Remote logical-device/capability declaration. */
} CO_401_controller_client_config_t;

/** Caller-owned portable client for one remote CiA 401 logical device. */
typedef struct {
    CO_profile_transport_t *transport; /**< Non-owning portable transport binding supplied by the platform backend. */
    CO_401_controller_t controller; /**< Stack-neutral remote CiA 401 Controller core. */
    uint8_t nodeId; /**< Remote CANopen Node-ID. */
    bool initialized; /**< True after successful client initialization. */
} CO_401_controller_client_t;

/**
 * @brief Initialize one remote CiA 401 client without owning transport or OS resources.
 *
 * @p transport and its backend context must outlive @p client. Backend readiness may change across reset/teardown;
 * every operation re-enters the transport contract instead of caching stack-specific protocol objects.
 *
 * @param client Caller-owned client storage.
 * @param transport Initialized portable profile transport binding.
 * @param config Remote Node-ID and CiA 401 logical-device declaration.
 * @return CO_PROFILE_CALL_OK on success or CO_PROFILE_CALL_INVALID_ARGUMENT for invalid input.
 */
CO_profile_call_result_t CO_401_controller_client_init(CO_401_controller_client_t *client,
                                                        CO_profile_transport_t *transport,
                                                        const CO_401_controller_client_config_t *config);

/**
 * @brief Read one configured digital-input bank through the portable transport.
 *
 * @p value is unchanged unless the terminal transfer status is CO_PROFILE_TRANSFER_OK.
 *
 * @param client Initialized portable client.
 * @param bank One-based remote digital-input bank.
 * @param value Output value on successful transfer.
 * @param result Terminal transfer result from the selected backend.
 * @return Portable call/admission result; protocol completion is reported through @p result.
 */
CO_profile_call_result_t CO_401_controller_client_readDigitalInput(CO_401_controller_client_t *client,
                                                                    uint8_t bank, uint8_t *value,
                                                                    CO_profile_transfer_result_t *result);

/**
 * @brief Write one configured digital-output bank through the portable transport.
 *
 * @param client Initialized portable client.
 * @param bank One-based remote digital-output bank.
 * @param value Value to write.
 * @param result Terminal transfer result from the selected backend.
 * @return Portable call/admission result; protocol completion is reported through @p result.
 */
CO_profile_call_result_t CO_401_controller_client_writeDigitalOutput(CO_401_controller_client_t *client,
                                                                      uint8_t bank, uint8_t value,
                                                                      CO_profile_transfer_result_t *result);

/**
 * @brief Read one configured INTEGER16 analogue-input channel through the portable transport.
 *
 * @p value is unchanged unless the terminal transfer status is CO_PROFILE_TRANSFER_OK.
 *
 * @param client Initialized portable client.
 * @param channel One-based remote analogue-input channel.
 * @param value Output value on successful transfer.
 * @param result Terminal transfer result from the selected backend.
 * @return Portable call/admission result; protocol completion is reported through @p result.
 */
CO_profile_call_result_t CO_401_controller_client_readAnalogInput(CO_401_controller_client_t *client,
                                                                   uint8_t channel, int16_t *value,
                                                                   CO_profile_transfer_result_t *result);

/**
 * @brief Write one configured INTEGER16 analogue-output channel through the portable transport.
 *
 * @param client Initialized portable client.
 * @param channel One-based remote analogue-output channel.
 * @param value Value to write.
 * @param result Terminal transfer result from the selected backend.
 * @return Portable call/admission result; protocol completion is reported through @p result.
 */
CO_profile_call_result_t CO_401_controller_client_writeAnalogOutput(CO_401_controller_client_t *client,
                                                                     uint8_t channel, int16_t value,
                                                                     CO_profile_transfer_result_t *result);

/**
 * @brief Read one bounded scalar in the remote CiA 401 logical-device profile block.
 *
 * The caller supplies a canonical CiA 401 index; the Controller core resolves the selected logical-device block before
 * the portable backend sees the absolute OD index.
 *
 * @param client Initialized portable client.
 * @param canonicalIndex Canonical CiA 401 index in 0x6000..0x67FF.
 * @param subIndex Remote OD sub-index.
 * @param size Exact scalar width in bytes, range 1..8.
 * @param result Terminal transfer result containing uploaded bytes on success.
 * @return Portable call/admission result; protocol completion is reported through @p result.
 */
CO_profile_call_result_t CO_401_controller_client_readProfileObject(CO_401_controller_client_t *client,
                                                                     uint16_t canonicalIndex,
                                                                     uint8_t subIndex, uint8_t size,
                                                                     CO_profile_transfer_result_t *result);

/**
 * @brief Write one bounded scalar in the remote CiA 401 logical-device profile block.
 *
 * @param client Initialized portable client.
 * @param canonicalIndex Canonical CiA 401 index in 0x6000..0x67FF.
 * @param subIndex Remote OD sub-index.
 * @param data CANopen little-endian scalar bytes.
 * @param size Exact scalar width in bytes, range 1..8.
 * @param result Terminal transfer result from the selected backend.
 * @return Portable call/admission result; protocol completion is reported through @p result.
 */
CO_profile_call_result_t CO_401_controller_client_writeProfileObject(CO_401_controller_client_t *client,
                                                                      uint16_t canonicalIndex,
                                                                      uint8_t subIndex, const void *data,
                                                                      uint8_t size,
                                                                      CO_profile_transfer_result_t *result);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_401_CONTROLLER_CLIENT_H_ */
