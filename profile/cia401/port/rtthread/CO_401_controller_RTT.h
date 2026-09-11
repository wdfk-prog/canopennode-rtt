/**
 * @file CO_401_controller_RTT.h
 * @brief RT-Thread compatibility facade for the portable CiA 401 Controller client.
 */

#ifndef CO_401_CONTROLLER_RTT_H_
#define CO_401_CONTROLLER_RTT_H_

#include <stdint.h>

#include <rtthread.h>

#include "CO_401_controller_client.h"
#include "CO_profile_master_RTT.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Source-compatible RT-Thread configuration alias for the portable client declaration. */
typedef CO_401_controller_client_config_t CO_401_controller_RTT_config_t;

/** Source-compatible RT-Thread runtime alias; portable Controller state owns no RT-Thread resources. */
typedef CO_401_controller_client_t CO_401_controller_RTT_t;

/**
 * @brief Initialize one remote CiA 401 client on the common RT-Thread Master backend.
 * @param adapter Caller-owned adapter storage.
 * @param transport Attached common RT-Thread profile Master transport.
 * @param config Remote Node-ID and CiA 401 logical-device declaration.
 * @return RT_EOK on success or an RT-Thread error when the portable client/backend cannot be bound.
 */
rt_err_t CO_401_controller_RTT_init(CO_401_controller_RTT_t *adapter, CO_profile_master_RTT_t *transport,
                                    const CO_401_controller_RTT_config_t *config);

/**
 * @brief Read one configured digital-input bank through the RT-Thread Master backend.
 * @param adapter Initialized adapter.
 * @param bank One-based remote digital-input bank.
 * @param value Output value on successful transfer; unchanged on protocol failure.
 * @param result Terminal stack-neutral transfer result.
 * @return RT_EOK after a terminal transport result or an RT-Thread admission/IPC error.
 */
rt_err_t CO_401_controller_RTT_readDigitalInput(CO_401_controller_RTT_t *adapter, uint8_t bank,
                                                uint8_t *value, CO_profile_transfer_result_t *result);

/**
 * @brief Write one configured digital-output bank through the RT-Thread Master backend.
 * @param adapter Initialized adapter.
 * @param bank One-based remote digital-output bank.
 * @param value Value to write.
 * @param result Terminal stack-neutral transfer result.
 * @return RT_EOK after a terminal transport result or an RT-Thread admission/IPC error.
 */
rt_err_t CO_401_controller_RTT_writeDigitalOutput(CO_401_controller_RTT_t *adapter, uint8_t bank,
                                                  uint8_t value, CO_profile_transfer_result_t *result);

/**
 * @brief Read one configured INTEGER16 analogue-input channel through the RT-Thread Master backend.
 * @param adapter Initialized adapter.
 * @param channel One-based remote analogue-input channel.
 * @param value Output value on successful transfer; unchanged on protocol failure.
 * @param result Terminal stack-neutral transfer result.
 * @return RT_EOK after a terminal transport result or an RT-Thread admission/IPC error.
 */
rt_err_t CO_401_controller_RTT_readAnalogInput(CO_401_controller_RTT_t *adapter, uint8_t channel,
                                               int16_t *value, CO_profile_transfer_result_t *result);

/**
 * @brief Write one configured INTEGER16 analogue-output channel through the RT-Thread Master backend.
 * @param adapter Initialized adapter.
 * @param channel One-based remote analogue-output channel.
 * @param value Value to write.
 * @param result Terminal stack-neutral transfer result.
 * @return RT_EOK after a terminal transport result or an RT-Thread admission/IPC error.
 */
rt_err_t CO_401_controller_RTT_writeAnalogOutput(CO_401_controller_RTT_t *adapter, uint8_t channel,
                                                 int16_t value, CO_profile_transfer_result_t *result);

/**
 * @brief Read one bounded scalar in the remote CiA 401 logical-device profile block.
 * @param adapter Initialized adapter.
 * @param canonicalIndex Canonical CiA 401 index in 0x6000..0x67FF.
 * @param subIndex Remote OD sub-index.
 * @param size Exact scalar width in bytes, range 1..8.
 * @param result Terminal stack-neutral transfer result containing uploaded bytes on success.
 * @return RT_EOK after a terminal transport result or an RT-Thread admission/IPC error.
 */
rt_err_t CO_401_controller_RTT_readProfileObject(CO_401_controller_RTT_t *adapter, uint16_t canonicalIndex,
                                                 uint8_t subIndex, uint8_t size,
                                                 CO_profile_transfer_result_t *result);

/**
 * @brief Write one bounded scalar in the remote CiA 401 logical-device profile block.
 * @param adapter Initialized adapter.
 * @param canonicalIndex Canonical CiA 401 index in 0x6000..0x67FF.
 * @param subIndex Remote OD sub-index.
 * @param data CANopen little-endian scalar bytes.
 * @param size Exact scalar width in bytes, range 1..8.
 * @param result Terminal stack-neutral transfer result.
 * @return RT_EOK after a terminal transport result or an RT-Thread admission/IPC error.
 */
rt_err_t CO_401_controller_RTT_writeProfileObject(CO_401_controller_RTT_t *adapter, uint16_t canonicalIndex,
                                                  uint8_t subIndex, const void *data, uint8_t size,
                                                  CO_profile_transfer_result_t *result);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_401_CONTROLLER_RTT_H_ */
