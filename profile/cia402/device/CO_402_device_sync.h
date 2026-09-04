/**
 * @file CO_402_device_sync.h
 * @brief Bounded multi-axis cyclic synchronous bridge for CiA 402 Device modes.
 */
#ifndef CO_402_DEVICE_SYNC_H
#define CO_402_DEVICE_SYNC_H

#include <stdbool.h>
#include <stdint.h>

#include "CO_402_mode.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** One coherent cyclic command generation published to motor control. */
typedef struct CO_402_sync_command {
    uint32_t sequence;          /**< Manager-owned modulo-2^32 SYNC generation shared by all axes in this cycle. */
    CO_402_mode_t mode;         /**< Active cyclic mode that owns the target field. */
    int32_t targetPosition;     /**< CSP target position; zero for other modes. */
    int32_t targetVelocity;     /**< CSV target velocity; zero for other modes. */
    int16_t targetTorque;       /**< CST target torque; zero for other modes. */
} CO_402_sync_command_t;

/** One coherent motor-feedback generation returned to the CANopen process image. */
typedef struct CO_402_sync_feedback {
    uint32_t sequence;          /**< Command generation that produced this feedback snapshot. */
    int32_t positionActual;     /**< Position actual value for TPDO/SDO publication. */
    int32_t velocityActual;     /**< Velocity actual value for TPDO/SDO publication. */
    int16_t torqueActual;       /**< Torque actual value for TPDO/SDO publication. */
    bool driveFollowsCommand;   /**< Product verdict for cyclic Statusword bit 12 in this generation. */
} CO_402_sync_feedback_t;

/**
 * @brief Product-owned bounded handoff between co_rt and the motor-control loop.
 *
 * Both callbacks execute in the synchronous realtime path while the CANopen
 * lifecycle mutex and OD lock are already held. They must not sleep, allocate,
 * log, or acquire an unbounded lock. Motor control must consume/produce whole
 * snapshots so sequence checks can reject stale or mixed generations. Endpoints must
 * not retain an unmatched feedback snapshot across a complete uint32_t sequence wrap.
 * `driveFollowsCommand` is a product-level verdict for Statusword bit 12; sequence
 * freshness alone does not prove that the physical drive follows the command. A false
 * publish result leaves motor-command ownership with the product and suppresses
 * feedback readback for that axis in this generation. Missing/stale feedback
 * leaves the previous OD actual values unchanged; neither case invents a PDS
 * fault transition in the realtime path.
 */
typedef struct CO_402_device_sync_if {
    uint32_t supportedModes; /**< CO_402_SUPPORTED_MODE_CSP/CSV/CST bits implemented by this endpoint. */
    bool (*publishCommand)(void *object, const CO_402_sync_command_t *command); /**< Publish one atomic command snapshot. */
    bool (*readFeedback)(void *object, CO_402_sync_feedback_t *feedback); /**< Read one atomic feedback snapshot. */
} CO_402_device_sync_if_t;

/** Fast-path-owned per-axis observability and generation state. */
typedef struct {
    CO_402_sync_command_t command;   /**< Last command snapshot attempted for this axis. */
    CO_402_sync_feedback_t feedback; /**< Last feedback snapshot returned by SyncIF. */
    bool active;                     /**< Axis passed PDS/mode/SyncIF eligibility for this SYNC. */
    bool commandPublished;           /**< SyncIF accepted this generation's command. */
    bool feedbackFresh;              /**< Feedback sequence matched the current manager generation. */
} CO_402_device_sync_runtime_t;

/**
 * @brief Encode cyclic Statusword bit 12 from one completed SyncIF generation.
 *
 * Generation freshness proves only coherence. The product-owned
 * `driveFollowsCommand` verdict decides whether bit 12 is asserted.
 *
 * @param runtime Per-axis synchronous runtime snapshot.
 * @return CO_402_STATUSWORD_DRIVE_FOLLOWS_COMMAND when the accepted, current
 * feedback generation reports drive-following; otherwise zero.
 */
static inline uint16_t CO_402_device_syncStatuswordBits(const CO_402_device_sync_runtime_t *runtime)
{
    return runtime != NULL && runtime->active && runtime->commandPublished && runtime->feedbackFresh
                   && runtime->feedback.driveFollowsCommand
               ? CO_402_STATUSWORD_DRIVE_FOLLOWS_COMMAND
               : 0U;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* CO_402_DEVICE_SYNC_H */
