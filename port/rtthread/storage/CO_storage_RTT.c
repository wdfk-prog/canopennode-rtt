/**
 * @file CO_storage_RTT.c
 * @brief RT-Thread CANopenNode storage frontend.
 * @details This file implements the common storage frontend used by the RT-Thread
 *          CANopenNode port and dispatches storage operations to the selected backend
 *          implementation.
 * @author wdfk-prog ()
 * @version 1.0.0
 * @date 2026.07.04
 *
 * @copyright Copyright (c) 2026
 *
 * @note :
 * @par Change Log:
 * Date       Version Author      Description
 * 2026.07.04 1.0.0   wdfk-prog   first version
 */

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/
#include "CO_storage_RTT_backend.h"

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0
#include <string.h>
#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */

/* Private define ------------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0

/** Selected backend operations after successful validation in co_storage_rtt_init(). */
static const CO_storage_rtt_backend_ops_t *co_storage_rtt_ops;

/**
 * @brief Validate the selected backend operation table.
 *
 * @param ops Backend operation table returned by co_storage_rtt_backend_get_ops().
 * @return true if all required operations are present, otherwise false.
 */
static bool_t co_storage_rtt_ops_valid(const CO_storage_rtt_backend_ops_t *ops)
{
    bool_t valid = ((ops != NULL) && (ops->init != NULL) && (ops->read != NULL) && (ops->store != NULL)
                    && (ops->restore != NULL));

#if defined(PKG_CANOPENNODE_LSS_PERSIST)
    valid = valid && (ops->aux_read != NULL) && (ops->aux_write != NULL);
#endif /* defined(PKG_CANOPENNODE_LSS_PERSIST) */

    return valid;
}

/**
 * @brief Initialize one RT-Thread storage entry.
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
                               uint8_t attr)
{
    if (entry == NULL) {
        return;
    }

    memset(entry, 0, sizeof(*entry));
    entry->addr = addr;
    entry->len = len;
    entry->subIndexOD = subIndexOD;
    entry->attr = attr;
    entry->storageModule = NULL;
}

/**
 * @brief Initialize RT-Thread CANopen storage and read stored data.
 *
 * @param storage Storage object. It must exist permanently.
 * @param CANmodule CAN module passed to CO_storage_init().
 * @param OD_1010_StoreParameters OD entry for 0x1010 Store parameters.
 * @param OD_1011_RestoreDefaultParameters OD entry for 0x1011 Restore default parameters.
 * @param entries Storage entries. May be NULL when entriesCount is zero; otherwise the array must exist permanently.
 * @param entriesCount Number of storage entries. May be zero when only backend auxiliary persistence is used.
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
                                     uint32_t *storageInitError)
{
    uint32_t storageInitErrorLocal = 0U;

    co_storage_rtt_ops = co_storage_rtt_backend_get_ops();

    if (storageInitError == NULL) {
        storageInitError = &storageInitErrorLocal;
    }
    *storageInitError = 0U;

    if (storage != NULL) {
        storage->enabled = false;
    }
    if ((storage == NULL) || ((entries == NULL) && (entriesCount > 0U)) || !co_storage_rtt_ops_valid(co_storage_rtt_ops)) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return co_storage_rtt_ops->init(storage, CANmodule, OD_1010_StoreParameters, OD_1011_RestoreDefaultParameters,
                                    entries, entriesCount, instanceName, storageInitError);
}

/**
 * @brief Read one CANopenNode storage entry into RAM.
 *
 * @param entry Storage entry read before CO_CANopenInit() consumes OD values.
 * @param CANmodule CAN module passed through co_storage_rtt_init().
 * @return ODR_OK on success or missing data, otherwise an ODR_t error code.
 */
ODR_t co_storage_rtt_read(CO_storage_entry_t *entry, CO_CANmodule_t *CANmodule)
{
    return co_storage_rtt_ops->read(entry, CANmodule);
}

/**
 * @brief Store one CANopenNode storage entry.
 *
 * @param entry Storage entry requested by CANopenNode object 0x1010.
 * @param CANmodule CAN module passed through CO_storage_init().
 * @return ODR_OK on success, otherwise an ODR_t error code.
 */
ODR_t co_storage_rtt_store(CO_storage_entry_t *entry, CO_CANmodule_t *CANmodule)
{
    return co_storage_rtt_ops->store(entry, CANmodule);
}

/**
 * @brief Restore one CANopenNode storage entry to its default state.
 *
 * @param entry Storage entry requested by CANopenNode object 0x1011.
 * @param CANmodule CAN module passed through CO_storage_init().
 * @return ODR_OK on success, otherwise an ODR_t error code.
 */
ODR_t co_storage_rtt_restore(CO_storage_entry_t *entry, CO_CANmodule_t *CANmodule)
{
    return co_storage_rtt_ops->restore(entry, CANmodule);
}

/**
 * @brief Process automatic storage entries and report automatic-storage failures.
 *
 * @param storage Storage object initialized by co_storage_rtt_init().
 * @param co CANopen object used to report automatic-storage emergency status. Must not be NULL.
 * @param saveAll True to force all automatic entries to persistent media, false for normal cyclic processing.
 * @return true if automatic storage processing was skipped or completed without backend error, otherwise false.
 */
bool_t co_storage_rtt_auto_process(CO_storage_t *storage, CO_t *co, bool_t saveAll)
{
    bool_t ok = true;

    if ((storage == NULL) || !storage->enabled) {
        return true;
    }

    if (co_storage_rtt_ops->auto_process != NULL) {
        ok = co_storage_rtt_ops->auto_process(storage, saveAll);
    }

    if (ok) {
        CO_errorReset(co->em, CO_EM_NON_VOLATILE_AUTO_SAVE, 0U);
    } else {
        CO_errorReport(co->em, CO_EM_NON_VOLATILE_AUTO_SAVE, CO_EMC_HARDWARE, 0U);
    }

    return ok;
}

#if defined(PKG_CANOPENNODE_LSS_PERSIST)
bool_t co_storage_rtt_aux_read(CO_storage_t *storage, size_t offset, uint8_t *data, size_t len)
{
    if ((storage == NULL) || (data == NULL) || (len == 0U) || (co_storage_rtt_ops == NULL)
        || (co_storage_rtt_ops->aux_read == NULL)) {
        return false;
    }

    return co_storage_rtt_ops->aux_read(storage, offset, data, len);
}

bool_t co_storage_rtt_aux_write(CO_storage_t *storage, size_t offset, const uint8_t *data, size_t len)
{
    if ((storage == NULL) || (data == NULL) || (len == 0U) || (co_storage_rtt_ops == NULL)
        || (co_storage_rtt_ops->aux_write == NULL)) {
        return false;
    }

    return co_storage_rtt_ops->aux_write(storage, offset, data, len);
}
#endif /* defined(PKG_CANOPENNODE_LSS_PERSIST) */

#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */
