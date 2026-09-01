/**
 * @file CO_demo_storage.h
 * @brief Test-only CANopenNode EEPROM storage diagnostic contract.
 */

#ifndef CO_DEMO_STORAGE_H_
#define CO_DEMO_STORAGE_H_

#include "CANopen.h"
#include "OD.h"
#include "storage/CO_storage.h"

#if !defined(PKG_CANOPENNODE_DEMO_STORAGE_DIAGNOSTIC)
#error "CO_demo_storage requires PKG_CANOPENNODE_DEMO_STORAGE_DIAGNOSTIC"
#endif /* !defined(PKG_CANOPENNODE_DEMO_STORAGE_DIAGNOSTIC) */

#if !defined(PKG_CANOPENNODE_USING_STORAGE_EEPROM) || !defined(PKG_CANOPENNODE_USING_STORAGE_AT24C)
#error "CO_demo_storage requires the built-in AT24CXX EEPROM storage backend"
#endif /* !defined(PKG_CANOPENNODE_USING_STORAGE_EEPROM) || !defined(PKG_CANOPENNODE_USING_STORAGE_AT24C) */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Maximum number of bytes transferred through one raw diagnostic word. */
#define CO_DEMO_STORAGE_RAW_WORD_SIZE 4U

/**
 * Maximum raw bytes covered by the first PERSIST_COMM storage entry.
 *
 * CO_storageEeprom places the complete fixed signature table before the first
 * payload. The diagnostic targets OD 0x1010/0x1011 sub-index 2, which is required to
 * be the first configured entry when this diagnostic is enabled.
 */
#define CO_DEMO_STORAGE_BACKUP_CAPACITY \
    (sizeof(OD_PERSIST_COMM_t) + (sizeof(uint32_t) * CO_CONFIG_STORAGE_MAX_ENTRIES_COUNT))

/** Storage diagnostic command published through OD 0x2305:02. */
typedef enum {
    CO_DEMO_STORAGE_COMMAND_NONE = 0, /**< No diagnostic operation requested. */
    CO_DEMO_STORAGE_COMMAND_REFRESH = 1, /**< Refresh layout and current signature metadata. */
    CO_DEMO_STORAGE_COMMAND_BACKUP = 2, /**< Copy the complete raw COMM entry region into target RAM. */
    CO_DEMO_STORAGE_COMMAND_RESTORE = 3, /**< Restore and verify the target-RAM raw backup. */
    CO_DEMO_STORAGE_COMMAND_CORRUPT_SIGNATURE = 4, /**< Flip one CRC bit in the stored COMM signature. */
    CO_DEMO_STORAGE_COMMAND_CORRUPT_DATA = 5, /**< Flip one byte in the stored COMM payload. */
    CO_DEMO_STORAGE_COMMAND_RAW_READ = 6, /**< Read one to four raw bytes into OD 0x2305:06. */
    CO_DEMO_STORAGE_COMMAND_RAW_WRITE = 7, /**< Write and verify one to four raw bytes from OD 0x2305:06. */
    CO_DEMO_STORAGE_COMMAND_CLEAR_BACKUP = 8 /**< Invalidate the target-RAM backup without touching EEPROM. */
} CO_demo_storage_command_t;

/** Normalized result published through OD 0x2305:09. */
typedef enum {
    CO_DEMO_STORAGE_RESULT_NONE = 0, /**< No completed diagnostic request is represented. */
    CO_DEMO_STORAGE_RESULT_SUCCESS = 1, /**< The requested diagnostic operation completed successfully. */
    CO_DEMO_STORAGE_RESULT_INVALID_ARGUMENT = 2, /**< One or more request fields are invalid. */
    CO_DEMO_STORAGE_RESULT_NOT_READY = 3, /**< Storage or the AT24CXX module is not ready. */
    CO_DEMO_STORAGE_RESULT_ENTRY_NOT_FOUND = 4, /**< Requested storage sub-index is not configured. */
    CO_DEMO_STORAGE_RESULT_UNSUPPORTED_LAYOUT = 5, /**< The raw layout is not safe for this storage diagnostic. */
    CO_DEMO_STORAGE_RESULT_RANGE_ERROR = 6, /**< Requested raw range lies outside the selected entry region. */
    CO_DEMO_STORAGE_RESULT_IO_ERROR = 7, /**< EEPROM read or write failed. */
    CO_DEMO_STORAGE_RESULT_VERIFY_ERROR = 8, /**< EEPROM read-back did not match the requested write. */
    CO_DEMO_STORAGE_RESULT_BACKUP_INVALID = 9 /**< No matching target-RAM backup is available for restore. */
} CO_demo_storage_result_t;

/** Normalized storage initialization state published through OD 0x2305:0A. */
typedef enum {
    CO_DEMO_STORAGE_STARTUP_UNKNOWN = 0, /**< No storage initialization result has been captured yet. */
    CO_DEMO_STORAGE_STARTUP_OK = 1, /**< Storage initialization completed without reported corruption. */
    CO_DEMO_STORAGE_STARTUP_DATA_CORRUPT = 2, /**< Persistent data is invalid and initError carries a sub-index mask. */
    CO_DEMO_STORAGE_STARTUP_ERROR = 3 /**< Storage initialization failed with another error. */
} CO_demo_storage_startup_state_t;

/** Long-lived test-only storage diagnostic state for one application instance. */
typedef struct {
    CO_storage_t *storage; /**< Current storage object, refreshed after each communication reset. */
    CO_storage_entry_t *entries; /**< Current storage entry array configured by the application. */
    CO_storage_entry_t *entry; /**< Selected first PERSIST_COMM storage entry. */
    uint8_t entriesCount; /**< Number of storage entries configured by the application. */
    uint8_t ready; /**< Non-zero when the selected raw layout is valid and accessible. */
    uint32_t lastConsumedSeq; /**< Last OD request sequence consumed by mainline. */
    uint32_t activeSeq; /**< Request sequence currently being executed or most recently accepted. */
    uint32_t completeSeq; /**< Last request sequence with a published terminal result. */
    int32_t result; /**< Latest CO_demo_storage_result_t terminal result. */
    uint32_t startupSeq; /**< Counts storage initialization attempts in the current MCU boot. */
    int32_t startupResult; /**< Raw CO_ReturnError_t returned by the latest storage initialization. */
    uint32_t startupError; /**< Backend-specific storage initialization detail or corruption bitmask. */
    uint16_t startupProbe; /**< 0x1017 value immediately after storage load and before master reconfiguration. */
    uint8_t startupState; /**< Latest CO_demo_storage_startup_state_t value. */
    size_t storageOffset; /**< Board-configured CANopenNode EEPROM storage start offset. */
    size_t storageRegionSize; /**< Effective CANopenNode EEPROM reserved region size. */
    size_t eepromSize; /**< AT24CXX EEPROM capacity in bytes. */
    size_t pageSize; /**< AT24CXX EEPROM page size in bytes. */
    uint8_t addrInput; /**< AT24CXX AddrInput value used by the storage adapter. */
    size_t signatureAddress; /**< EEPROM address of the selected entry signature. */
    size_t dataAddress; /**< EEPROM address of the selected entry payload. */
    size_t dataLength; /**< Selected entry payload length in bytes. */
    size_t rawStart; /**< First EEPROM byte included in complete raw baseline backup. */
    size_t rawLength; /**< Number of EEPROM bytes included in complete raw baseline backup. */
    uint32_t signatureValue; /**< Current 32-bit CO_storageEeprom signature value. */
    uint8_t backupValid; /**< Non-zero only when backupRaw contains a complete verified baseline read. */
    uint8_t backupEntrySubIndex; /**< Storage sub-index associated with the current target-RAM backup. */
    uint16_t backupCrc; /**< CRC16-CCITT of backupRaw for diagnostic comparison. */
    size_t backupRawStart; /**< Raw EEPROM start address associated with backupRaw. */
    size_t backupRawLength; /**< Number of valid bytes currently stored in backupRaw. */
    uint8_t backupRaw[CO_DEMO_STORAGE_BACKUP_CAPACITY]; /**< Target-RAM copy of the complete raw COMM entry region. */
} CO_demo_storage_t;

/**
 * @brief Initialize the test-only storage diagnostic state.
 *
 * The target-RAM baseline buffer starts invalid and remains invalid until the
 * Host explicitly commits a BACKUP command.
 *
 * @param demo Storage diagnostic state.
 */
void CO_demo_storage_init(CO_demo_storage_t *demo);

/**
 * @brief Capture one storage backend initialization result and refresh diagnostic layout.
 *
 * This hook must run immediately after co_storage_rtt_init(), before CANopen
 * communication objects can be reconfigured by the Host. It preserves the
 * freshly loaded 0x1017 value for reset and power-cycle assertions.
 *
 * @param demo Storage diagnostic state.
 * @param storage Current initialized storage object.
 * @param entries Storage entry array configured by the RT-Thread application.
 * @param entriesCount Number of entries in @p entries.
 * @param initResult Raw storage initialization return code.
 * @param initError Backend-specific initialization detail or corruption bitmask.
 */
void CO_demo_storage_on_backend_init(CO_demo_storage_t *demo, CO_storage_t *storage,
                                     CO_storage_entry_t *entries, uint8_t entriesCount,
                                     CO_ReturnError_t initResult, uint32_t initError);

/**
 * @brief Execute a newly committed storage diagnostic request from mainline.
 *
 * @param demo Storage diagnostic state.
 */
void CO_demo_storage_process(CO_demo_storage_t *demo);

/**
 * @brief Consume pending diagnostic requests before local communication reset.
 *
 * The target-RAM backup is intentionally preserved across communication reset,
 * but a real MCU reset or power cycle clears it. Host-side cleanup therefore
 * keeps its own complete raw baseline for reset and power-loss modes.
 *
 * @param demo Storage diagnostic state.
 */
void CO_demo_storage_reset(CO_demo_storage_t *demo);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_DEMO_STORAGE_H_ */
