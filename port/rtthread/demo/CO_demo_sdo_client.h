/**
 * @file CO_demo_sdo_client.h
 * @brief Test-only non-blocking SDO client transaction driver.
 */

#ifndef CO_DEMO_SDO_CLIENT_H_
#define CO_DEMO_SDO_CLIENT_H_

#include "CANopen.h"

#if !defined(PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST)
#error "CO_demo_sdo_client requires PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST"
#endif /* !defined(PKG_CANOPENNODE_DEMO_SDO_CLIENT_TEST) */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** SDO client test command published through OD 0x2303:02. */
typedef enum {
    CO_DEMO_SDO_CLIENT_COMMAND_NONE = 0, /**< No transaction requested. */
    CO_DEMO_SDO_CLIENT_COMMAND_UPLOAD = 1, /**< Upload data from the selected SDO server. */
    CO_DEMO_SDO_CLIENT_COMMAND_DOWNLOAD = 2 /**< Download generated test data to the selected SDO server. */
} CO_demo_sdo_client_command_t;

/** Normalized application result published through OD 0x2303:0B. */
typedef enum {
    CO_DEMO_SDO_CLIENT_RESULT_NONE = 0, /**< No completed transaction is available. */
    CO_DEMO_SDO_CLIENT_RESULT_SUCCESS = 1, /**< The CANopenNode SDO transaction completed successfully. */
    CO_DEMO_SDO_CLIENT_RESULT_ABORT = 2, /**< The SDO server/client ended the transaction with an abort code. */
    CO_DEMO_SDO_CLIENT_RESULT_TIMEOUT = 3, /**< The CANopenNode SDO client reported CO_SDO_AB_TIMEOUT. */
    CO_DEMO_SDO_CLIENT_RESULT_RESET_CANCELLED = 4, /**< Communication reset cancelled an active transaction. */
    CO_DEMO_SDO_CLIENT_RESULT_SETUP_ERROR = 5, /**< Request validation, setup, or initiate failed. */
    CO_DEMO_SDO_CLIENT_RESULT_UNSUPPORTED = 6, /**< The request uses a test option not enabled by J04. */
    CO_DEMO_SDO_CLIENT_RESULT_INTERNAL_ERROR = 7 /**< Test-wrapper bookkeeping detected an inconsistent result. */
} CO_demo_sdo_client_result_t;

/** Long-lived state for one test-only SDO client transaction driver. */
typedef struct {
    uint8_t phase; /**< Internal non-blocking transaction phase. */
    uint8_t command; /**< Latched CO_demo_sdo_client_command_t for the active request. */
    uint8_t remoteNode; /**< Latched target SDO server Node-ID. */
    uint8_t subIndex; /**< Latched target Object Dictionary sub-index. */
    uint8_t flags; /**< Latched request flags; J04 accepts only zero. */
    uint16_t index; /**< Latched target Object Dictionary index. */
    uint32_t lastConsumedSeq; /**< Last OD request sequence accepted by mainline. */
    uint32_t activeSeq; /**< Request sequence currently being processed, or the latest completed one. */
    uint32_t completeSeq; /**< Last request sequence with a published terminal result. */
    uint32_t payloadSize; /**< Requested DOWNLOAD payload size in bytes. */
    uint32_t probeValue; /**< U32 probe value or deterministic payload seed. */
    uint32_t transferredSize; /**< Number of bytes actually consumed or produced by the transaction. */
    uint32_t resultValue; /**< First four uploaded bytes decoded as CANopen little-endian U32. */
    uint32_t checksum; /**< FNV-1a checksum of bytes transferred by the test wrapper. */
    uint32_t byteOffset; /**< Number of payload bytes generated or consumed so far. */
    uint32_t checksumState; /**< Running FNV-1a checksum state for the active transaction. */
    uint8_t resultBytes[4]; /**< First uploaded bytes retained for resultValue publication. */
    uint8_t resultByteCount; /**< Number of valid bytes currently stored in resultBytes. */
    int32_t result; /**< Latest CO_demo_sdo_client_result_t terminal result. */
    CO_SDO_abortCode_t abortCode; /**< Latest CANopen SDO abort code, or CO_SDO_AB_NONE. */
} CO_demo_sdo_client_t;

/**
 * @brief Initialize SDO client test state from the current OD request sequence.
 *
 * @param demo SDO client test state.
 */
void CO_demo_sdo_client_init(CO_demo_sdo_client_t *demo);

/**
 * @brief Bind the test driver to a newly initialized CANopenNode stack.
 *
 * The current request sequence is consumed without replay. A terminal result
 * already published before communication reset is preserved across rebind.
 *
 * @param demo SDO client test state.
 * @param co Current CANopenNode object.
 * @return true when at least one SDO client object is available.
 */
bool_t CO_demo_sdo_client_bind(CO_demo_sdo_client_t *demo, CO_t *co);

/**
 * @brief Process one non-blocking SDO client test iteration from mainline.
 *
 * @param demo SDO client test state.
 * @param co Current CANopenNode object.
 * @param localNodeId Active local CANopen Node-ID included in diagnostic logs.
 * @param timeDifferenceUs Elapsed mainline time passed to CANopenNode in microseconds.
 * @param resetStatus Reset request returned by the preceding CO_process() call.
 */
void CO_demo_sdo_client_process(CO_demo_sdo_client_t *demo, CO_t *co, uint8_t localNodeId,
                                uint32_t timeDifferenceUs, CO_NMT_reset_cmd_t resetStatus);

/**
 * @brief Consume pending SDO client requests before local stack recreation.
 *
 * @param demo SDO client test state.
 */
void CO_demo_sdo_client_reset(CO_demo_sdo_client_t *demo);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_DEMO_SDO_CLIENT_H_ */
