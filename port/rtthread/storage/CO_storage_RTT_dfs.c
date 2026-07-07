/**
 * @file CO_storage_RTT_dfs.c
 * @brief RT-Thread DFS backend for CANopenNode storage entries.
 * @details This file implements the DFS file backend used to persist CANopenNode storage entries through RT-Thread DFS APIs.
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
#include "CO_storage_RTT_dfs.h"
#include "co_rtt_log.h"
#include "301/crc16-ccitt.h"

#if (((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0) && defined(PKG_CANOPENNODE_USING_STORAGE_DFS)

#include <dfs.h>
#include <dfs_file.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <rtthread.h>

/* Private define ------------------------------------------------------------*/

#ifndef CO_CONFIG_STORAGE_MAX_ENTRIES_COUNT
#define CO_CONFIG_STORAGE_MAX_ENTRIES_COUNT 1U
#endif /* CO_CONFIG_STORAGE_MAX_ENTRIES_COUNT */

/*
 * dfs_file_open() uses the same numeric open flag contract as RT-Thread DFS
 * POSIX wrappers, but this backend intentionally avoids POSIX-only headers and
 * keeps the required file-open values local to this backend.
 */
#define CO_STORAGE_RTT_DFS_FLAG_RDONLY 0000000U
#define CO_STORAGE_RTT_DFS_FLAG_WRONLY 0000001U
#define CO_STORAGE_RTT_DFS_FLAG_CREAT  0000100U
#define CO_STORAGE_RTT_DFS_FLAG_TRUNC  0001000U
#define CO_STORAGE_RTT_DFS_MAGIC       0x43524446UL
#define CO_STORAGE_RTT_DFS_VERSION     1U

/* Private function prototypes -----------------------------------------------*/

/**
 * @brief DFS persisted file header.
 */
typedef struct {
    uint32_t magic;       /**< File magic used to reject unrelated data. */
    uint16_t version;     /**< DFS storage file format version. */
    uint16_t subIndexOD;  /**< Storage sub-index used as an entry identity check. */
    uint32_t len;         /**< Payload length in bytes. */
    uint16_t crc;         /**< CRC16-CCITT over the payload. */
    uint16_t reserved;    /**< Reserved for future format flags. */
} CO_storage_rtt_dfs_header_t;

/**
 * @brief DFS backend state for one CANopenNode instance.
 */
typedef struct {
    bool_t used;                         /**< True when this static slot is assigned. */
    CO_storage_t *storage;               /**< Storage object owning this slot. */
    CO_storage_entry_t *entries;         /**< Entry array owning the private entry metadata. */
    uint8_t entriesCount;                /**< Number of entries bound to this slot. */
    CO_storage_rtt_dfs_entry_t entryPrivates[CO_CONFIG_STORAGE_MAX_ENTRIES_COUNT]; /**< Per-entry metadata. */
} CO_storage_rtt_dfs_instance_t;

/** Static DFS backend pool. One slot is reserved for each configured CANopenNode instance. */
static CO_storage_rtt_dfs_instance_t co_storage_rtt_dfs_instances[CO_RTT_CAN_BINDING_COUNT];

/**
 * @brief Allocate or reuse a DFS backend instance slot.
 *
 * @param storage Storage object that owns the backend slot.
 * @return Backend instance slot, or NULL if all slots are in use.
 */
static CO_storage_rtt_dfs_instance_t *co_storage_rtt_dfs_instance_get(CO_storage_t *storage)
{
    for (uint8_t i = 0U; i < CO_RTT_CAN_BINDING_COUNT; i++) {
        if ((co_storage_rtt_dfs_instances[i].used) && (co_storage_rtt_dfs_instances[i].storage == storage)) {
            return &co_storage_rtt_dfs_instances[i];
        }
    }

    for (uint8_t i = 0U; i < CO_RTT_CAN_BINDING_COUNT; i++) {
        if (!co_storage_rtt_dfs_instances[i].used) {
            memset(&co_storage_rtt_dfs_instances[i], 0, sizeof(co_storage_rtt_dfs_instances[i]));
            co_storage_rtt_dfs_instances[i].used = true;
            co_storage_rtt_dfs_instances[i].storage = storage;
            return &co_storage_rtt_dfs_instances[i];
        }
    }

    return NULL;
}

/**
 * @brief Bind DFS private metadata to storage entries.
 *
 * @param instance Backend instance slot.
 * @param entries Storage entries to bind.
 * @param entriesCount Number of entries in @p entries.
 * @param instanceName Optional application instance name used as a file prefix.
 */
static void co_storage_rtt_dfs_entries_bind(CO_storage_rtt_dfs_instance_t *instance,
                                               CO_storage_entry_t *entries,
                                               uint8_t entriesCount,
                                               const char *instanceName)
{
    instance->entries = entries;
    instance->entriesCount = entriesCount;

    for (uint8_t i = 0U; i < entriesCount; i++) {
        CO_storage_rtt_dfs_entry_t *entryPrivate = &instance->entryPrivates[i];

        entryPrivate->directory = NULL;
        entryPrivate->filePrefix = instanceName;
        entryPrivate->autoCrc = 0U;
        entryPrivate->autoCrcValid = 0U;
        entryPrivate->dataCorrupt = 0U;
        entries[i].storageModule = entryPrivate;
    }
}

/**
 * @brief Check whether a DFS open return code means a missing file.
 *
 * @param ret Negative return code from dfs_file_open().
 * @return true if ENOENT is available and @p ret maps to ENOENT, otherwise false.
 */
static rt_bool_t co_storage_rtt_dfs_is_noent(int ret)
{
#if defined(ENOENT)
    return (ret == -ENOENT) ? RT_TRUE : RT_FALSE;
#else
    (void)ret;

    return RT_FALSE;
#endif /* defined(ENOENT) */
}

/**
 * @brief Get optional DFS metadata for an entry.
 *
 * @param entry Storage entry.
 * @return Per-entry DFS metadata, or NULL to use Kconfig defaults.
 */
static const CO_storage_rtt_dfs_entry_t *co_storage_rtt_dfs_config(const CO_storage_entry_t *entry)
{
    return (entry != NULL) ? (const CO_storage_rtt_dfs_entry_t *)entry->storageModule : NULL;
}

/**
 * @brief Get the DFS directory for an entry.
 *
 * @param entry Storage entry.
 * @return Directory path.
 */
static const char *co_storage_rtt_dfs_directory(const CO_storage_entry_t *entry)
{
    const CO_storage_rtt_dfs_entry_t *config = co_storage_rtt_dfs_config(entry);

    if ((config != NULL) && (config->directory != NULL) && (config->directory[0] != '\0')) {
        return config->directory;
    }

    return PKG_CANOPENNODE_STORAGE_DFS_DIR;
}

/**
 * @brief Build a storage file path for an entry.
 *
 * @param entry Storage entry.
 * @param path Output path buffer.
 * @param pathSize Output path buffer size.
 * @return RT_EOK on success, otherwise -RT_ERROR.
 */
static rt_err_t co_storage_rtt_dfs_make_path(const CO_storage_entry_t *entry, char *path, size_t pathSize)
{
    const CO_storage_rtt_dfs_entry_t *config;
    const char *dir;
    int len;

    if ((entry == NULL) || (path == NULL) || (pathSize == 0U)) {
        return -RT_ERROR;
    }

    dir = co_storage_rtt_dfs_directory(entry);
    if ((dir == NULL) || (dir[0] == '\0')) {
        return -RT_ERROR;
    }

    config = co_storage_rtt_dfs_config(entry);
    if ((config != NULL) && (config->filePrefix != NULL) && (config->filePrefix[0] != '\0')) {
        len = rt_snprintf(path, pathSize, "%s/%s_storage_%02x.bin", dir, config->filePrefix, entry->subIndexOD);
    } else {
        len = rt_snprintf(path, pathSize, "%s/co_storage_%02x.bin", dir, entry->subIndexOD);
    }
    if ((len < 0) || ((size_t)len >= pathSize)) {
        return -RT_ERROR;
    }

    return RT_EOK;
}

/**
 * @brief Build a DFS persisted file header for one entry.
 *
 * @param entry Storage entry whose payload is persisted.
 * @param header Output header.
 * @return RT_EOK on success, otherwise -RT_ERROR.
 */
static rt_err_t co_storage_rtt_dfs_header_init(const CO_storage_entry_t *entry, CO_storage_rtt_dfs_header_t *header)
{
    if ((entry == NULL) || (entry->addr == NULL) || (entry->len == 0U) || (header == NULL)
        || (entry->len > UINT32_MAX)) {
        return -RT_ERROR;
    }

    header->magic = CO_STORAGE_RTT_DFS_MAGIC;
    header->version = CO_STORAGE_RTT_DFS_VERSION;
    header->subIndexOD = entry->subIndexOD;
    header->len = (uint32_t)entry->len;
    header->crc = crc16_ccitt(entry->addr, entry->len, 0);
    header->reserved = 0U;

    return RT_EOK;
}

/**
 * @brief Mark an entry as having no usable persisted DFS file.
 *
 * @param entry Storage entry whose DFS metadata should be updated.
 */
static void co_storage_rtt_dfs_mark_missing(CO_storage_entry_t *entry)
{
    CO_storage_rtt_dfs_entry_t *config = (CO_storage_rtt_dfs_entry_t *)entry->storageModule;

    if (config != NULL) {
        config->autoCrc = 0U;
        config->autoCrcValid = 0U;
        config->dataCorrupt = 0U;
    }
}

/**
 * @brief Mark an entry as successfully loaded from DFS.
 *
 * @param entry Storage entry whose DFS metadata should be updated.
 * @param crc CRC16 of the loaded payload.
 */
static void co_storage_rtt_dfs_mark_loaded(CO_storage_entry_t *entry, uint16_t crc)
{
    CO_storage_rtt_dfs_entry_t *config = (CO_storage_rtt_dfs_entry_t *)entry->storageModule;

    if (config != NULL) {
        config->autoCrc = crc;
        config->autoCrcValid = 1U;
        config->dataCorrupt = 0U;
    }
}

/**
 * @brief Mark an entry as recoverably corrupt while keeping the current RAM value.
 *
 * @param entry Storage entry whose DFS metadata should be updated.
 */
static void co_storage_rtt_dfs_mark_corrupt(CO_storage_entry_t *entry)
{
    CO_storage_rtt_dfs_entry_t *config = (CO_storage_rtt_dfs_entry_t *)entry->storageModule;

    if (config != NULL) {
        config->autoCrc = crc16_ccitt(entry->addr, entry->len, 0);
        config->autoCrcValid = 1U;
        config->dataCorrupt = 1U;
    }
}

/**
 * @brief Check whether the latest startup read rejected one entry as corrupt.
 *
 * @param entry Storage entry to inspect.
 * @return RT_TRUE if @p entry is marked recoverably corrupt, otherwise RT_FALSE.
 */
static rt_bool_t co_storage_rtt_dfs_is_corrupt(const CO_storage_entry_t *entry)
{
    const CO_storage_rtt_dfs_entry_t *config = co_storage_rtt_dfs_config(entry);

    return ((config != NULL) && (config->dataCorrupt != 0U)) ? RT_TRUE : RT_FALSE;
}

/**
 * @brief Validate a DFS persisted file header for one entry.
 *
 * @param entry Storage entry expected by the current Object Dictionary.
 * @param header Header read from the persisted file.
 * @return true if the header matches @p entry, otherwise false.
 */
static bool_t co_storage_rtt_dfs_header_matches(const CO_storage_entry_t *entry,
                                                const CO_storage_rtt_dfs_header_t *header)
{
    return ((entry != NULL) && (header != NULL) && (header->magic == CO_STORAGE_RTT_DFS_MAGIC)
            && (header->version == CO_STORAGE_RTT_DFS_VERSION) && (header->subIndexOD == entry->subIndexOD)
            && (header->len == entry->len));
}

/**
 * @brief Store one entry through the RT-Thread DFS backend.
 *
 * @param entry Storage entry whose payload should be written.
 * @param CANmodule CAN module passed through CO_storage_init(). It is unused by this backend.
 * @return ODR_OK on success, otherwise ODR_HW.
 */
static ODR_t co_storage_rtt_dfs_store(CO_storage_entry_t *entry, CO_CANmodule_t *CANmodule)
{
    char path[PKG_CANOPENNODE_STORAGE_DFS_MAX_PATH];
    struct dfs_file fd;
    CO_storage_rtt_dfs_header_t header;
    int ret;
    int written;

    (void)CANmodule;

    if ((entry->addr == NULL) || (entry->len == 0U)) {
        return ODR_HW;
    }
    if (co_storage_rtt_dfs_make_path(entry, path, sizeof(path)) != RT_EOK) {
        CO_RTT_LOG_W("DFS storage path build failed: sub=0x%02x", entry->subIndexOD);
        return ODR_HW;
    }
    if (co_storage_rtt_dfs_header_init(entry, &header) != RT_EOK) {
        CO_RTT_LOG_W("DFS storage header build failed: sub=0x%02x", entry->subIndexOD);
        return ODR_HW;
    }

    rt_memset(&fd, 0, sizeof(fd));

    /*
     * This default DFS backend writes directly to the final file with truncation.
     * It is simple, but it is not power-loss safe: reset or power failure during
     * write/flush/close may leave an empty, partial or CRC-mismatched file. The
     * startup read path rejects invalid headers, length mismatch and CRC mismatch
     * without copying invalid payload into RAM. The storage object stays enabled
     * so explicit restore/save can recover from a rejected file. Products
     * that require fail-safe parameter storage should replace this backend with
     * an atomic temporary-file/rename scheme or
     * a flash backend with commit/rollback semantics.
     */
    ret = dfs_file_open(&fd, path, CO_STORAGE_RTT_DFS_FLAG_WRONLY
                                  | CO_STORAGE_RTT_DFS_FLAG_CREAT
                                  | CO_STORAGE_RTT_DFS_FLAG_TRUNC);
    if (ret < 0) {
        CO_RTT_LOG_W("DFS storage open for write failed: path=%s ret=%d", path, ret);
        return ODR_HW;
    }

    written = dfs_file_write(&fd, &header, sizeof(header));
    if ((written < 0) || ((size_t)written != sizeof(header))) {
        CO_RTT_LOG_W("DFS storage header write failed: path=%s written=%d expected=%lu", path, written,
                     (unsigned long)sizeof(header));
        (void)dfs_file_close(&fd);
        return ODR_HW;
    }

    written = dfs_file_write(&fd, entry->addr, entry->len);
    if ((written < 0) || ((size_t)written != entry->len)) {
        CO_RTT_LOG_W("DFS storage payload write failed: path=%s written=%d expected=%lu", path, written,
                     (unsigned long)entry->len);
        (void)dfs_file_close(&fd);
        return ODR_HW;
    }
    if (dfs_file_flush(&fd) < 0) {
        CO_RTT_LOG_W("DFS storage flush failed: path=%s", path);
        (void)dfs_file_close(&fd);
        return ODR_HW;
    }

    ret = dfs_file_close(&fd);
    if (ret < 0) {
        CO_RTT_LOG_W("DFS storage close after write failed: path=%s ret=%d", path, ret);
        return ODR_HW;
    }

    co_storage_rtt_dfs_mark_loaded(entry, header.crc);

    return ODR_OK;
}

/**
 * @brief Restore defaults for one entry through the RT-Thread DFS backend.
 *
 * The default backend restores by removing the persisted file only. It does not
 * rewrite the current RAM object dictionary image. Generated OD defaults take
 * effect after the stack/application is recreated and no persisted file is read.
 *
 * @param entry Storage entry requested by CANopenNode object 0x1011.
 * @param CANmodule CAN module passed through CO_storage_init(). It is unused by this backend.
 * @return ODR_OK on success, otherwise ODR_HW.
 */
static ODR_t co_storage_rtt_dfs_restore(CO_storage_entry_t *entry, CO_CANmodule_t *CANmodule)
{
    char path[PKG_CANOPENNODE_STORAGE_DFS_MAX_PATH];
    struct dfs_file fd;
    int ret;

    (void)CANmodule;

    if (co_storage_rtt_dfs_make_path(entry, path, sizeof(path)) != RT_EOK) {
        CO_RTT_LOG_W("DFS restore path build failed: sub=0x%02x", entry->subIndexOD);
        return ODR_HW;
    }

    rt_memset(&fd, 0, sizeof(fd));
    ret = dfs_file_open(&fd, path, CO_STORAGE_RTT_DFS_FLAG_RDONLY);
    if (ret < 0) {
        if (co_storage_rtt_dfs_is_noent(ret) == RT_TRUE) {
            co_storage_rtt_dfs_mark_missing(entry);
            return ODR_OK;
        }
#if !defined(ENOENT)
        co_storage_rtt_dfs_mark_missing(entry);
        CO_RTT_LOG_W("DFS restore open failed without ENOENT mapping; treating as restored: path=%s ret=%d", path,
                     ret);
        return ODR_OK;
#else
        CO_RTT_LOG_W("DFS restore open failed: path=%s ret=%d", path, ret);
        return ODR_HW;
#endif /* !defined(ENOENT) */
    }
    ret = dfs_file_close(&fd);
    if (ret < 0) {
        CO_RTT_LOG_W("DFS restore close before unlink failed: path=%s ret=%d", path, ret);
        return ODR_HW;
    }

    ret = dfs_file_unlink(path);
    if (ret < 0) {
        CO_RTT_LOG_W("DFS restore unlink failed: path=%s ret=%d", path, ret);
        return ODR_HW;
    }

    co_storage_rtt_dfs_mark_missing(entry);

    return ODR_OK;
}

/**
 * @brief Read one persisted entry through the RT-Thread DFS backend.
 *
 * When ENOENT is available, only missing files are treated as first startup.
 * Without ENOENT this backend cannot distinguish a missing file from another
 * open error, so it logs the path and keeps the previous first-startup behavior.
 * Existing files must contain one valid DFS storage header, exactly CO_storage_entry_t::len
 * payload bytes and no trailing data. The header includes magic, version, sub-index,
 * payload length and CRC16. Invalid file content is marked as recoverably corrupt;
 * the current RAM value is kept so OD defaults remain available for restore/save.
 *
 * @param entry Storage entry whose payload should be read.
 * @param CANmodule CAN module passed through co_storage_rtt_init(). It is unused by this backend.
 * @return ODR_OK on success or when no usable file is available, otherwise ODR_HW.
 */
static ODR_t co_storage_rtt_dfs_read(CO_storage_entry_t *entry, CO_CANmodule_t *CANmodule)
{
    char path[PKG_CANOPENNODE_STORAGE_DFS_MAX_PATH];
    struct dfs_file fd;
    CO_storage_rtt_dfs_header_t header;
    uint8_t extraByte;
    uint8_t *payload = NULL;
    uint16_t crc;
    int ret;
    int readLen;
    ODR_t odr = ODR_HW;

    (void)CANmodule;

    if ((entry->addr == NULL) || (entry->len == 0U)) {
        return ODR_HW;
    }
    if (co_storage_rtt_dfs_make_path(entry, path, sizeof(path)) != RT_EOK) {
        CO_RTT_LOG_W("DFS read path build failed: sub=0x%02x", entry->subIndexOD);
        return ODR_HW;
    }

    rt_memset(&fd, 0, sizeof(fd));
    ret = dfs_file_open(&fd, path, CO_STORAGE_RTT_DFS_FLAG_RDONLY);
    if (ret < 0) {
        if (co_storage_rtt_dfs_is_noent(ret) == RT_TRUE) {
            co_storage_rtt_dfs_mark_missing(entry);
            CO_RTT_LOG_D("DFS storage file missing, using OD defaults: path=%s", path);
            return ODR_OK;
        }
#if !defined(ENOENT)
        co_storage_rtt_dfs_mark_missing(entry);
        CO_RTT_LOG_W("DFS read open failed without ENOENT mapping; treating as first startup: path=%s ret=%d", path,
                     ret);
        return ODR_OK;
#else
        CO_RTT_LOG_W("DFS read open failed: path=%s ret=%d", path, ret);
        return ODR_HW;
#endif /* !defined(ENOENT) */
    }

    readLen = dfs_file_read(&fd, &header, sizeof(header));
    if ((readLen < 0) || ((size_t)readLen != sizeof(header))) {
        CO_RTT_LOG_W("DFS read header length mismatch: path=%s read=%d expected=%lu", path, readLen,
                     (unsigned long)sizeof(header));
        co_storage_rtt_dfs_mark_corrupt(entry);
        goto close_file;
    }
    if (!co_storage_rtt_dfs_header_matches(entry, &header)) {
        CO_RTT_LOG_W("DFS read header mismatch: path=%s sub=0x%02x len=%lu", path, entry->subIndexOD,
                     (unsigned long)entry->len);
        co_storage_rtt_dfs_mark_corrupt(entry);
        goto close_file;
    }

    payload = (uint8_t *)rt_malloc(entry->len);
    if (payload == RT_NULL) {
        CO_RTT_LOG_W("DFS read payload allocation failed: path=%s len=%lu", path, (unsigned long)entry->len);
        goto close_file;
    }

    readLen = dfs_file_read(&fd, payload, entry->len);
    if ((readLen < 0) || ((size_t)readLen != entry->len)) {
        CO_RTT_LOG_W("DFS read payload length mismatch: path=%s read=%d expected=%lu", path, readLen,
                     (unsigned long)entry->len);
        co_storage_rtt_dfs_mark_corrupt(entry);
        goto close_file;
    }

    crc = crc16_ccitt(payload, entry->len, 0);
    if (crc != header.crc) {
        CO_RTT_LOG_W("DFS read CRC mismatch: path=%s stored=0x%04x calculated=0x%04x", path, header.crc, crc);
        co_storage_rtt_dfs_mark_corrupt(entry);
        goto close_file;
    }

    ret = dfs_file_read(&fd, &extraByte, sizeof(extraByte));
    if (ret != 0) {
        CO_RTT_LOG_W("DFS read found trailing data: path=%s ret=%d", path, ret);
        co_storage_rtt_dfs_mark_corrupt(entry);
        goto close_file;
    }

    memcpy(entry->addr, payload, entry->len);
    co_storage_rtt_dfs_mark_loaded(entry, crc);
    odr = ODR_OK;

close_file:
    if (payload != NULL) {
        rt_free(payload);
    }
    ret = dfs_file_close(&fd);
    if ((ret < 0) && (odr == ODR_OK)) {
        CO_RTT_LOG_W("DFS read close failed: path=%s ret=%d", path, ret);
        return ODR_HW;
    }

    return odr;
}

/**
 * @brief Process automatic DFS storage entries.
 *
 * Normal cyclic processing stores an automatic entry only when its CRC changed
 * since the last successful DFS store/read. saveAll forces every automatic entry
 * to be written once.
 *
 * @param storage Storage object initialized by co_storage_rtt_init().
 * @param saveAll True to force all automatic entries to DFS files, false to store changed entries only.
 * @return true if automatic DFS processing completed without backend error, otherwise false.
 */
static bool_t co_storage_rtt_dfs_auto_process(CO_storage_t *storage, bool_t saveAll)
{
    bool_t ok = true;

    if ((storage == NULL) || !storage->enabled) {
        return true;
    }

    for (uint8_t i = 0U; i < storage->entriesCount; i++) {
        CO_storage_entry_t *entry = &storage->entries[i];
        CO_storage_rtt_dfs_entry_t *config = (CO_storage_rtt_dfs_entry_t *)entry->storageModule;
        uint16_t crc;

        if ((entry->attr & (uint8_t)CO_storage_auto) == 0U) {
            continue;
        }
        if (config == NULL) {
            ok = false;
            continue;
        }

        crc = crc16_ccitt(entry->addr, entry->len, 0);
        if (saveAll || (config->autoCrcValid == 0U) || (config->autoCrc != crc)) {
            if (co_storage_rtt_dfs_store(entry, NULL) != ODR_OK) {
                ok = false;
            }
        }
    }

    return ok;
}

/**
 * @brief Initialize DFS storage and read all persisted entries.
 *
 * @param storage Storage object initialized by the common RT-Thread storage frontend.
 * @param CANmodule CAN module passed through co_storage_rtt_init().
 * @param OD_1010_StoreParameters OD entry for 0x1010 Store parameters.
 * @param OD_1011_RestoreDefaultParameters OD entry for 0x1011 Restore default parameters.
 * @param entries Storage entries to bind to DFS metadata.
 * @param entriesCount Number of storage entries.
 * @param instanceName Optional application instance name used as the DFS file prefix.
 * @param storageInitError Optional error detail. Stores a one-based failed entry index on read failure.
 * @return CO_ERROR_NO on success, CO_ERROR_DATA_CORRUPT if a persisted file was rejected while
 * storage remains enabled for restore/save, otherwise another CANopenNode error code.
 */
static CO_ReturnError_t co_storage_rtt_dfs_init(CO_storage_t *storage,
                                                CO_CANmodule_t *CANmodule,
                                                OD_entry_t *OD_1010_StoreParameters,
                                                OD_entry_t *OD_1011_RestoreDefaultParameters,
                                                CO_storage_entry_t *entries,
                                                uint8_t entriesCount,
                                                const char *instanceName,
                                                uint32_t *storageInitError)
{
    CO_storage_rtt_dfs_instance_t *instance;
    CO_ReturnError_t err;

    if ((entries == NULL) || (entriesCount == 0U) || (entriesCount > CO_CONFIG_STORAGE_MAX_ENTRIES_COUNT)) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    instance = co_storage_rtt_dfs_instance_get(storage);
    if (instance == NULL) {
        return CO_ERROR_OUT_OF_MEMORY;
    }

    co_storage_rtt_dfs_entries_bind(instance, entries, entriesCount, instanceName);

    err = CO_storage_init(storage, CANmodule, OD_1010_StoreParameters, OD_1011_RestoreDefaultParameters,
                          co_storage_rtt_store, co_storage_rtt_restore, entries, entriesCount);
    if (err != CO_ERROR_NO) {
        storage->enabled = false;
        return err;
    }

    err = CO_ERROR_NO;
    for (uint8_t i = 0U; i < entriesCount; i++) {
        ODR_t odr = co_storage_rtt_dfs_read(&entries[i], CANmodule);

        if (odr != ODR_OK) {
            if (co_storage_rtt_dfs_is_corrupt(&entries[i]) == RT_TRUE) {
                if ((storageInitError != NULL) && (*storageInitError == 0U)) {
                    *storageInitError = (uint32_t)i + 1U;
                }
                err = CO_ERROR_DATA_CORRUPT;
                continue;
            }

            storage->enabled = false;
            if (storageInitError != NULL) {
                *storageInitError = (uint32_t)i + 1U;
            }
            return CO_ERROR_DATA_CORRUPT;
        }
    }

    storage->enabled = true;
    return err;
}

/** Selected DFS backend operations. */
static const CO_storage_rtt_backend_ops_t co_storage_rtt_dfs_ops = {
    .init = co_storage_rtt_dfs_init,
    .read = co_storage_rtt_dfs_read,
    .store = co_storage_rtt_dfs_store,
    .restore = co_storage_rtt_dfs_restore,
    .auto_process = co_storage_rtt_dfs_auto_process,
};

/**
 * @brief Get weak built-in DFS backend operations.
 *
 * @return Address of the built-in DFS operation table.
 */
rt_weak const CO_storage_rtt_backend_ops_t *co_storage_rtt_backend_get_ops(void)
{
    return &co_storage_rtt_dfs_ops;
}

#endif /* (((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0) && defined(PKG_CANOPENNODE_USING_STORAGE_DFS) */
