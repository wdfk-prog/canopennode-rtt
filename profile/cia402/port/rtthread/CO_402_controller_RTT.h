/**
 * @file CO_402_controller_RTT.h
 * @brief RT-Thread compatibility facade for the portable CiA 402 Controller client.
 */

#ifndef CO_402_CONTROLLER_RTT_H_
#define CO_402_CONTROLLER_RTT_H_

#include <stdbool.h>
#include <stdint.h>

#include <rtthread.h>

#include "CO_402_controller_client.h"
#include "CO_profile_master_RTT.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Source-compatible RT-Thread configuration alias for the portable remote-axis declaration. */
typedef CO_402_controller_client_config_t CO_402_controller_RTT_config_t;

/** Source-compatible RT-Thread step-result alias. */
typedef CO_402_controller_client_step_result_t CO_402_controller_RTT_step_result_t;

/** Source-compatible RT-Thread runtime alias; portable Controller state owns no RT-Thread resources. */
typedef CO_402_controller_client_t CO_402_controller_RTT_t;

/**
 * @brief Initialize one remote CiA 402 client on the common RT-Thread Master backend.
 * @param adapter Caller-owned adapter storage.
 * @param transport Attached common RT-Thread profile Master transport.
 * @param config Remote Node-ID, logical-device slot and PDS timeout policy.
 * @return RT_EOK on success or an RT-Thread error when the portable client/backend cannot be bound.
 */
rt_err_t CO_402_controller_RTT_init(CO_402_controller_RTT_t *adapter, CO_profile_master_RTT_t *transport,
                                    const CO_402_controller_RTT_config_t *config);

/**
 * @brief Request one PDS target through the portable Controller client.
 * @param adapter Initialized remote-axis adapter.
 * @param target Requested PDS target.
 * @return true when the request is accepted or is an idempotent repeat; otherwise false.
 */
bool CO_402_controller_RTT_setTarget(CO_402_controller_RTT_t *adapter, CO_402_controller_target_t target);

/**
 * @brief Clear the active target while preserving any mandatory pending Fault Reset edge.
 * @param adapter Initialized remote-axis adapter; NULL is accepted and ignored.
 */
void CO_402_controller_RTT_clearTarget(CO_402_controller_RTT_t *adapter);

/**
 * @brief Request one explicit CiA 402 Fault Reset pulse.
 * @param adapter Initialized remote-axis adapter.
 * @return true when the portable Controller client accepts the request; otherwise false.
 */
bool CO_402_controller_RTT_requestFaultReset(CO_402_controller_RTT_t *adapter);

/**
 * @brief Execute one blocking SDO-backed PDS Controller step through the RT-Thread Master backend.
 *
 * The semantic state/retry behavior is implemented by the portable Controller client. This facade only converts the
 * stack-neutral call result to the existing RT-Thread error contract.
 *
 * @param adapter Initialized remote-axis adapter.
 * @param timeDifferenceUs Elapsed Controller time for this invocation; use a per-call delta after transport failures.
 * @param result Detailed semantic and transport result for this call.
 * @return RT_EOK when attempted transport operations reached terminal results, or an RT-Thread admission/IPC error.
 */
rt_err_t CO_402_controller_RTT_process(CO_402_controller_RTT_t *adapter, uint32_t timeDifferenceUs,
                                       CO_402_controller_RTT_step_result_t *result);

/**
 * @brief Return the latest recognized remote PDS state from the shared Controller core.
 * @param adapter Initialized remote-axis adapter.
 * @return Latest recognized PDS state, or CO_402_STATE_UNKNOWN for invalid/uninitialized input.
 */
CO_402_state_t CO_402_controller_RTT_getRemoteState(const CO_402_controller_RTT_t *adapter);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_CONTROLLER_RTT_H_ */
