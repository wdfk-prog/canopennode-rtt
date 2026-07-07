/**
 * @file CO_storage_RTT_eeprom.c
 * @brief Generic EEPROM storage backend for RT-Thread CANopenNode storage.
 * @details This file implements the generic EEPROM backend adapter around CANopenNode
 *          storageEeprom and the RT-Thread EEPROM module provider.
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

#if (((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0) && defined(PKG_CANOPENNODE_USING_STORAGE_EEPROM)

#include "301/crc16-ccitt.h"
#include "co_rtt_log.h"
#include "storage/CO_eeprom.h"
#include "storage/CO_storageEeprom.h"

#include <string.h>

/* Private define ------------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/**
 * @brief Attach one EEPROM module pointer to all entries and clear EEPROM metadata.
 *
 * @param entries Storage entries to bind.
 * @param entriesCount Number of entries in @p entries.
 * @param storageModule Persistent EEPROM module pointer.
 */
static void co_storage_rtt_eeprom_entries_bind(CO_storage_entry_t *entries, uint8_t entriesCount, void *storageModule)
{
    for (uint8_t i = 0U; i < entriesCount; i++) {
        entries[i].storageModule = storageModule;
        entries[i].eepromAddrSignature = 0U;
        entries[i].eepromAddr = 0U;
        entries[i].offset = 0U;
        entries[i].crc = 0U;
        entries[i].eepromPayloadChanged = false;
    }
}

/**
 * @brief Read one EEPROM entry payload into RAM.
 *
 * @param entry Storage entry whose payload should be read.
 * @param CANmodule CAN module passed through co_storage_rtt_init(). It is unused by this backend.
 * @return ODR_OK on success, otherwise ODR_HW.
 */
static ODR_t co_storage_rtt_eeprom_read(CO_storage_entry_t *entry, CO_CANmodule_t *CANmodule)
{
    (void)CANmodule;

    if ((entry == NULL) || (entry->addr == NULL) || (entry->len == 0U) || (entry->storageModule == NULL)) {
        return ODR_HW;
    }

    CO_eeprom_readBlock(entry->storageModule, entry->addr, entry->eepromAddr, entry->len);
    return ODR_OK;
}

/**
 * @brief Update and verify one EEPROM entry signature from the current payload.
 *
 * @param entry Storage entry whose EEPROM signature should be refreshed.
 * @return ODR_OK on success, otherwise ODR_HW.
 */
static ODR_t co_storage_rtt_eeprom_update_signature(CO_storage_entry_t *entry)
{
    bool_t writeOk;
    uint16_t crcRead;
    uint16_t signatureOfEntry;
    uint32_t signature;
    uint32_t signatureRead;

    if ((entry == NULL) || (entry->addr == NULL) || (entry->len == 0U) || (entry->storageModule == NULL)) {
        return ODR_HW;
    }

    entry->crc = crc16_ccitt(entry->addr, entry->len, 0);
    crcRead = CO_eeprom_getCrcBlock(entry->storageModule, entry->eepromAddr, entry->len);
    if (entry->crc != crcRead) {
        return ODR_HW;
    }

    signatureOfEntry = (uint16_t)entry->len;
    signature = (((uint32_t)entry->crc) << 16) | signatureOfEntry;
    writeOk = CO_eeprom_writeBlock(entry->storageModule, (uint8_t *)&signature, entry->eepromAddrSignature,
                                   sizeof(signature));

    CO_eeprom_readBlock(entry->storageModule, (uint8_t *)&signatureRead, entry->eepromAddrSignature,
                        sizeof(signatureRead));
    if ((signature != signatureRead) || !writeOk) {
        return ODR_HW;
    }

    return ODR_OK;
}

/**
 * @brief Store one EEPROM entry payload and signature.
 *
 * @param entry Storage entry requested by CANopenNode object 0x1010.
 * @param CANmodule CAN module passed through CO_storage_init(). It is unused by this backend.
 * @return ODR_OK on success, otherwise ODR_HW.
 */
static ODR_t co_storage_rtt_eeprom_store(CO_storage_entry_t *entry, CO_CANmodule_t *CANmodule)
{
    bool_t writeOk;

    (void)CANmodule;

    if ((entry == NULL) || (entry->addr == NULL) || (entry->len == 0U) || (entry->storageModule == NULL)) {
        return ODR_HW;
    }

    writeOk = CO_eeprom_writeBlock(entry->storageModule, entry->addr, entry->eepromAddr, entry->len);
    if (!writeOk) {
        return ODR_HW;
    }

    return co_storage_rtt_eeprom_update_signature(entry);
}

/**
 * @brief Restore one EEPROM entry by erasing its signature.
 *
 * @param entry Storage entry requested by CANopenNode object 0x1011.
 * @param CANmodule CAN module passed through CO_storage_init(). It is unused by this backend.
 * @return ODR_OK on success, otherwise ODR_HW.
 */
static ODR_t co_storage_rtt_eeprom_restore(CO_storage_entry_t *entry, CO_CANmodule_t *CANmodule)
{
    bool_t writeOk;
    uint32_t signature = 0xFFFFFFFFU;
    uint32_t signatureRead;

    (void)CANmodule;

    if ((entry == NULL) || (entry->storageModule == NULL)) {
        return ODR_HW;
    }

    writeOk = CO_eeprom_writeBlock(entry->storageModule, (uint8_t *)&signature, entry->eepromAddrSignature,
                                   sizeof(signature));
    CO_eeprom_readBlock(entry->storageModule, (uint8_t *)&signatureRead, entry->eepromAddrSignature,
                        sizeof(signatureRead));
    if ((signature != signatureRead) || !writeOk) {
        return ODR_HW;
    }

    return ODR_OK;
}

/**
 * @brief Update one EEPROM payload byte and report whether it actually changed.
 *
 * @param entry Storage entry that owns the EEPROM payload.
 * @param offset Payload byte offset inside @p entry.
 * @param payloadChanged Output flag set to true if the stored byte differed and the update succeeded.
 * @return true on successful update or when the byte was already current, otherwise false.
 */
static bool_t co_storage_rtt_eeprom_update_payload_byte(CO_storage_entry_t *entry,
                                                        size_t offset,
                                                        bool_t *payloadChanged)
{
    uint8_t dataByteToUpdate;
    uint8_t storedByte;
    size_t eepromAddr;

    dataByteToUpdate = ((uint8_t *)entry->addr)[offset];
    eepromAddr = entry->eepromAddr + offset;
    CO_eeprom_readBlock(entry->storageModule, &storedByte, eepromAddr, sizeof(storedByte));
    if (!CO_eeprom_updateByte(entry->storageModule, dataByteToUpdate, eepromAddr)) {
        return false;
    }

    if ((payloadChanged != NULL) && (storedByte != dataByteToUpdate)) {
        *payloadChanged = true;
    }

    return true;
}

/**
 * @brief Initialize generic EEPROM storage and delegate layout handling to CO_storageEeprom.c.
 *
 * @param storage Storage object initialized by the common RT-Thread storage frontend.
 * @param CANmodule CAN module passed through co_storage_rtt_init().
 * @param OD_1010_StoreParameters OD entry for 0x1010 Store parameters.
 * @param OD_1011_RestoreDefaultParameters OD entry for 0x1011 Restore default parameters.
 * @param entries Storage entries to bind to the EEPROM module.
 * @param entriesCount Number of storage entries.
 * @param instanceName Optional CANopenNodeRTT instance name.
 * @param storageInitError Optional error detail returned by CO_storageEeprom_init().
 * @return CO_ERROR_NO on success, otherwise a CANopenNode error code.
 */
static CO_ReturnError_t co_storage_rtt_eeprom_init(CO_storage_t *storage,
                                                   CO_CANmodule_t *CANmodule,
                                                   OD_entry_t *OD_1010_StoreParameters,
                                                   OD_entry_t *OD_1011_RestoreDefaultParameters,
                                                   CO_storage_entry_t *entries,
                                                   uint8_t entriesCount,
                                                   const char *instanceName,
                                                   uint32_t *storageInitError)
{
    void *storageModule;

    if ((entries == NULL) || (entriesCount == 0U) || (entriesCount > CO_CONFIG_STORAGE_MAX_ENTRIES_COUNT)) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    storageModule = co_storage_rtt_eeprom_module_get(storage, instanceName);
    if (storageModule == NULL) {
        return CO_ERROR_OUT_OF_MEMORY;
    }

    co_storage_rtt_eeprom_entries_bind(entries, entriesCount, storageModule);
    return CO_storageEeprom_init(storage, CANmodule, storageModule, OD_1010_StoreParameters,
                                 OD_1011_RestoreDefaultParameters, entries, entriesCount, storageInitError);
}

/**
 * @brief Process automatic EEPROM storage entries.
 *
 * This local RT-Thread wrapper keeps CANopenNode upstream files unchanged,
 * refreshes the signature after one complete entry payload save, and stops the
 * current entry on a failed EEPROM byte update so saveAll cannot loop forever
 * when the device reports a persistent write error.
 *
 * @param storage Storage object initialized by co_storage_rtt_init().
 * @param saveAll True to update all bytes, false to update one changed byte per call.
 * @return true if automatic EEPROM processing completed without backend error, otherwise false.
 */
static bool_t co_storage_rtt_eeprom_auto_process(CO_storage_t *storage, bool_t saveAll)
{
    bool_t ok = true;

    for (uint8_t n = 0U; n < storage->entriesCount; n++) {
        CO_storage_entry_t *entry = &storage->entries[n];

        if ((entry->attr & (uint8_t)CO_storage_auto) == 0U) {
            continue;
        }

        if (saveAll) {
            bool_t entryOk = true;
            bool_t payloadChanged = false;

            for (size_t i = 0U; i < entry->len; i++) {
                if (!co_storage_rtt_eeprom_update_payload_byte(entry, i, &payloadChanged)) {
                    CO_RTT_LOG_E("EEPROM auto saveAll failed: sub=%u offset=%lu addr=%lu",
                                 entry->subIndexOD, (unsigned long)i, (unsigned long)(entry->eepromAddr + i));
                    entryOk = false;
                    ok = false;
                    break;
                }
            }

            if (payloadChanged) {
                entry->eepromPayloadChanged = true;
            }
            if (entryOk && entry->eepromPayloadChanged) {
                if (co_storage_rtt_eeprom_update_signature(entry) == ODR_OK) {
                    entry->eepromPayloadChanged = false;
                } else {
                    CO_RTT_LOG_E("EEPROM auto saveAll signature failed: sub=%u", entry->subIndexOD);
                    ok = false;
                }
            }
        } else {
            bool_t payloadChanged = false;
            size_t offset = entry->offset;

            if (co_storage_rtt_eeprom_update_payload_byte(entry, offset, &payloadChanged)) {
                if (payloadChanged) {
                    entry->eepromPayloadChanged = true;
                }
                if (++entry->offset >= entry->len) {
                    entry->offset = 0U;
                    if (entry->eepromPayloadChanged) {
                        if (co_storage_rtt_eeprom_update_signature(entry) == ODR_OK) {
                            entry->eepromPayloadChanged = false;
                        } else {
                            CO_RTT_LOG_E("EEPROM auto save signature failed: sub=%u", entry->subIndexOD);
                            ok = false;
                        }
                    }
                }
            } else {
                CO_RTT_LOG_E("EEPROM auto save failed: sub=%u offset=%lu addr=%lu",
                             entry->subIndexOD, (unsigned long)offset, (unsigned long)(entry->eepromAddr + offset));
                ok = false;
            }
        }
    }

    return ok;
}

/** Selected generic EEPROM backend operations. */
static const CO_storage_rtt_backend_ops_t co_storage_rtt_eeprom_ops = {
    .init = co_storage_rtt_eeprom_init,
    .read = co_storage_rtt_eeprom_read,
    .store = co_storage_rtt_eeprom_store,
    .restore = co_storage_rtt_eeprom_restore,
    .auto_process = co_storage_rtt_eeprom_auto_process,
};

/**
 * @brief Get weak built-in EEPROM backend operations.
 *
 * @return Address of the built-in EEPROM operation table.
 */
rt_weak const CO_storage_rtt_backend_ops_t *co_storage_rtt_backend_get_ops(void)
{
    return &co_storage_rtt_eeprom_ops;
}

#endif /* (((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0) && defined(PKG_CANOPENNODE_USING_STORAGE_EEPROM) */
