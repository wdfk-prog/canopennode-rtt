/**
 * @file CO_402_device_sync.c
 * @brief Bounded all-axis CSP/CSV/CST bridge executed inside co_rt.
 */
#include <string.h>

#include "CO_402_device_internal.h"
#include "CO_402_device_sync.h"
#if CO_402_CONFIG_MODE_CSP
#include "modes/CO_402_mode_csp.h"
#endif /* CO_402_CONFIG_MODE_CSP */
#if CO_402_CONFIG_MODE_CSV
#include "modes/CO_402_mode_csv.h"
#endif /* CO_402_CONFIG_MODE_CSV */
#if CO_402_CONFIG_MODE_CST
#include "modes/CO_402_mode_cst.h"
#endif /* CO_402_CONFIG_MODE_CST */

/** Snapshot exactly one target field from the RPDO-updated OD process image. */
static bool snapshotCommand(CO_402_device_axis_t *axis, uint32_t sequence)
{
    CO_402_sync_command_t *command = &axis->syncRuntime.command;

    memset(command, 0, sizeof(*command));
    command->sequence = sequence;
    command->mode = axis->mode;

    switch (axis->mode) {
#if CO_402_CONFIG_MODE_CSP
        case CO_402_MODE_CYCLIC_SYNC_POSITION:
            return CO_402_mode_csp_snapshot(axis, command);
#endif /* CO_402_CONFIG_MODE_CSP */
#if CO_402_CONFIG_MODE_CSV
        case CO_402_MODE_CYCLIC_SYNC_VELOCITY:
            return CO_402_mode_csv_snapshot(axis, command);
#endif /* CO_402_CONFIG_MODE_CSV */
#if CO_402_CONFIG_MODE_CST
        case CO_402_MODE_CYCLIC_SYNC_TORQUE:
            return CO_402_mode_cst_snapshot(axis, command);
#endif /* CO_402_CONFIG_MODE_CST */
        case CO_402_MODE_NONE:
        default:
            return false;
    }
}

/** Return true when the active semantic mode owns cyclic Statusword bit 12. */
static bool isCyclicMode(CO_402_mode_t mode)
{
    switch (mode) {
#if CO_402_CONFIG_MODE_CSP
        case CO_402_MODE_CYCLIC_SYNC_POSITION:
            return true;
#endif /* CO_402_CONFIG_MODE_CSP */
#if CO_402_CONFIG_MODE_CSV
        case CO_402_MODE_CYCLIC_SYNC_VELOCITY:
            return true;
#endif /* CO_402_CONFIG_MODE_CSV */
#if CO_402_CONFIG_MODE_CST
        case CO_402_MODE_CYCLIC_SYNC_TORQUE:
            return true;
#endif /* CO_402_CONFIG_MODE_CST */
        case CO_402_MODE_NONE:
        default:
            return false;
    }
}

/** Publish one TPDO-visible cyclic image; stale/missing feedback clears only the cyclic status bit. */
static bool publishSyncImage(CO_402_device_axis_t *axis, bool publishActuals)
{
    CO_402_device_sync_runtime_t *runtime = &axis->syncRuntime;
    const CO_402_sync_feedback_t *feedback = &runtime->feedback;
    void *statuswordPtr;
    void *positionActual = NULL;
    void *velocityActual = NULL;
#if CO_402_CONFIG_MODE_CST
    void *torqueActual = NULL;
#endif /* CO_402_CONFIG_MODE_CST */
    uint16_t statusword;
    ODR_t odRet;

    statuswordPtr = OD_getPtr(axis->od.statusword, 0U, sizeof(statusword), &odRet);
    if (statuswordPtr == NULL || odRet != ODR_OK) {
        return false;
    }
    memcpy(&statusword, statuswordPtr, sizeof(statusword));

    if (publishActuals) {
        /*
         * Preflight every original-storage pointer before mutating any field. The caller
         * already holds the OD lock, so the following memcpy group becomes one TPDO-visible
         * commit and cannot leave a mixed generation when an OD contract is invalid.
         */
        positionActual = OD_getPtr(axis->od.positionActualValue, 0U, sizeof(feedback->positionActual), &odRet);
        if (positionActual == NULL || odRet != ODR_OK) {
            return false;
        }
        velocityActual = OD_getPtr(axis->od.velocityActualValue, 0U, sizeof(feedback->velocityActual), &odRet);
        if (velocityActual == NULL || odRet != ODR_OK) {
            return false;
        }
#if CO_402_CONFIG_MODE_CST
        if (axis->mode == CO_402_MODE_CYCLIC_SYNC_TORQUE) {
            torqueActual = OD_getPtr(axis->od.torqueActualValue, 0U, sizeof(feedback->torqueActual), &odRet);
            if (torqueActual == NULL || odRet != ODR_OK) {
                return false;
            }
        }
#endif /* CO_402_CONFIG_MODE_CST */
    }

    statusword = (uint16_t)(statusword & (uint16_t)~CO_402_STATUSWORD_DRIVE_FOLLOWS_COMMAND);
    statusword = (uint16_t)(statusword | CO_402_device_syncStatuswordBits(runtime));

    if (publishActuals) {
        memcpy(positionActual, &feedback->positionActual, sizeof(feedback->positionActual));
        memcpy(velocityActual, &feedback->velocityActual, sizeof(feedback->velocityActual));
#if CO_402_CONFIG_MODE_CST
        if (torqueActual != NULL) {
            memcpy(torqueActual, &feedback->torqueActual, sizeof(feedback->torqueActual));
        }
#endif /* CO_402_CONFIG_MODE_CST */
    }
    memcpy(statuswordPtr, &statusword, sizeof(statusword));
    return true;
}

void CO_402_device_processSyncLocked(CO_402_device_manager_t *manager, uint32_t dtUs)
{
    uint8_t axisIndex;

    (void)dtUs;
    if (manager == NULL || !manager->odBound) {
        return;
    }

    /* One manager generation is allocated before any axis command is published. */
    manager->syncSequence++;

    /* Phase 1 snapshots every eligible axis before motor control can observe this generation. */
    for (axisIndex = 0U; axisIndex < manager->axisCount; axisIndex++) {
        CO_402_device_axis_t *axis = &manager->axes[axisIndex];
        CO_402_device_sync_runtime_t *runtime = &axis->syncRuntime;

        runtime->active = false;
        runtime->commandPublished = false;
        runtime->feedbackFresh = false;
        /* PDS and mode-transition ownership suppresses cyclic commands until the supervisor is stable again. */
        if (!CO_402_device_syncFastPathEligible(axis)) {
            continue;
        }

        runtime->active = snapshotCommand(axis, manager->syncSequence);
    }

    /* Phase 2 publishes all commands with the same sequence; one rejected axis does not block peers. */
    for (axisIndex = 0U; axisIndex < manager->axisCount; axisIndex++) {
        CO_402_device_axis_t *axis = &manager->axes[axisIndex];
        CO_402_device_sync_runtime_t *runtime = &axis->syncRuntime;

        if (runtime->active) {
            runtime->commandPublished = axis->sync->publishCommand(axis->syncObject, &runtime->command);
        }
    }

    /* Phase 3 obtains complete feedback snapshots only for commands accepted in this generation. */
    for (axisIndex = 0U; axisIndex < manager->axisCount; axisIndex++) {
        CO_402_device_axis_t *axis = &manager->axes[axisIndex];
        CO_402_device_sync_runtime_t *runtime = &axis->syncRuntime;
        CO_402_sync_feedback_t feedback;

        if (!runtime->active || !runtime->commandPublished) {
            continue;
        }
        memset(&feedback, 0, sizeof(feedback));
        if (!axis->sync->readFeedback(axis->syncObject, &feedback)) {
            continue;
        }

        runtime->feedback = feedback;
        runtime->feedbackFresh = feedback.sequence == manager->syncSequence;
    }

    /* Phase 4 updates actual values and cyclic Statusword bit 12 before TPDO observes this generation. */
    for (axisIndex = 0U; axisIndex < manager->axisCount; axisIndex++) {
        CO_402_device_axis_t *axis = &manager->axes[axisIndex];
        CO_402_device_sync_runtime_t *runtime = &axis->syncRuntime;
        bool publishActuals;

        if (!isCyclicMode(axis->mode)) {
            continue;
        }

        publishActuals = runtime->active && runtime->feedbackFresh;
        if (!publishSyncImage(axis, publishActuals) && publishActuals) {
            runtime->feedbackFresh = false;
            /* Preflight failure leaves actuals untouched; clear any previous drive-follows indication if possible. */
            (void)publishSyncImage(axis, false);
        }
    }
}

void CO_402_device_onCommunicationReset(CO_402_device_manager_t *manager)
{
    uint8_t axisIndex;

    if (manager == NULL || manager->axes == NULL) {
        return;
    }

    /* Preserve syncSequence as an anti-stale barrier across CANopen communication generations. */
    for (axisIndex = 0U; axisIndex < manager->axisCount; axisIndex++) {
        memset(&manager->axes[axisIndex].syncRuntime, 0, sizeof(manager->axes[axisIndex].syncRuntime));
    }
}
