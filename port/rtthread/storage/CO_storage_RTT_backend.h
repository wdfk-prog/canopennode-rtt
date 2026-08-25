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
     * This is the normal OD-backed Storage initialization path. Auxiliary-only
     * pre-CAN preparation uses aux_init and must not require this callback to be
     * invoked twice during one application startup.
     *
     * @param storage Storage object. It must exist permanently.
     * @param CANmodule CAN module passed to CO_storage_init().
     * @param OD_1010_StoreParameters OD entry for 0x1010 Store parameters.
     * @param OD_1011_RestoreDefaultParameters OD entry for 0x1011 Restore default parameters.
     * @param entries Storage entries. May be NULL when entriesCount is zero; otherwise the array must exist permanently.
     * @param entriesCount Number of normal Storage entries. May be zero when no OD-backed Storage group is configured.
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

#if defined(PKG_CANOPENNODE_LSS_PERSIST)
    /**
     * @brief Prepare the backend auxiliary persistence area before the first CAN initialization.
     *
     * This callback is intentionally separate from init. It may initialize the
     * backend medium and bind backend-private state required by aux_read/aux_write,
     * but it must not register normal OD Storage entries or require init to have
     * run first.
     *
     * @param storage Storage object. It must exist permanently.
     * @param instanceName Optional application instance name used to separate backend data.
     * @param storageInitError Error detail pointer, never NULL when called by co_storage_rtt_aux_init().
     * @return CO_ERROR_NO on success, otherwise a CANopenNode error code.
     */
    CO_ReturnError_t (*aux_init)(CO_storage_t *storage,
                                 const char *instanceName,
                                 uint32_t *storageInitError);

    /**
     * @brief Read bytes from the backend auxiliary persistence area.
     *
     * The auxiliary area is reserved by the selected backend and is separate
     * from normal CO_storage_entry_t payloads. Offsets are relative to that
     * backend-owned area.
     *
     * @param storage Storage object prepared by co_storage_rtt_aux_init() or initialized by co_storage_rtt_init().
     * @param offset Byte offset relative to the auxiliary area.
     * @param data Destination buffer.
     * @param len Number of bytes to read.
     * @return true when all requested bytes are read, otherwise false.
     */
    bool_t (*aux_read)(CO_storage_t *storage, size_t offset, uint8_t *data, size_t len);

    /**
     * @brief Write bytes to the backend auxiliary persistence area.
     *
     * A true return must mean the requested bytes have reached the backend media
     * in program order. Backends with caches or deferred writes must flush them
     * before returning true so commit-last record semantics remain valid.
     *
     * @param storage Storage object prepared by co_storage_rtt_aux_init() or initialized by co_storage_rtt_init().
     * @param offset Byte offset relative to the auxiliary area.
     * @param data Source buffer.
     * @param len Number of bytes to write.
     * @return true when all requested bytes are written, otherwise false.
     */
    bool_t (*aux_write)(CO_storage_t *storage, size_t offset, const uint8_t *data, size_t len);
#endif /* defined(PKG_CANOPENNODE_LSS_PERSIST) */
} CO_storage_rtt_backend_ops_t;

/* Exported variables ---------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/

#if defined(PKG_CANOPENNODE_USING_STORAGE_EEPROM)
/**
 * @brief Get the EEPROM device module for one storage object.
 *
 * The generic EEPROM backend stores this persistent module pointer in every
 * CO_storage_entry_t::storageModule before calling CO_storageEeprom_init(). The
 * built-in AT24CXX provider supplies a weak implementation; another EEPROM
 * provider may supply a strong replacement.
 *
 * @param storage Storage object requesting the EEPROM module.
 * @param instanceName Optional CANopenNodeRTT instance name.
 * @return EEPROM device module pointer, or NULL if no module slot is available.
 */
void *co_storage_rtt_eeprom_module_get(CO_storage_t *storage, const char *instanceName);

#if defined(PKG_CANOPENNODE_LSS_PERSIST)
/**
 * @brief Prepare the EEPROM provider auxiliary area before normal Storage init.
 *
 * A custom EEPROM provider implements this hook independently from CO_eeprom_init()
 * so auxiliary pre-CAN preparation does not require the normal EEPROM Storage
 * initializer to be idempotent.
 *
 * @param storage Storage object that owns the EEPROM provider instance.
 * @param instanceName Optional CANopenNodeRTT instance name.
 * @param storageInitError Optional provider error detail.
 * @return CO_ERROR_NO on success, otherwise a CANopenNode error code.
 */
CO_ReturnError_t co_storage_rtt_eeprom_provider_aux_init(CO_storage_t *storage,
                                                         const char *instanceName,
                                                         uint32_t *storageInitError);

/**
 * @brief Read bytes from the EEPROM provider auxiliary persistence area.
 *
 * @param storage Storage object that owns the EEPROM provider instance.
 * @param offset Byte offset relative to the provider-owned auxiliary area.
 * @param data Destination buffer.
 * @param len Number of bytes to read.
 * @return true when all requested bytes are read, otherwise false.
 */
bool_t co_storage_rtt_eeprom_aux_read(CO_storage_t *storage, size_t offset, uint8_t *data, size_t len);

/**
 * @brief Write bytes to the EEPROM provider auxiliary persistence area.
 *
 * @param storage Storage object that owns the EEPROM provider instance.
 * @param offset Byte offset relative to the provider-owned auxiliary area.
 * @param data Source buffer.
 * @param len Number of bytes to write.
 * @return true when all requested bytes are written, otherwise false.
 */
bool_t co_storage_rtt_eeprom_aux_write(CO_storage_t *storage, size_t offset, const uint8_t *data, size_t len);
#endif /* defined(PKG_CANOPENNODE_LSS_PERSIST) */
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
 * backend does not support cyclic CO_storage_auto handling. When LSS persistence
 * is enabled, aux_init, aux_read and aux_write must also be non-NULL. aux_init is
 * the dedicated pre-CAN auxiliary preparation hook; normal init remains a separate
 * OD-backed Storage initialization path.
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
