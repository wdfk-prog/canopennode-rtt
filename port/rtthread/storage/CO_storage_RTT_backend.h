/**
 * @file CO_storage_RTT_backend.h
 * @brief RT-Thread CANopenNode storage backend interface.
 * @details This header declares the storage backend operation table used by the
 *          RT-Thread storage frontend and built-in storage backends.
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
#ifndef CO_STORAGE_RTT_BACKEND_H_
#define CO_STORAGE_RTT_BACKEND_H_

/* Includes ------------------------------------------------------------------*/

#include "CO_storage_RTT.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0

/**
 * @brief RT-Thread storage backend operation table.
 */
typedef struct {
    /**
     * @brief Initialize the selected storage backend and read persisted entries.
     *
     * @param storage Storage object. It must exist permanently.
     * @param CANmodule CAN module passed to CO_storage_init().
     * @param OD_1010_StoreParameters OD entry for 0x1010 Store parameters.
     * @param OD_1011_RestoreDefaultParameters OD entry for 0x1011 Restore default parameters.
     * @param entries Storage entries. The array must exist permanently.
     * @param entriesCount Number of storage entries.
     * @param instanceName Optional application instance name used to separate backend data.
     * @param storageInitError Error detail pointer, never NULL when called by co_storage_rtt_init().
     * @return CO_ERROR_NO on success, otherwise a CANopenNode error code.
     */
    CO_ReturnError_t (*init)(CO_storage_t *storage,
                             CO_CANmodule_t *CANmodule,
                             OD_entry_t *OD_1010_StoreParameters,
                             OD_entry_t *OD_1011_RestoreDefaultParameters,
                             CO_storage_entry_t *entries,
                             uint8_t entriesCount,
                             const char *instanceName,
                             uint32_t *storageInitError);

    /**
     * @brief Read one storage entry into RAM.
     *
     * @param entry Storage entry that must be populated before CO_CANopenInit() consumes OD values.
     * @param CANmodule CAN module originally passed to co_storage_rtt_init().
     * @return ODR_OK on success or missing first-startup data, otherwise an ODR_t error code.
     */
    ODR_t (*read)(CO_storage_entry_t *entry, CO_CANmodule_t *CANmodule);

    /**
     * @brief Store one storage entry to persistent media.
     *
     * @param entry Storage entry requested by CANopenNode object 0x1010.
     * @param CANmodule CAN module originally passed to co_storage_rtt_init().
     * @return ODR_OK on success, otherwise an ODR_t error code.
     */
    ODR_t (*store)(CO_storage_entry_t *entry, CO_CANmodule_t *CANmodule);

    /**
     * @brief Restore one storage entry to backend default state.
     *
     * @param entry Storage entry requested by CANopenNode object 0x1011.
     * @param CANmodule CAN module originally passed to co_storage_rtt_init().
     * @return ODR_OK on success, otherwise an ODR_t error code.
     */
    ODR_t (*restore)(CO_storage_entry_t *entry, CO_CANmodule_t *CANmodule);

    /**
     * @brief Process automatic storage entries.
     *
     * Backends that support CO_storage_auto use this callback to persist automatic
     * entries from the application main loop. A false return means at least one
     * automatic entry failed and the frontend should keep/report the automatic
     * storage error state.
     *
     * @param storage Storage object initialized by co_storage_rtt_init().
     * @param saveAll True to force all automatic entries to persistent media, false for normal cyclic processing.
     * @return true if automatic processing completed without backend error, otherwise false.
     */
    bool_t (*auto_process)(CO_storage_t *storage, bool_t saveAll);
} CO_storage_rtt_backend_ops_t;

/* Exported variables ---------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/

#if defined(PKG_CANOPENNODE_USING_STORAGE_EEPROM)
/**
 * @brief Get the EEPROM device module for one storage object.
 *
 * The built-in EEPROM storage backend stores this persistent module pointer in
 * every CO_storage_entry_t::storageModule before calling CO_storageEeprom_init().
 *
 * @param storage Storage object requesting the EEPROM module.
 * @param instanceName Optional CANopenNodeRTT instance name.
 * @return EEPROM device module pointer, or NULL if no module slot is available.
 */
void *co_storage_rtt_eeprom_module_get(CO_storage_t *storage, const char *instanceName);
#endif /* defined(PKG_CANOPENNODE_USING_STORAGE_EEPROM) */

/**
 * @brief Get selected RT-Thread storage backend operations.
 *
 * Built-in backend translation units provide rt_weak definitions for this symbol.
 * A user backend selected by PKG_CANOPENNODE_USING_STORAGE_USER must provide a
 * strong definition. A strong application definition may also override a weak
 * built-in backend when a package backend is selected. The returned operation
 * table must have static or otherwise permanent lifetime; init, read, store and
 * restore callbacks must be non-NULL, while auto_process may be NULL if the
 * backend does not support cyclic CO_storage_auto handling.
 *
 * @return Selected backend operation table, or NULL to make co_storage_rtt_init() fail with
 * CO_ERROR_ILLEGAL_ARGUMENT.
 */
const CO_storage_rtt_backend_ops_t *co_storage_rtt_backend_get_ops(void);

#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_STORAGE_RTT_BACKEND_H_ */
