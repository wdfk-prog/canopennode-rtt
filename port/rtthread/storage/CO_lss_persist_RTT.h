/**
 * @file CO_lss_persist_RTT.h
 * @brief Persistent LSS Node-ID and bitrate storage for RT-Thread storage backends.
 */

#ifndef CO_LSS_PERSIST_RTT_H_
#define CO_LSS_PERSIST_RTT_H_

#include "CO_storage_RTT.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief Result of loading the persistent LSS configuration record.
 */
typedef enum {
    CO_LSS_PERSIST_LOAD_OK = 0,      /**< A valid record was loaded. */
    CO_LSS_PERSIST_LOAD_EMPTY,       /**< The reserved record is erased and no saved configuration exists. */
    CO_LSS_PERSIST_LOAD_INVALID,     /**< The record exists but failed format, CRC, range, or commit validation. */
    CO_LSS_PERSIST_LOAD_IO_ERROR,    /**< The backend record could not be read. */
    CO_LSS_PERSIST_LOAD_CONFIG_ERROR /**< The selected storage backend cannot provide the auxiliary area. */
} CO_lss_persist_load_result_t;

/**
 * @brief Load a saved LSS Node-ID and bitrate from the selected storage backend.
 *
 * Output values are changed only after the complete record passes validation.
 *
 * @param storage Storage object prepared for auxiliary persistence.
 * @param nodeId In/out Node-ID. Preserved when no valid record can be loaded.
 * @param bitrate In/out CAN bitrate in kbit/s. Preserved when no valid record can be loaded.
 * @return Load result describing whether a saved configuration was applied.
 */
CO_lss_persist_load_result_t co_lss_persist_rtt_load(CO_storage_t *storage, uint8_t *nodeId, uint16_t *bitrate);

/**
 * @brief Store an LSS Node-ID and bitrate in the selected storage backend.
 *
 * The previous commit marker is invalidated first and the valid marker is written
 * only after the body and CRC have been read back successfully. If persistence is
 * interrupted during the write, the record is intentionally invalid on the next
 * startup.
 *
 * @param storage Storage object prepared for auxiliary persistence.
 * @param nodeId Node-ID in range 1..127 or CO_LSS_NODE_ID_ASSIGNMENT.
 * @param bitrate CAN bitrate in kbit/s from the standard LSS timing table.
 * @return true after body readback verification and successful commit write, otherwise false.
 */
bool_t co_lss_persist_rtt_store(CO_storage_t *storage, uint8_t nodeId, uint16_t bitrate);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_LSS_PERSIST_RTT_H_ */
