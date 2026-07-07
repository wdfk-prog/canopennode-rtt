/**
 * @file CO_storage_RTT.h
 * @brief CANopenNode storage helpers for RT-Thread applications.
 * @details This header declares the common RT-Thread storage frontend helpers and storage operation callbacks.
 * @author wdfk-prog ()
 * @version 1.0.0
 * @date 2026.07.04
 *
 * @copyright Copyright (c) 2026
 *
 * @note :
 * @par 修改日志:
 * Date       Version Author      Description
 * 2026.07.04 1.0.0   wdfk-prog   first version
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef CO_STORAGE_RTT_H_
#define CO_STORAGE_RTT_H_

/* Includes ------------------------------------------------------------------*/

#include "CANopen.h"
#include "storage/CO_storage.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/

/* Exported variables ---------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0

/**
 * @brief Initialize one RT-Thread storage entry.
 *
 * Backend-private data is attached later by the selected backend init operation.
 *
 * @param entry Storage entry to initialize.
 * @param addr Address of the data to store.
 * @param len Length of the data to store.
 * @param subIndexOD Sub-index in OD objects 1010h and 1011h.
 * @param attr CO_storage_attributes_t bit mask.
 */
void co_storage_rtt_entry_init(CO_storage_entry_t *entry,
                               void *addr,
                               size_t len,
                               uint8_t subIndexOD,
                               uint8_t attr);

/**
 * @brief Initialize RT-Thread CANopen storage and read stored data.
 *
 * The selected backend operation table performs backend-specific setup, attaches
 * permanent backend-private data to every entry, registers OD 0x1010/0x1011
 * callbacks, and reads persisted values before CO_CANopenInit() consumes OD data.
 *
 * @param storage Storage object. It must exist permanently.
 * @param CANmodule CAN module passed to CO_storage_init().
 * @param OD_1010_StoreParameters OD entry for 0x1010 Store parameters.
 * @param OD_1011_RestoreDefaultParameters OD entry for 0x1011 Restore default parameters.
 * @param entries Storage entries. The array must exist permanently.
 * @param entriesCount Number of storage entries.
 * @param instanceName Optional application instance name used to separate backend data.
 * @param storageInitError Optional error detail. Stores a one-based failed entry index on read failure.
 * @return CO_ERROR_NO on success, CO_ERROR_ILLEGAL_ARGUMENT on invalid arguments, or another CANopenNode error code.
 */
CO_ReturnError_t co_storage_rtt_init(CO_storage_t *storage,
                                     CO_CANmodule_t *CANmodule,
                                     OD_entry_t *OD_1010_StoreParameters,
                                     OD_entry_t *OD_1011_RestoreDefaultParameters,
                                     CO_storage_entry_t *entries,
                                     uint8_t entriesCount,
                                     const char *instanceName,
                                     uint32_t *storageInitError);

/**
 * @brief Read one CANopenNode storage entry into RAM.
 *
 * @param entry Storage entry read before CO_CANopenInit() consumes OD values.
 * @param CANmodule CAN module passed through co_storage_rtt_init().
 * @return ODR_OK on success or missing data, otherwise an ODR_t error code.
 */
ODR_t co_storage_rtt_read(CO_storage_entry_t *entry, CO_CANmodule_t *CANmodule);

/**
 * @brief Store one CANopenNode storage entry.
 *
 * @param entry Storage entry requested by CANopenNode object 0x1010.
 * @param CANmodule CAN module passed through CO_storage_init().
 * @return ODR_OK on success, otherwise an ODR_t error code.
 */
ODR_t co_storage_rtt_store(CO_storage_entry_t *entry, CO_CANmodule_t *CANmodule);

/**
 * @brief Restore one CANopenNode storage entry to its default state.
 *
 * @param entry Storage entry requested by CANopenNode object 0x1011.
 * @param CANmodule CAN module passed through CO_storage_init().
 * @return ODR_OK on success, otherwise an ODR_t error code.
 */
ODR_t co_storage_rtt_restore(CO_storage_entry_t *entry, CO_CANmodule_t *CANmodule);

/**
 * @brief Process automatic storage entries and report automatic-storage failures.
 *
 * @param storage Storage object initialized by co_storage_rtt_init().
 * @param co CANopen object used to report automatic-storage emergency status. Must not be NULL.
 * @param saveAll True to force all automatic entries to persistent media, false for normal cyclic processing.
 * @return true if automatic storage processing was skipped or completed without backend error, otherwise false.
 */
bool_t co_storage_rtt_auto_process(CO_storage_t *storage, CO_t *co, bool_t saveAll);

#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_STORAGE_RTT_H_ */
