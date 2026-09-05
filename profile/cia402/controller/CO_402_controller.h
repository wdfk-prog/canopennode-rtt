/**
 * @file CO_402_controller.h
 * @brief Transport-agnostic CiA 402 Controller PDS command sequencer.
 */

#ifndef CO_402_CONTROLLER_H
#define CO_402_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

#include "CO_402_state.h"

#ifdef __cplusplus
extern "C" {
#endif

/** PDS-owned Controlword bits: bits 0..3 plus Fault Reset bit 7. */
#define CO_402_CONTROLLER_PDS_CONTROLWORD_MASK 0x008FU

/** A zero timeout disables the corresponding timeout check. */
#define CO_402_CONTROLLER_TIMEOUT_DISABLED 0U

/** User-requestable remote PDS targets. Observation-only states are intentionally excluded. */
typedef enum {
    CO_402_CONTROLLER_TARGET_SWITCH_ON_DISABLED = 0, /**< Request Switch on disabled. */
    CO_402_CONTROLLER_TARGET_READY_TO_SWITCH_ON,     /**< Request Ready to switch on. */
    CO_402_CONTROLLER_TARGET_SWITCHED_ON,            /**< Request Switched on. */
    CO_402_CONTROLLER_TARGET_OPERATION_ENABLED,      /**< Request Operation enabled. */
    CO_402_CONTROLLER_TARGET_QUICK_STOP_ACTIVE       /**< Request Quick stop active. */
} CO_402_controller_target_t;

/** Runtime result returned by one Controller processing step. */
typedef enum {
    CO_402_CONTROLLER_RESULT_IDLE = 0,          /**< No target or recovery request is active. */
    CO_402_CONTROLLER_RESULT_IN_PROGRESS,       /**< Waiting for the remote PDS transition. */
    CO_402_CONTROLLER_RESULT_TARGET_REACHED,    /**< The requested PDS target is observed. */
    CO_402_CONTROLLER_RESULT_REMOTE_FAULT,      /**< The remote drive is in Fault. */
    CO_402_CONTROLLER_RESULT_WAIT_FAULT_REACTION, /**< Waiting for Fault reaction active to finish. */
    CO_402_CONTROLLER_RESULT_STATUS_UNKNOWN,    /**< Statusword cannot currently resolve a PDS state. */
    CO_402_CONTROLLER_RESULT_FEEDBACK_TIMEOUT,  /**< No fresh Statusword arrived within policy. */
    CO_402_CONTROLLER_RESULT_TRANSITION_TIMEOUT, /**< Current transition exceeded its policy timeout. */
    CO_402_CONTROLLER_RESULT_POLICY_BLOCKED     /**< Requested path requires policy this helper does not own. */
} CO_402_controller_result_t;

/** Caller-provided timeout policy. Values are expressed in microseconds. */
typedef struct {
    uint32_t transitionTimeout_us; /**< Per-observed-state transition timeout; zero disables it. */
    uint32_t feedbackTimeout_us;   /**< Maximum age of fresh Statusword feedback; zero disables it. */
} CO_402_controller_config_t;

/** One remote Statusword observation supplied by the user's transport/application layer. */
typedef struct {
    bool statuswordValid; /**< True only when statusword contains a fresh observation. */
    uint16_t statusword;  /**< Remote CiA 402 Statusword value. */
} CO_402_controller_feedback_t;

/**
 * @brief PDS-owned Controlword update.
 *
 * The caller merges this update into its complete Controlword so operation-mode
 * bits remain owned by the application or higher-level motion profile logic.
 */
typedef struct {
    uint16_t value; /**< PDS-owned bit values selected by mask. */
    uint16_t mask;  /**< Controlword bits owned by this update. */
    bool valid;     /**< True when the caller must apply this update. */
} CO_402_controller_controlword_update_t;

/** Caller-owned runtime for one remote CiA 402 axis. */
typedef struct {
    CO_402_controller_config_t config; /**< Caller policy copied during initialization. */

    CO_402_state_t remoteState;   /**< Last decoded remote PDS state. */
    uint16_t remoteStatusword;     /**< Last received raw Statusword. */
    bool remoteStateValid;         /**< True when remoteState came from a recognized Statusword. */
    bool remoteStatuswordInvalid;  /**< Latched until a later valid Statusword replaces an invalid one. */

    CO_402_controller_target_t target; /**< Current caller-requested target. */
    bool targetValid;                   /**< True while target is active. */

    bool faultResetRequested; /**< Explicit Fault Reset request waiting to be asserted. */
    bool faultResetPulseHigh; /**< Fault Reset high phase was emitted and requires a low update next. */

    uint32_t transitionElapsed_us; /**< Time spent waiting in the current observed transition state. */
    uint32_t feedbackElapsed_us;   /**< Time elapsed since the last fresh Statusword. */

    CO_402_state_t transitionFrom; /**< Last state that reset the transition timer. */
    CO_402_command_t pendingCommand; /**< PDS command selected by the most recent process step. */
    CO_402_controller_result_t result; /**< Current Controller result recorded by lifecycle, request, or process APIs. */
} CO_402_controller_axis_t;

/**
 * @brief Initialize one Controller axis.
 *
 * The Controller owns no transport object, Node-ID, RTOS object, or heap memory.
 * Timeout values are caller policy; zero disables the corresponding timeout.
 *
 * @param axis Caller-owned runtime object.
 * @param config Caller-owned timeout configuration copied into @p axis.
 * @return true on success, false when an argument is NULL.
 */
bool CO_402_controller_init(CO_402_controller_axis_t *axis,
                            const CO_402_controller_config_t *config);

/**
 * @brief Reset runtime observations while preserving timeout policy and pending Fault Reset deassertion.
 *
 * The configured timeout policy is preserved. If a Fault Reset high update was
 * already emitted, the pending explicit low update is also preserved because
 * the caller-owned Controlword may still contain bit 7 set.
 *
 * @param axis Controller runtime object. NULL is accepted and ignored.
 */
void CO_402_controller_reset(CO_402_controller_axis_t *axis);

/**
 * @brief Request one reachable PDS target.
 *
 * Repeating the same active target is idempotent and does not restart the
 * transition timeout. Changing the target starts a new transition interval.
 *
 * @param axis Controller runtime object.
 * @param target Requested target state.
 * @return true when @p target is valid and no Fault/Fault Reset recovery is active; otherwise false.
 */
bool CO_402_controller_setTarget(CO_402_controller_axis_t *axis,
                                 CO_402_controller_target_t target);

/**
 * @brief Clear the current target request without changing the observed remote state.
 *
 * @param axis Controller runtime object. NULL is accepted and ignored.
 */
void CO_402_controller_clearTarget(CO_402_controller_axis_t *axis);

/**
 * @brief Request one explicit Fault Reset pulse.
 *
 * The request is accepted only while the last valid remote state is Fault.
 * Fault Reset never restores a previously requested motion target automatically.
 *
 * @param axis Controller runtime object.
 * @return true when the reset request was accepted.
 */
bool CO_402_controller_requestFaultReset(CO_402_controller_axis_t *axis);

/**
 * @brief Process one Controller cycle.
 *
 * @param axis Controller runtime object.
 * @param feedback Remote Statusword observation for this cycle. Must not be NULL.
 * @param timeDifference_us Elapsed time since the previous call.
 * @param controlwordUpdate Output update for PDS-owned Controlword bits. Must not be NULL. When valid,
 * the caller must consume it independently of the returned result so an explicit
 * Fault Reset deassertion is not lost on an error or idle result. When this pointer
 * is non-NULL, the output is cleared before argument validation to prevent reuse of
 * a stale valid update on an invalid-input return.
 * @return Current Controller result. Invalid pointer arguments return
 * CO_402_CONTROLLER_RESULT_STATUS_UNKNOWN; when @p axis is non-NULL the result is recorded in it.
 */
CO_402_controller_result_t CO_402_controller_process(
    CO_402_controller_axis_t *axis,
    const CO_402_controller_feedback_t *feedback,
    uint32_t timeDifference_us,
    CO_402_controller_controlword_update_t *controlwordUpdate);

/**
 * @brief Return the current decoded remote state.
 *
 * @param axis Controller runtime object.
 * @return Decoded remote PDS state, or CO_402_STATE_UNKNOWN when no recognized latest observation exists.
 */
CO_402_state_t CO_402_controller_getRemoteState(const CO_402_controller_axis_t *axis);

/**
 * @brief Return the current Controller result.
 *
 * @param axis Controller runtime object.
 * @return Current recorded Controller result, or CO_402_CONTROLLER_RESULT_STATUS_UNKNOWN for NULL.
 */
CO_402_controller_result_t CO_402_controller_getResult(const CO_402_controller_axis_t *axis);

/**
 * @brief Merge one Controller PDS update into a complete Controlword.
 *
 * @param currentControlword Current caller-owned complete Controlword.
 * @param update PDS update returned by CO_402_controller_process().
 * @return Merged Controlword. If @p update is NULL or invalid, the input is returned unchanged.
 */
uint16_t CO_402_controller_applyControlwordUpdate(
    uint16_t currentControlword,
    const CO_402_controller_controlword_update_t *update);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_CONTROLLER_H */
