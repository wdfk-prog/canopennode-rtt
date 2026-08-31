/**
 * @file CO_demo_nmt_master.h
 * @brief Heartbeat-supervised automatic NMT master validation demo.
 */

#ifndef CO_DEMO_NMT_MASTER_H_
#define CO_DEMO_NMT_MASTER_H_

#include "CANopen.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Runtime state for the automatic NMT master validation sequence. */
typedef struct {
    uint8_t step; /**< Index of the current NMT validation step. */
    int8_t hbConsumerIdx; /**< Heartbeat-consumer index assigned to the remote node. */
    uint8_t phase; /**< Internal non-blocking validation phase. */
    uint32_t referenceTimeMs; /**< Timestamp used for state-transition timeout measurement. */
    uint32_t hbConsumerOriginal; /**< OD 0x1016 value replaced while the demo test is active. */
    uint32_t hbConsumerApplied; /**< OD 0x1016 value owned by the demo while validation is active. */
    bool_t hbConfigured; /**< true after OD 0x1016 is configured for the remote node. */
    bool_t hbOverrideActive; /**< true while the demo-owned 0x1016 override must be restored. */
    bool_t resetObserved; /**< true after a reset command produces a remote boot-up transition. */
    bool_t finished; /**< true after the complete command/state sequence passes. */
    bool_t failed; /**< true after configuration, timeout, or CAN transmit failure. */
} CO_demo_nmt_master_t;

/**
 * @brief Initialize NMT master validation runtime state.
 *
 * @param demo NMT master validation state.
 */
void CO_demo_nmt_master_init(CO_demo_nmt_master_t *demo);

/**
 * @brief Bind heartbeat supervision to a newly initialized CANopenNode stack.
 *
 * One demo OD 0x1016 entry is configured for the target node before CAN normal
 * mode is enabled. An existing entry for the same node is reused; otherwise the
 * first unconfigured consumer entry is used. The previous OD 0x1016 value is
 * saved and restored when the test finishes, fails, or the local node resets.
 *
 * @param demo NMT master validation state.
 * @param co Newly initialized CANopenNode object.
 */
void CO_demo_nmt_master_bind(CO_demo_nmt_master_t *demo, CO_t *co);

/**
 * @brief Process one non-blocking NMT master validation iteration.
 *
 * The sequence starts only after the local node is Operational. If the remote
 * peer is not already PRE-OP, the demo first sends a fixture-preparation PREOP
 * command and waits for a PRE-OP heartbeat. The same normalization is repeated
 * after remote resets when the software peer automatically starts Operational.
 * Normal validation commands still require a different pre-command state.
 *
 * @param demo NMT master validation state.
 * @param co Current CANopenNode object.
 * @param localNodeId Active local CANopen Node-ID.
 * @param nowMs Current monotonic RT-Thread time in milliseconds.
 * @param resetStatus Reset request returned by the preceding CO_process() call.
 */
void CO_demo_nmt_master_process(CO_demo_nmt_master_t *demo, CO_t *co, uint8_t localNodeId,
                                uint32_t nowMs, CO_NMT_reset_cmd_t resetStatus);

#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
/**
 * @brief Merge immediate NMT test work or the active state timeout into mainline timerNext.
 *
 * SEND_COMMAND contributes a zero deadline so a state transition that schedules
 * the next NMT command does not depend on an unrelated heartbeat/event wakeup.
 *
 * @param demo NMT master validation state.
 * @param nowMs Current monotonic RT-Thread time in milliseconds.
 * @param timerNextUs Optional mainline deadline accumulator.
 */
void CO_demo_nmt_master_update_timer_next(const CO_demo_nmt_master_t *demo, uint32_t nowMs,
                                          uint32_t *timerNextUs);
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */

/**
 * @brief Reset the NMT master validation sequence before local stack recreation.
 *
 * Any still-owned Heartbeat Consumer override gets one final restore attempt
 * before the runtime state is cleared.
 *
 * @param demo NMT master validation state.
 */
void CO_demo_nmt_master_reset(CO_demo_nmt_master_t *demo);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_DEMO_NMT_MASTER_H_ */
