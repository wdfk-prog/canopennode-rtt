/**
 * @file CO_profile_transport.h
 * @brief Stack- and RTOS-neutral transport contract for remote Device Profile controllers.
 */

#ifndef CO_PROFILE_TRANSPORT_H_
#define CO_PROFILE_TRANSPORT_H_

#include <stdbool.h>
#include <stdint.h>

#include "CO_profile_controller.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Stack-neutral admission result for one controller-to-transport call. */
typedef enum {
    CO_PROFILE_CALL_OK = 0, /**< The transport accepted the call and published a terminal transfer result. */
    CO_PROFILE_CALL_INVALID_ARGUMENT, /**< The portable contract rejected an invalid argument. */
    CO_PROFILE_CALL_NOT_READY, /**< The backend is not ready for a controller operation. */
    CO_PROFILE_CALL_BUSY, /**< The backend cannot admit another operation in the current context/state. */
    CO_PROFILE_CALL_CONTEXT_ERROR, /**< The caller context is not permitted by this backend. */
    CO_PROFILE_CALL_BACKEND_ERROR /**< A backend-local admission/IPC failure occurred. */
} CO_profile_call_status_t;

/** Portable call status plus the optional backend-native error value. */
typedef struct {
    CO_profile_call_status_t status; /**< Portable admission classification. */
    int32_t backendError; /**< Backend-native error, or zero when no backend-specific value exists. */
} CO_profile_call_result_t;

/** CiA 301 NMT command values exposed without a CANopen stack dependency. */
typedef enum {
    CO_PROFILE_NMT_ENTER_OPERATIONAL = 0x01, /**< Request NMT Operational. */
    CO_PROFILE_NMT_ENTER_STOPPED = 0x02, /**< Request NMT Stopped. */
    CO_PROFILE_NMT_ENTER_PRE_OPERATIONAL = 0x80, /**< Request NMT Pre-operational. */
    CO_PROFILE_NMT_RESET_NODE = 0x81, /**< Request Reset Node. */
    CO_PROFILE_NMT_RESET_COMMUNICATION = 0x82 /**< Request Reset Communication. */
} CO_profile_nmt_command_t;

struct CO_profile_transport;
typedef struct CO_profile_transport CO_profile_transport_t;

/** Backend operations used by portable Device Profile Controller control-plane clients. */
typedef struct {
    /** Blocking or otherwise terminalizing scalar upload implemented by the concrete backend. */
    CO_profile_call_result_t (*sdoUpload)(void *context, uint8_t remoteNodeId, uint16_t index,
                                          uint8_t subIndex, uint8_t expectedSize,
                                          CO_profile_transfer_result_t *result);
    /** Blocking or otherwise terminalizing scalar download implemented by the concrete backend. */
    CO_profile_call_result_t (*sdoDownload)(void *context, uint8_t remoteNodeId, uint16_t index,
                                            uint8_t subIndex, const void *data, uint8_t size,
                                            CO_profile_transfer_result_t *result);
    /** Optional NMT command dispatch; SDO-only profile clients do not require this capability. */
    CO_profile_call_result_t (*nmtCommand)(void *context, CO_profile_nmt_command_t command,
                                           uint8_t nodeId, CO_profile_transfer_result_t *result);
} CO_profile_transport_ops_t;

/** Portable binding between one backend-owned context and its transport operations. */
struct CO_profile_transport {
    void *context; /**< Backend-owned context forwarded unchanged to every operation. */
    const CO_profile_transport_ops_t *ops; /**< Backend operation table; must outlive this binding. */
};

/**
 * @brief Bind one portable transport object to a backend context and operation table.
 *
 * The binding owns neither @p context nor @p ops. Their lifetime must cover every controller using the binding.
 *
 * @param transport Caller-owned portable transport binding.
 * @param context Backend-owned context; NULL is permitted when the backend does not require state.
 * @param ops Backend operation table. SDO upload/download are mandatory; NMT dispatch is optional.
 * @return true when the mandatory SDO operations are present; otherwise false and @p transport is cleared.
 */
bool CO_profileTransport_init(CO_profile_transport_t *transport, void *context,
                              const CO_profile_transport_ops_t *ops);

/**
 * @brief Upload one bounded scalar through the selected backend.
 *
 * @param transport Initialized portable transport binding.
 * @param remoteNodeId Remote CANopen Node-ID in range 1..127.
 * @param index Absolute remote OD index.
 * @param subIndex Remote OD sub-index.
 * @param expectedSize Exact scalar width in bytes, range 1..8.
 * @param result Terminal transfer result populated when the backend accepts the call.
 * @return Portable call/admission status. Protocol completion is reported through @p result.
 */
CO_profile_call_result_t CO_profileTransport_sdoUpload(CO_profile_transport_t *transport,
                                                        uint8_t remoteNodeId, uint16_t index,
                                                        uint8_t subIndex, uint8_t expectedSize,
                                                        CO_profile_transfer_result_t *result);

/**
 * @brief Download one bounded scalar through the selected backend.
 *
 * @param transport Initialized portable transport binding.
 * @param remoteNodeId Remote CANopen Node-ID in range 1..127.
 * @param index Absolute remote OD index.
 * @param subIndex Remote OD sub-index.
 * @param data CANopen little-endian scalar bytes.
 * @param size Exact scalar width in bytes, range 1..8.
 * @param result Terminal transfer result populated when the backend accepts the call.
 * @return Portable call/admission status. Protocol completion is reported through @p result.
 */
CO_profile_call_result_t CO_profileTransport_sdoDownload(CO_profile_transport_t *transport,
                                                          uint8_t remoteNodeId, uint16_t index,
                                                          uint8_t subIndex, const void *data,
                                                          uint8_t size,
                                                          CO_profile_transfer_result_t *result);

/**
 * @brief Emit one CiA 301 NMT command through the selected backend.
 *
 * @param transport Initialized portable transport binding.
 * @param command One of the five CiA 301 NMT command values declared by CO_profile_nmt_command_t.
 * @param nodeId Remote Node-ID in range 1..127 or zero for broadcast.
 * @param result Terminal local/protocol result populated when the backend accepts the call.
 * @return Portable call/admission status. CO_PROFILE_CALL_NOT_READY reports a backend without NMT capability;
 *         terminal local/protocol state is reported through @p result after an accepted call.
 */
CO_profile_call_result_t CO_profileTransport_nmtCommand(CO_profile_transport_t *transport,
                                                         CO_profile_nmt_command_t command,
                                                         uint8_t nodeId,
                                                         CO_profile_transfer_result_t *result);

/**
 * @brief Check whether a value is one of the five CiA 301 NMT commands exposed by this contract.
 *
 * @param command Candidate NMT command value.
 * @return true only for a supported CiA 301 NMT command; otherwise false.
 */
bool CO_profileTransport_nmtCommandValid(CO_profile_nmt_command_t command);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_PROFILE_TRANSPORT_H_ */
