/**
 * @file CO_lss_persist_RTT.c
 * @brief Persistent LSS Node-ID and bitrate storage for RT-Thread storage backends.
 */

#include "CO_lss_persist_RTT.h"

#if defined(PKG_CANOPENNODE_LSS_PERSIST)

#include "301/crc16-ccitt.h"

#include <stddef.h>
#include <string.h>

#define CO_LSS_PERSIST_MAGIC                "LSS1"
#define CO_LSS_PERSIST_FORMAT_VERSION       1U
#define CO_LSS_PERSIST_FLAGS                0U
#define CO_LSS_PERSIST_COMMIT_MARKER        0xA55AU
#define CO_LSS_PERSIST_INVALID_MARKER       0x0000U

/**
 * @brief Fixed single-slot LSS persistence record.
 *
 * Multi-byte fields are byte arrays so the on-media layout is independent of
 * compiler alignment and target endianness. Field offsets and record size are
 * derived with offsetof() and sizeof().
 */
typedef struct {
    uint8_t magic[sizeof(CO_LSS_PERSIST_MAGIC) - 1U]; /**< Record magic. */
    uint8_t version;                                  /**< Record format version. */
    uint8_t flags;                                    /**< Reserved format flags. */
    uint8_t length[sizeof(uint16_t)];                  /**< Encoded record size. */
    uint8_t nodeId;                                   /**< Persisted CANopen Node-ID. */
    uint8_t reserved;                                 /**< Reserved byte, must be zero. */
    uint8_t bitrate[sizeof(uint16_t)];                 /**< Persisted bitrate in kbit/s. */
    uint8_t crc[sizeof(uint16_t)];                     /**< CRC16 over bytes before this field. */
    uint8_t commit[sizeof(uint16_t)];                  /**< Commit marker written last. */
} CO_lss_persist_record_t;

/** Serialized record size derived from the on-media record type. */
#define CO_LSS_PERSIST_RECORD_SIZE          ((size_t)sizeof(CO_lss_persist_record_t))
/** CRC field offset and CRC coverage length derived from the record type. */
#define CO_LSS_PERSIST_CRC_OFFSET           ((size_t)offsetof(CO_lss_persist_record_t, crc))
/** Commit field offset derived from the record type. */
#define CO_LSS_PERSIST_COMMIT_OFFSET        ((size_t)offsetof(CO_lss_persist_record_t, commit))

/** Check whether a Node-ID can be persisted as an LSS pending value. */
static bool_t co_lss_persist_node_id_valid(uint8_t nodeId)
{
    return (((nodeId >= 1U) && (nodeId <= 0x7FU)) || (nodeId == CO_LSS_NODE_ID_ASSIGNMENT));
}

/** Check whether a bitrate is defined by the CANopen LSS standard timing table. */
static bool_t co_lss_persist_bitrate_valid(uint16_t bitrate)
{
    size_t i;

    for (i = 0U; i < (sizeof(CO_LSS_bitTimingTableLookup) / sizeof(CO_LSS_bitTimingTableLookup[0])); i++) {
        if ((CO_LSS_bitTimingTableLookup[i] != 0U) && (CO_LSS_bitTimingTableLookup[i] == bitrate)) {
            return true;
        }
    }

    return false;
}

/** Check whether all record bytes still contain the erased storage value. */
static bool_t co_lss_persist_record_erased(const CO_lss_persist_record_t *record)
{
    const uint8_t *bytes = (const uint8_t *)record;
    size_t i;

    if (record == NULL) {
        return false;
    }

    for (i = 0U; i < sizeof(*record); i++) {
        if (bytes[i] != 0xFFU) {
            return false;
        }
    }

    return true;
}

/** Serialize the record body and CRC while keeping the commit marker invalid. */
static void co_lss_persist_record_encode(CO_lss_persist_record_t *record, uint8_t nodeId, uint16_t bitrate)
{
    if (record == NULL) {
        return;
    }

    memset(record, 0, sizeof(*record));
    memcpy(record->magic, CO_LSS_PERSIST_MAGIC, sizeof(record->magic));
    record->version = CO_LSS_PERSIST_FORMAT_VERSION;
    record->flags = CO_LSS_PERSIST_FLAGS;
    CO_setUint16(record->length, (uint16_t)CO_LSS_PERSIST_RECORD_SIZE);
    record->nodeId = nodeId;
    record->reserved = 0U;
    CO_setUint16(record->bitrate, bitrate);
    CO_setUint16(record->crc, crc16_ccitt((const uint8_t *)record, CO_LSS_PERSIST_CRC_OFFSET, 0U));
    CO_setUint16(record->commit, CO_LSS_PERSIST_INVALID_MARKER);
}

/** Validate the complete record and decode values only after every check passes. */
static bool_t co_lss_persist_record_decode(const CO_lss_persist_record_t *record, uint8_t *nodeId, uint16_t *bitrate)
{
    uint16_t savedBitrate;
    uint16_t savedCrc;
    uint16_t calculatedCrc;

    if ((record == NULL) || (nodeId == NULL) || (bitrate == NULL)) {
        return false;
    }
    if (memcmp(record->magic, CO_LSS_PERSIST_MAGIC, sizeof(record->magic)) != 0) {
        return false;
    }
    if ((record->version != CO_LSS_PERSIST_FORMAT_VERSION) || (record->flags != CO_LSS_PERSIST_FLAGS)
        || (CO_getUint16(record->length) != CO_LSS_PERSIST_RECORD_SIZE) || (record->reserved != 0U)
        || (CO_getUint16(record->commit) != CO_LSS_PERSIST_COMMIT_MARKER)) {
        return false;
    }

    savedBitrate = CO_getUint16(record->bitrate);
    savedCrc = CO_getUint16(record->crc);
    calculatedCrc = crc16_ccitt((const uint8_t *)record, CO_LSS_PERSIST_CRC_OFFSET, 0U);

    if ((savedCrc != calculatedCrc) || !co_lss_persist_node_id_valid(record->nodeId)
        || !co_lss_persist_bitrate_valid(savedBitrate)) {
        return false;
    }

    *nodeId = record->nodeId;
    *bitrate = savedBitrate;
    return true;
}

CO_lss_persist_load_result_t co_lss_persist_rtt_load(CO_storage_t *storage, uint8_t *nodeId, uint16_t *bitrate)
{
    CO_lss_persist_record_t record;
    uint8_t savedNodeId;
    uint16_t savedBitrate;

    if ((storage == NULL) || (nodeId == NULL) || (bitrate == NULL)) {
        return CO_LSS_PERSIST_LOAD_CONFIG_ERROR;
    }
    if (!co_storage_rtt_aux_read(storage, 0U, (uint8_t *)&record, CO_LSS_PERSIST_RECORD_SIZE)) {
        return CO_LSS_PERSIST_LOAD_IO_ERROR;
    }
    if (co_lss_persist_record_erased(&record)) {
        return CO_LSS_PERSIST_LOAD_EMPTY;
    }

    savedNodeId = *nodeId;
    savedBitrate = *bitrate;
    if (!co_lss_persist_record_decode(&record, &savedNodeId, &savedBitrate)) {
        return CO_LSS_PERSIST_LOAD_INVALID;
    }

    *nodeId = savedNodeId;
    *bitrate = savedBitrate;
    return CO_LSS_PERSIST_LOAD_OK;
}

bool_t co_lss_persist_rtt_store(CO_storage_t *storage, uint8_t nodeId, uint16_t bitrate)
{
    CO_lss_persist_record_t record;
    CO_lss_persist_record_t verify;
    uint8_t invalidMarker[sizeof(record.commit)];
    uint8_t commitMarker[sizeof(record.commit)];
    const size_t commitOffset = CO_LSS_PERSIST_COMMIT_OFFSET;

    if ((storage == NULL) || !co_lss_persist_node_id_valid(nodeId) || !co_lss_persist_bitrate_valid(bitrate)) {
        return false;
    }

    co_lss_persist_record_encode(&record, nodeId, bitrate);
    CO_setUint16(invalidMarker, CO_LSS_PERSIST_INVALID_MARKER);
    if (!co_storage_rtt_aux_write(storage, commitOffset, invalidMarker, sizeof(invalidMarker))) {
        return false;
    }
    if (!co_storage_rtt_aux_write(storage, 0U, (const uint8_t *)&record, commitOffset)) {
        return false;
    }
    if (!co_storage_rtt_aux_read(storage, 0U, (uint8_t *)&verify, commitOffset)
        || (memcmp(&record, &verify, commitOffset) != 0)) {
        return false;
    }

    CO_setUint16(commitMarker, CO_LSS_PERSIST_COMMIT_MARKER);
    if (!co_storage_rtt_aux_write(storage, commitOffset, commitMarker, sizeof(commitMarker))) {
        /* A failed callback may still have partially reached media. Best-effort invalidate the slot again. */
        (void)co_storage_rtt_aux_write(storage, commitOffset, invalidMarker, sizeof(invalidMarker));
        return false;
    }

    /*
     * The body was already read back before commit. aux_write() returning true
     * guarantees the valid marker reached media in program order, so success is
     * final here. A later readback failure must not turn a committed record into
     * a reported store failure while leaving that record valid on media.
     */
    return true;
}

#endif /* defined(PKG_CANOPENNODE_LSS_PERSIST) */
