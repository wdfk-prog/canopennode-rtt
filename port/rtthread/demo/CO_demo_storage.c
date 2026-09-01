/**
 * @file CO_demo_storage.c
 * @brief Test-only CANopenNode EEPROM storage diagnostic implementation.
 */

#include "CO_demo_storage.h"

#include "301/crc16-ccitt.h"
#include "co_rtt_log.h"
#include "storage/CO_storage_RTT_at24c.h"

#include <string.h>

/** OD 0x1010/0x1011 sub-index used by the EEPROM storage diagnostic. */
#define CO_DEMO_STORAGE_COMM_SUB_INDEX 2U

/** CRC bit flipped by the signature-corruption command without changing the stored length. */
#define CO_DEMO_STORAGE_SIGNATURE_CORRUPT_MASK 0x00010000UL

/** Maximum stack buffer used while verifying a full raw restore. */
#define CO_DEMO_STORAGE_VERIFY_CHUNK_SIZE 32U

/** Publish all diagnostic output fields that are owned by target mainline. */
static void CO_demo_storage_publish(const CO_demo_storage_t *demo)
{
    if (demo == NULL) {
        return;
    }

    OD_RAM.x2305_storage_diagnostic.active_seq = demo->activeSeq;
    OD_RAM.x2305_storage_diagnostic.complete_seq = demo->completeSeq;
    OD_RAM.x2305_storage_diagnostic.result = demo->result;
    OD_RAM.x2305_storage_diagnostic.startup_state = demo->startupState;
    OD_RAM.x2305_storage_diagnostic.startup_result = demo->startupResult;
    OD_RAM.x2305_storage_diagnostic.startup_error = demo->startupError;
    OD_RAM.x2305_storage_diagnostic.storage_offset = (uint32_t)demo->storageOffset;
    OD_RAM.x2305_storage_diagnostic.eeprom_size = (uint32_t)demo->eepromSize;
    OD_RAM.x2305_storage_diagnostic.page_size = (uint16_t)demo->pageSize;
    OD_RAM.x2305_storage_diagnostic.addr_input = demo->addrInput;
    OD_RAM.x2305_storage_diagnostic.signature_address = (uint32_t)demo->signatureAddress;
    OD_RAM.x2305_storage_diagnostic.data_address = (uint32_t)demo->dataAddress;
    OD_RAM.x2305_storage_diagnostic.data_length = (uint32_t)demo->dataLength;
    OD_RAM.x2305_storage_diagnostic.raw_start = (uint32_t)demo->rawStart;
    OD_RAM.x2305_storage_diagnostic.raw_length = (uint32_t)demo->rawLength;
    OD_RAM.x2305_storage_diagnostic.signature_value = demo->signatureValue;
    OD_RAM.x2305_storage_diagnostic.backup_valid = demo->backupValid;
    OD_RAM.x2305_storage_diagnostic.backup_crc = demo->backupCrc;
    OD_RAM.x2305_storage_diagnostic.startup_seq = demo->startupSeq;
    OD_RAM.x2305_storage_diagnostic.startup_probe = demo->startupProbe;
}

/** Find the first configured storage entry matching one OD 0x1010/0x1011 sub-index. */
static CO_storage_entry_t *CO_demo_storage_find_entry(CO_demo_storage_t *demo, uint8_t subIndex)
{
    uint8_t i;

    if ((demo == NULL) || (demo->storage == NULL) || (demo->entries == NULL)) {
        return NULL;
    }

    for (i = 0U; i < demo->entriesCount; i++) {
        if (demo->entries[i].subIndexOD == subIndex) {
            return &demo->entries[i];
        }
    }

    return NULL;
}

/** Read and decode the current selected-entry signature. */
static bool_t CO_demo_storage_read_signature(CO_demo_storage_t *demo)
{
    uint8_t raw[sizeof(uint32_t)];

    if ((demo == NULL) || (demo->entry == NULL)
        || !co_storage_rtt_at24c_raw_read(demo->entry->storageModule, demo->signatureAddress, raw, sizeof(raw))) {
        return false;
    }

    demo->signatureValue = CO_getUint32(raw);
    return true;
}

/** Refresh the first PERSIST_COMM raw layout without changing EEPROM contents. */
static CO_demo_storage_result_t CO_demo_storage_refresh_layout(CO_demo_storage_t *demo)
{
    CO_storage_entry_t *entry;
    size_t rawEnd;
    size_t regionEnd;
    size_t signatureTableSize;
    uint8_t selectedSubIndex;

    if ((demo == NULL) || (demo->storage == NULL) || (demo->entries == NULL)) {
        return CO_DEMO_STORAGE_RESULT_NOT_READY;
    }

    selectedSubIndex = OD_RAM.x2305_storage_diagnostic.entry_sub_index;
    entry = CO_demo_storage_find_entry(demo, selectedSubIndex);
    if (entry == NULL) {
        demo->ready = 0U;
        return CO_DEMO_STORAGE_RESULT_ENTRY_NOT_FOUND;
    }

    /* The raw storage baseline is intentionally defined only for sub-index 2 as
     * the first entry. Then its signature starts at the storage base and the
     * contiguous region through its payload includes the complete signature table. */
    if ((selectedSubIndex != CO_DEMO_STORAGE_COMM_SUB_INDEX) || (entry != &demo->entries[0])) {
        demo->ready = 0U;
        return CO_DEMO_STORAGE_RESULT_UNSUPPORTED_LAYOUT;
    }

    if (!co_storage_rtt_at24c_raw_get_info(entry->storageModule, &demo->storageOffset,
                                             &demo->storageRegionSize, &demo->eepromSize,
                                             &demo->pageSize, &demo->addrInput)) {
        demo->ready = 0U;
        return CO_DEMO_STORAGE_RESULT_NOT_READY;
    }

    signatureTableSize = sizeof(uint32_t) * CO_CONFIG_STORAGE_MAX_ENTRIES_COUNT;
    if ((demo->storageOffset >= demo->eepromSize) || (demo->storageRegionSize == 0U)
        || (demo->storageRegionSize > (demo->eepromSize - demo->storageOffset))) {
        demo->ready = 0U;
        return CO_DEMO_STORAGE_RESULT_UNSUPPORTED_LAYOUT;
    }

    regionEnd = demo->storageOffset + demo->storageRegionSize;
    if ((signatureTableSize > demo->storageRegionSize)
        || (entry->eepromAddrSignature != demo->storageOffset)
        || (entry->eepromAddr != (demo->storageOffset + signatureTableSize))
        || (entry->len == 0U) || (entry->eepromAddr >= regionEnd)
        || (entry->len > (regionEnd - entry->eepromAddr))) {
        demo->ready = 0U;
        return CO_DEMO_STORAGE_RESULT_UNSUPPORTED_LAYOUT;
    }

    rawEnd = entry->eepromAddr + entry->len;
    demo->entry = entry;
    demo->signatureAddress = entry->eepromAddrSignature;
    demo->dataAddress = entry->eepromAddr;
    demo->dataLength = entry->len;
    demo->rawStart = entry->eepromAddrSignature;
    demo->rawLength = rawEnd - demo->rawStart;

    if ((demo->rawLength == 0U) || (demo->rawLength > sizeof(demo->backupRaw))) {
        demo->ready = 0U;
        return CO_DEMO_STORAGE_RESULT_UNSUPPORTED_LAYOUT;
    }
    if (!CO_demo_storage_read_signature(demo)) {
        demo->ready = 0U;
        return CO_DEMO_STORAGE_RESULT_IO_ERROR;
    }

    demo->ready = 1U;
    return CO_DEMO_STORAGE_RESULT_SUCCESS;
}

/** Validate one raw diagnostic word range relative to the complete raw region. */
static bool_t CO_demo_storage_raw_request_valid(const CO_demo_storage_t *demo, size_t *address, size_t *length)
{
    const uint32_t rawOffset = OD_RAM.x2305_storage_diagnostic.raw_offset;
    const uint8_t rawSize = OD_RAM.x2305_storage_diagnostic.raw_size;

    if ((demo == NULL) || (address == NULL) || (length == NULL) || (demo->ready == 0U)
        || (rawSize == 0U) || (rawSize > CO_DEMO_STORAGE_RAW_WORD_SIZE)
        || ((size_t)rawOffset >= demo->rawLength) || ((size_t)rawSize > (demo->rawLength - (size_t)rawOffset))) {
        return false;
    }

    *address = demo->rawStart + (size_t)rawOffset;
    *length = (size_t)rawSize;
    return true;
}

/** Copy the complete raw entry region into target RAM and publish its CRC. */
static CO_demo_storage_result_t CO_demo_storage_backup(CO_demo_storage_t *demo)
{
    CO_demo_storage_result_t result = CO_demo_storage_refresh_layout(demo);

    if (result != CO_DEMO_STORAGE_RESULT_SUCCESS) {
        return result;
    }
    if (!co_storage_rtt_at24c_raw_read(demo->entry->storageModule, demo->rawStart,
                                         demo->backupRaw, demo->rawLength)) {
        demo->backupValid = 0U;
        return CO_DEMO_STORAGE_RESULT_IO_ERROR;
    }

    demo->backupRawStart = demo->rawStart;
    demo->backupRawLength = demo->rawLength;
    demo->backupEntrySubIndex = demo->entry->subIndexOD;
    demo->backupCrc = crc16_ccitt(demo->backupRaw, demo->backupRawLength, 0U);
    demo->backupValid = 1U;
    return CO_DEMO_STORAGE_RESULT_SUCCESS;
}

/** Verify EEPROM bytes against the current target-RAM backup without a large stack buffer. */
static bool_t CO_demo_storage_verify_backup(CO_demo_storage_t *demo)
{
    uint8_t verify[CO_DEMO_STORAGE_VERIFY_CHUNK_SIZE];
    size_t offset = 0U;

    while (offset < demo->backupRawLength) {
        const size_t remaining = demo->backupRawLength - offset;
        const size_t chunk = (remaining < sizeof(verify)) ? remaining : sizeof(verify);

        if (!co_storage_rtt_at24c_raw_read(demo->entry->storageModule, demo->backupRawStart + offset,
                                             verify, chunk)
            || (memcmp(verify, &demo->backupRaw[offset], chunk) != 0)) {
            return false;
        }
        offset += chunk;
    }

    return true;
}

/** Restore the complete target-RAM raw baseline and verify every byte. */
static CO_demo_storage_result_t CO_demo_storage_restore(CO_demo_storage_t *demo)
{
    CO_demo_storage_result_t result = CO_demo_storage_refresh_layout(demo);

    if (result != CO_DEMO_STORAGE_RESULT_SUCCESS) {
        return result;
    }
    if ((demo->backupValid == 0U) || (demo->backupEntrySubIndex != demo->entry->subIndexOD)
        || (demo->backupRawStart != demo->rawStart) || (demo->backupRawLength != demo->rawLength)) {
        return CO_DEMO_STORAGE_RESULT_BACKUP_INVALID;
    }

    if (!co_storage_rtt_at24c_raw_write(demo->entry->storageModule, demo->backupRawStart,
                                          demo->backupRaw, demo->backupRawLength)) {
        return CO_DEMO_STORAGE_RESULT_IO_ERROR;
    }
    if (!CO_demo_storage_verify_backup(demo)) {
        return CO_DEMO_STORAGE_RESULT_VERIFY_ERROR;
    }
    if (!CO_demo_storage_read_signature(demo)) {
        return CO_DEMO_STORAGE_RESULT_IO_ERROR;
    }

    return CO_DEMO_STORAGE_RESULT_SUCCESS;
}

/** Corrupt only the CRC half of the selected CO_storageEeprom signature. */
static CO_demo_storage_result_t CO_demo_storage_corrupt_signature(CO_demo_storage_t *demo)
{
    uint8_t raw[sizeof(uint32_t)];
    uint32_t corrupted;
    CO_demo_storage_result_t result = CO_demo_storage_refresh_layout(demo);

    if (result != CO_DEMO_STORAGE_RESULT_SUCCESS) {
        return result;
    }

    corrupted = demo->signatureValue ^ CO_DEMO_STORAGE_SIGNATURE_CORRUPT_MASK;
    CO_setUint32(raw, corrupted);
    if (!co_storage_rtt_at24c_raw_write(demo->entry->storageModule, demo->signatureAddress, raw, sizeof(raw))) {
        return CO_DEMO_STORAGE_RESULT_IO_ERROR;
    }
    if (!CO_demo_storage_read_signature(demo)) {
        return CO_DEMO_STORAGE_RESULT_IO_ERROR;
    }
    if (demo->signatureValue != corrupted) {
        return CO_DEMO_STORAGE_RESULT_VERIFY_ERROR;
    }

    return CO_DEMO_STORAGE_RESULT_SUCCESS;
}

/** Corrupt one selected byte that must lie inside the entry payload. */
static CO_demo_storage_result_t CO_demo_storage_corrupt_data(CO_demo_storage_t *demo)
{
    uint8_t byte;
    uint8_t verify;
    size_t address;
    size_t length;
    CO_demo_storage_result_t result = CO_demo_storage_refresh_layout(demo);

    if (result != CO_DEMO_STORAGE_RESULT_SUCCESS) {
        return result;
    }
    if (!CO_demo_storage_raw_request_valid(demo, &address, &length) || (length != 1U)
        || (address < demo->dataAddress) || (address >= (demo->dataAddress + demo->dataLength))) {
        return CO_DEMO_STORAGE_RESULT_RANGE_ERROR;
    }

    if (!co_storage_rtt_at24c_raw_read(demo->entry->storageModule, address, &byte, sizeof(byte))) {
        return CO_DEMO_STORAGE_RESULT_IO_ERROR;
    }
    byte ^= 0x01U;
    if (!co_storage_rtt_at24c_raw_write(demo->entry->storageModule, address, &byte, sizeof(byte))) {
        return CO_DEMO_STORAGE_RESULT_IO_ERROR;
    }
    if (!co_storage_rtt_at24c_raw_read(demo->entry->storageModule, address, &verify, sizeof(verify))) {
        return CO_DEMO_STORAGE_RESULT_IO_ERROR;
    }

    return (verify == byte) ? CO_DEMO_STORAGE_RESULT_SUCCESS : CO_DEMO_STORAGE_RESULT_VERIFY_ERROR;
}

/** Read one to four raw bytes and pack them into the diagnostic U32 field. */
static CO_demo_storage_result_t CO_demo_storage_raw_read(CO_demo_storage_t *demo)
{
    uint8_t raw[CO_DEMO_STORAGE_RAW_WORD_SIZE] = {0U};
    size_t address;
    size_t length;
    CO_demo_storage_result_t result = CO_demo_storage_refresh_layout(demo);

    if (result != CO_DEMO_STORAGE_RESULT_SUCCESS) {
        return result;
    }
    if (!CO_demo_storage_raw_request_valid(demo, &address, &length)) {
        return CO_DEMO_STORAGE_RESULT_RANGE_ERROR;
    }
    if (!co_storage_rtt_at24c_raw_read(demo->entry->storageModule, address, raw, length)) {
        return CO_DEMO_STORAGE_RESULT_IO_ERROR;
    }

    OD_RAM.x2305_storage_diagnostic.raw_value = CO_getUint32(raw);
    return CO_DEMO_STORAGE_RESULT_SUCCESS;
}

/** Unpack and write one to four raw bytes, then verify the exact write. */
static CO_demo_storage_result_t CO_demo_storage_raw_write(CO_demo_storage_t *demo)
{
    uint8_t raw[CO_DEMO_STORAGE_RAW_WORD_SIZE];
    uint8_t verify[CO_DEMO_STORAGE_RAW_WORD_SIZE];
    size_t address;
    size_t length;
    CO_demo_storage_result_t result = CO_demo_storage_refresh_layout(demo);

    if (result != CO_DEMO_STORAGE_RESULT_SUCCESS) {
        return result;
    }
    if (!CO_demo_storage_raw_request_valid(demo, &address, &length)) {
        return CO_DEMO_STORAGE_RESULT_RANGE_ERROR;
    }

    CO_setUint32(raw, OD_RAM.x2305_storage_diagnostic.raw_value);
    if (!co_storage_rtt_at24c_raw_write(demo->entry->storageModule, address, raw, length)) {
        return CO_DEMO_STORAGE_RESULT_IO_ERROR;
    }
    if (!co_storage_rtt_at24c_raw_read(demo->entry->storageModule, address, verify, length)) {
        return CO_DEMO_STORAGE_RESULT_IO_ERROR;
    }

    return (memcmp(raw, verify, length) == 0) ? CO_DEMO_STORAGE_RESULT_SUCCESS
                                              : CO_DEMO_STORAGE_RESULT_VERIFY_ERROR;
}

/** Execute one latched command and return a normalized diagnostic result. */
static CO_demo_storage_result_t CO_demo_storage_execute(CO_demo_storage_t *demo, uint8_t command)
{
    switch ((CO_demo_storage_command_t)command) {
    case CO_DEMO_STORAGE_COMMAND_REFRESH:
        return CO_demo_storage_refresh_layout(demo);
    case CO_DEMO_STORAGE_COMMAND_BACKUP:
        return CO_demo_storage_backup(demo);
    case CO_DEMO_STORAGE_COMMAND_RESTORE:
        return CO_demo_storage_restore(demo);
    case CO_DEMO_STORAGE_COMMAND_CORRUPT_SIGNATURE:
        return CO_demo_storage_corrupt_signature(demo);
    case CO_DEMO_STORAGE_COMMAND_CORRUPT_DATA:
        return CO_demo_storage_corrupt_data(demo);
    case CO_DEMO_STORAGE_COMMAND_RAW_READ:
        return CO_demo_storage_raw_read(demo);
    case CO_DEMO_STORAGE_COMMAND_RAW_WRITE:
        return CO_demo_storage_raw_write(demo);
    case CO_DEMO_STORAGE_COMMAND_CLEAR_BACKUP:
        demo->backupValid = 0U;
        demo->backupCrc = 0U;
        demo->backupEntrySubIndex = 0U;
        demo->backupRawStart = 0U;
        demo->backupRawLength = 0U;
        return CO_DEMO_STORAGE_RESULT_SUCCESS;
    case CO_DEMO_STORAGE_COMMAND_NONE:
    default:
        return CO_DEMO_STORAGE_RESULT_INVALID_ARGUMENT;
    }
}

void CO_demo_storage_init(CO_demo_storage_t *demo)
{
    if (demo == NULL) {
        return;
    }

    (void)memset(demo, 0, sizeof(*demo));
    demo->lastConsumedSeq = OD_RAM.x2305_storage_diagnostic.request_seq;
    demo->activeSeq = demo->lastConsumedSeq;
    demo->completeSeq = demo->lastConsumedSeq;
    demo->result = (int32_t)CO_DEMO_STORAGE_RESULT_NONE;
    demo->startupResult = (int32_t)CO_ERROR_NO;
    demo->startupState = (uint8_t)CO_DEMO_STORAGE_STARTUP_UNKNOWN;
    CO_demo_storage_publish(demo);
}

void CO_demo_storage_on_backend_init(CO_demo_storage_t *demo, CO_storage_t *storage,
                                     CO_storage_entry_t *entries, uint8_t entriesCount,
                                     CO_ReturnError_t initResult, uint32_t initError)
{
    CO_demo_storage_result_t layoutResult;

    if (demo == NULL) {
        return;
    }

    demo->storage = storage;
    demo->entries = entries;
    demo->entry = NULL;
    demo->entriesCount = entriesCount;
    demo->ready = 0U;
    demo->startupSeq++;
    demo->startupResult = (int32_t)initResult;
    demo->startupError = initError;
    demo->startupProbe = OD_PERSIST_COMM.x1017_producerHeartbeatTime;
    if (initResult == CO_ERROR_NO) {
        demo->startupState = (uint8_t)CO_DEMO_STORAGE_STARTUP_OK;
    } else if ((initResult == CO_ERROR_DATA_CORRUPT) && (initError != UINT32_MAX)) {
        demo->startupState = (uint8_t)CO_DEMO_STORAGE_STARTUP_DATA_CORRUPT;
    } else {
        demo->startupState = (uint8_t)CO_DEMO_STORAGE_STARTUP_ERROR;
    }

    if ((storage != NULL) && (entries != NULL) && (entriesCount > 0U)) {
        layoutResult = CO_demo_storage_refresh_layout(demo);
        if (layoutResult != CO_DEMO_STORAGE_RESULT_SUCCESS) {
            CO_RTT_LOG_W("storage diagnostic layout unavailable: result=%d", (int)layoutResult);
        }
    }

    CO_demo_storage_publish(demo);
}

void CO_demo_storage_process(CO_demo_storage_t *demo)
{
    uint32_t requestSeq;
    uint8_t command;

    if (demo == NULL) {
        return;
    }

    requestSeq = OD_RAM.x2305_storage_diagnostic.request_seq;
    if (requestSeq == demo->lastConsumedSeq) {
        return;
    }

    demo->lastConsumedSeq = requestSeq;
    demo->activeSeq = requestSeq;
    command = OD_RAM.x2305_storage_diagnostic.command;
    demo->result = (int32_t)CO_demo_storage_execute(demo, command);
    demo->completeSeq = requestSeq;
    CO_demo_storage_publish(demo);
}

void CO_demo_storage_reset(CO_demo_storage_t *demo)
{
    if (demo == NULL) {
        return;
    }

    /* Consume any request committed to the old stack. The RAM backup remains
     * valid because communication reset does not reset application memory. */
    demo->lastConsumedSeq = OD_RAM.x2305_storage_diagnostic.request_seq;
    CO_demo_storage_publish(demo);
}
