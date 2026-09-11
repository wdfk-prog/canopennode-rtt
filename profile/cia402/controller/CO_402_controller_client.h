/**
 * @file CO_402_controller_client.h
 * @brief Portable SDO control-plane adapter for the transport-neutral CiA 402 Controller.
 */

#ifndef CO_402_CONTROLLER_CLIENT_H_
#define CO_402_CONTROLLER_CLIENT_H_

#include <stdbool.h>
#include <stdint.h>

#include "CO_402_controller.h"
#include "CO_profile_transport.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Remote-axis declaration for one portable CiA 402 Controller client. */
typedef struct {
    uint8_t nodeId; /**< Remote CANopen Node-ID in range 1..127. */
    uint8_t logicalDevice; /**< Remote CiA 301 logical-device slot containing this CiA 402 axis. */
    CO_402_controller_config_t controller; /**< PDS transition/feedback timeout policy. */
} CO_402_controller_client_config_t;

/** One observable portable SDO-backed Controller step. */
typedef struct {
    CO_402_controller_result_t controllerResult; /**< PDS semantic result after the latest core process step. */
    CO_profile_transfer_result_t statuswordTransfer; /**< Terminal 0x6041 upload result when attempted. */
    CO_profile_transfer_result_t controlwordReadTransfer; /**< Terminal 0x6040 preservation read when attempted. */
    CO_profile_transfer_result_t controlwordWriteTransfer; /**< Terminal 0x6040 command write when attempted. */
    uint16_t statusword; /**< Decoded remote Statusword when statuswordTransfer is OK. */
    uint16_t controlword; /**< Complete merged Controlword attempted by the client. */
    bool statuswordValid; /**< True when this step obtained a fresh valid-size 0x6041 value. */
    bool commandPending; /**< True while a generated PDS update still needs successful transport acceptance. */
    bool commandAccepted; /**< True when this step successfully wrote the pending PDS update. */
} CO_402_controller_client_step_result_t;

/** Caller-owned portable Controller client for one remote CiA 402 axis. */
typedef struct {
    CO_profile_transport_t *transport; /**< Non-owning portable transport binding supplied by the platform backend. */
    CO_402_controller_axis_t controller; /**< Stack-neutral PDS Controller core. */
    uint8_t nodeId; /**< Remote CANopen Node-ID. */
    uint8_t logicalDevice; /**< Remote logical-device slot. */
    CO_402_controller_controlword_update_t pendingUpdate; /**< PDS update retried until transport accepts it. */
    uint32_t deferredElapsedUs; /**< Elapsed transport-blocked time pending feedback/transition accounting. */
    bool pendingUpdateValid; /**< True while @ref pendingUpdate must not be regenerated or skipped. */
    bool pendingFaultResetEdge; /**< True when pendingUpdate is a mandatory Fault Reset high/low edge. */
    bool initialized; /**< True after successful client initialization. */
} CO_402_controller_client_t;

/**
 * @brief Initialize one remote CiA 402 client without owning transport or OS resources.
 *
 * @p transport and its backend context must outlive @p client. The client stores no CANopen stack object; every SDO
 * operation re-enters the common transport contract so reset/teardown readiness stays backend-owned.
 *
 * @param client Caller-owned client storage.
 * @param transport Initialized portable profile transport binding.
 * @param config Remote Node-ID, logical-device slot and PDS timeout policy.
 * @return CO_PROFILE_CALL_OK on success or CO_PROFILE_CALL_INVALID_ARGUMENT for invalid input.
 */
CO_profile_call_result_t CO_402_controller_client_init(CO_402_controller_client_t *client,
                                                        CO_profile_transport_t *transport,
                                                        const CO_402_controller_client_config_t *config);

/**
 * @brief Request one PDS target through the shared Controller core.
 *
 * An ordinary PDS update that failed transport may be replaced when the target changes. A pending Fault Reset high/low
 * edge is never discarded because losing either edge violates the explicit pulse contract.
 *
 * @param client Initialized remote-axis client.
 * @param target Requested PDS target.
 * @return true when the request is accepted or is an idempotent repeat; otherwise false.
 */
bool CO_402_controller_client_setTarget(CO_402_controller_client_t *client,
                                         CO_402_controller_target_t target);

/**
 * @brief Clear the active PDS target while preserving any mandatory pending Fault Reset edge.
 *
 * Retry time accumulated before the clear is retained only as feedback age and does not consume a later target's new
 * transition timeout budget.
 *
 * @param client Initialized remote-axis client; NULL is accepted and ignored.
 */
void CO_402_controller_client_clearTarget(CO_402_controller_client_t *client);

/**
 * @brief Request one explicit CiA 402 Fault Reset pulse.
 *
 * @param client Initialized remote-axis client.
 * @return true when the shared Controller accepted the reset request; otherwise false.
 */
bool CO_402_controller_client_requestFaultReset(CO_402_controller_client_t *client);

/**
 * @brief Execute one blocking transport-backed PDS Controller step.
 *
 * Failed Controlword transport does not advance the semantic Controller past the generated update. This preserves the
 * mandatory Fault Reset high/low pulse and prevents transport retries from silently skipping a PDS command. Elapsed
 * time while that Controlword is still unaccepted ages feedback only and does not consume the transition budget. Once
 * the pending Controlword is accepted, later Statusword call-level outages are replayed into the next semantic step so
 * an active transition still observes wall-clock timeout; a newly observed remote state resets its per-state timer.
 * Replacing or clearing a target commits older deferred time to feedback so it cannot enter the rebuilt transition
 * budget.
 * The client reads the complete remote Controlword before merging PDS-owned bits so operation-mode-specific bits remain
 * owned by higher-level logic.
 * Because that preservation is an SDO upload followed by a separate SDO download, all writers of remote 0x6040 must
 * be externally serialized across the complete read-modify-write window. Per-call transport serialization does not
 * make the pair atomic; applications with mode-specific Controlword writers need one shared owner/lock.
 *
 * This is a commissioning/control-plane adapter. Cyclic PP/PV/HM/CSP/CSV/CST process data should use a PDO/SYNC
 * backend or product-specific fast path while reusing the same Pure-C Controller state semantics.
 *
 * @param client Initialized remote-axis client.
 * @param timeDifferenceUs Elapsed Controller time for this invocation; use a per-call delta after transport failures.
 * @param result Detailed semantic and transport result for this call.
 * @return Portable call/admission status for the attempted transport operations.
 */
CO_profile_call_result_t CO_402_controller_client_process(CO_402_controller_client_t *client,
                                                           uint32_t timeDifferenceUs,
                                                           CO_402_controller_client_step_result_t *result);

/**
 * @brief Return the latest recognized remote PDS state from the shared Controller core.
 *
 * @param client Initialized remote-axis client.
 * @return Latest recognized PDS state, or CO_402_STATE_UNKNOWN for invalid/uninitialized input.
 */
CO_402_state_t CO_402_controller_client_getRemoteState(const CO_402_controller_client_t *client);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_CONTROLLER_CLIENT_H_ */
