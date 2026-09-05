/**
 * @file CO_402_device.c
 * @brief Pure-C multi-axis CiA 402 Device manager.
 */

#include <string.h>

#include "CO_402_device.h"
#include "CO_402_device_fsa.h"
#include "CO_402_device_internal.h"

/*
 * Populate a manager-level initialization diagnostic.
 *
 * Object-specific callers overwrite index/sub-index later; configuration
 * failures deliberately report an index of zero because no OD object failed.
 */
static void setDiag(CO_402_init_diag_t *diag, CO_402_init_error_t error, uint8_t logicalDevice)
{
    if (diag == NULL) {
        return;
    }

    diag->error = error;
    diag->logicalDevice = logicalDevice;
    diag->index = 0U;
    diag->subIndex = 0U;
}

/*
 * Check the mandatory asynchronous PDS transition callbacks.
 *
 * Feedback and operation-mode callbacks remain optional. supportedModesForAxis()
 * advertises only modes whose complete non-blocking DriveIF/SyncIF path is available.
 */
static bool driveInterfaceValid(const CO_402_drive_if_t *drive)
{
    return drive != NULL && drive->shutdown != NULL && drive->switchOn != NULL && drive->enableOperation != NULL
           && drive->disableVoltage != NULL && drive->disableOperation != NULL && drive->quickStop != NULL
           && drive->faultReaction != NULL && drive->faultReset != NULL;
}

/* Advertise a mode only when both the build and this axis' complete callback path support it. */
static uint32_t supportedModesForAxis(const CO_402_device_axis_config_t *config)
{
    const CO_402_drive_if_t *drive;
    uint32_t supportedModes = 0U;

    if (config == NULL) {
        return 0U;
    }
    drive = config->drive;

    /* Safe mode switching requires explicit non-blocking entry and exit ownership. */
    if (drive == NULL || drive->modeEnter == NULL || drive->modeExit == NULL) {
        return 0U;
    }

#if CO_402_CONFIG_MODE_PP
    if (drive->profilePosition != NULL) {
        supportedModes |= CO_402_SUPPORTED_MODE_PP;
    }
#endif /* CO_402_CONFIG_MODE_PP */
#if CO_402_CONFIG_MODE_PV
    if (drive->profileVelocity != NULL) {
        supportedModes |= CO_402_SUPPORTED_MODE_PV;
    }
#endif /* CO_402_CONFIG_MODE_PV */
#if CO_402_CONFIG_MODE_HM
    if (drive->homing != NULL) {
        supportedModes |= CO_402_SUPPORTED_MODE_HM;
    }
#endif /* CO_402_CONFIG_MODE_HM */

#if CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST
    /* Cyclic modes additionally require a complete bounded command/feedback handoff. */
    if (config->sync != NULL && config->sync->publishCommand != NULL && config->sync->readFeedback != NULL) {
#if CO_402_CONFIG_MODE_CSP
        supportedModes |= config->sync->supportedModes & CO_402_SUPPORTED_MODE_CSP;
#endif /* CO_402_CONFIG_MODE_CSP */
#if CO_402_CONFIG_MODE_CSV
        supportedModes |= config->sync->supportedModes & CO_402_SUPPORTED_MODE_CSV;
#endif /* CO_402_CONFIG_MODE_CSV */
#if CO_402_CONFIG_MODE_CST
        supportedModes |= config->sync->supportedModes & CO_402_SUPPORTED_MODE_CST;
#endif /* CO_402_CONFIG_MODE_CST */
    }
#endif /* CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST */

    return supportedModes;
}

/*
 * Validate logical-device configuration before manager storage is published.
 */
static CO_402_init_error_t validateConfigs(const CO_402_device_axis_config_t *configs, uint8_t axisCount,
                                           CO_402_init_diag_t *diag)
{
    uint8_t axisIndex;
    uint8_t compareIndex;

    if (configs == NULL || axisCount == 0U || axisCount > CO_402_LOGICAL_DEVICE_COUNT_MAX) {
        setDiag(diag, CO_402_INIT_CONFIG_MISMATCH, 0U);
        return CO_402_INIT_CONFIG_MISMATCH;
    }

    for (axisIndex = 0U; axisIndex < axisCount; axisIndex++) {
        /* Every axis must reference one valid logical-device block and a complete PDS DriveIF. */
        if (configs[axisIndex].logicalDevice >= CO_402_LOGICAL_DEVICE_COUNT_MAX
            || !driveInterfaceValid(configs[axisIndex].drive)) {
            setDiag(diag, CO_402_INIT_BAD_AXIS, configs[axisIndex].logicalDevice);
            return CO_402_INIT_BAD_AXIS;
        }
#if CO_402_CONFIG_DIAGNOSTICS
        if (!CO_402_device_diag_ifValid(configs[axisIndex].diag)) {
            setDiag(diag, CO_402_INIT_DIAG_IF, configs[axisIndex].logicalDevice);
            return CO_402_INIT_DIAG_IF;
        }
#endif /* CO_402_CONFIG_DIAGNOSTICS */

        /* Duplicate logical-device blocks would make two axes own the same OD state. */
        for (compareIndex = (uint8_t)(axisIndex + 1U); compareIndex < axisCount; compareIndex++) {
            if (configs[axisIndex].logicalDevice == configs[compareIndex].logicalDevice) {
                setDiag(diag, CO_402_INIT_DUPLICATE_AXIS, configs[axisIndex].logicalDevice);
                return CO_402_INIT_DUPLICATE_AXIS;
            }
        }
    }

    return CO_402_INIT_OK;
}

/* Reset mode-local edge/command state without disturbing PDS state or another axis. */
static void resetModeRuntime(CO_402_device_axis_t *axis, CO_402_mode_t mode, uint16_t controlword)
{
    (void)controlword;
    if (axis == NULL) {
        return;
    }

    switch (mode) {
#if CO_402_CONFIG_MODE_PP
        case CO_402_MODE_PROFILE_POSITION:
            CO_402_mode_pp_reset(&axis->pp, controlword);
            break;
#endif /* CO_402_CONFIG_MODE_PP */
#if CO_402_CONFIG_MODE_PV
        case CO_402_MODE_PROFILE_VELOCITY:
            CO_402_mode_pv_reset(&axis->pv);
            break;
#endif /* CO_402_CONFIG_MODE_PV */
#if CO_402_CONFIG_MODE_HM
        case CO_402_MODE_HOMING:
            CO_402_mode_hm_reset(&axis->hm, controlword);
            break;
#endif /* CO_402_CONFIG_MODE_HM */
        case CO_402_MODE_NONE:
        default:
            break;
    }
}

/* Capability gating rejects recognized modes that this axis did not advertise in 0x6502. */
static bool modeSupported(const CO_402_device_axis_t *axis, CO_402_mode_t mode)
{
    const uint32_t bit = CO_402_modeCapabilityBit(mode);

    return mode == CO_402_MODE_NONE || (bit != 0U && (axis->supportedModes & bit) != 0U);
}

/* Fault-reaction and Quick-stop own the drive; mode entry/exit cannot run concurrently with them. */
static bool modeTransitionAllowed(CO_402_state_t state)
{
    return state == CO_402_STATE_SWITCH_ON_DISABLED || state == CO_402_STATE_READY_TO_SWITCH_ON
           || state == CO_402_STATE_SWITCHED_ON || state == CO_402_STATE_OPERATION_ENABLED;
}

/* A BUSY exit blocks ordinary 0x6060 writes until completion; a safety transfer may retire it. */
static bool continueModeExit(CO_402_device_axis_t *axis, uint16_t controlword)
{
    CO_402_drive_result_t result;

    if (axis->pendingExitMode == CO_402_MODE_NONE || !modeTransitionAllowed(axis->state)) {
        return true;
    }

    result = axis->drive->modeExit(axis->driveObject, axis->pendingExitMode);
    if (result == CO_402_DRIVE_ERROR) {
        axis->pendingExitMode = CO_402_MODE_NONE;
        return false;
    }
    if (result == CO_402_DRIVE_BUSY) {
        return true;
    }

    if (axis->mode == axis->pendingExitMode) {
        resetModeRuntime(axis, axis->mode, controlword);
        axis->mode = CO_402_MODE_NONE;
    }
    axis->pendingExitMode = CO_402_MODE_NONE;
    return true;
}

/* A BUSY entry blocks ordinary mode requests until completion; a safety transfer may retire it. */
static bool continueModeEnter(CO_402_device_axis_t *axis, uint16_t controlword)
{
    CO_402_drive_result_t result;
    CO_402_mode_t enteringMode;

    if (axis->pendingEnterMode == CO_402_MODE_NONE || !modeTransitionAllowed(axis->state)) {
        return true;
    }

    enteringMode = axis->pendingEnterMode;
    result = axis->drive->modeEnter(axis->driveObject, enteringMode);
    if (result == CO_402_DRIVE_ERROR) {
        axis->pendingEnterMode = CO_402_MODE_NONE;
        return false;
    }
    if (result == CO_402_DRIVE_BUSY) {
        return true;
    }

    axis->mode = enteringMode;
    axis->pendingEnterMode = CO_402_MODE_NONE;
    resetModeRuntime(axis, enteringMode, controlword);
    return true;
}

/* Serialize exit-before-enter and defer ordinary requests while either callback owns the transition. */
static bool processModeSelection(CO_402_device_axis_t *axis, uint16_t controlword, bool *runActiveMode)
{
    CO_402_mode_t requestedMode;

    if (runActiveMode == NULL) {
        return false;
    }
    *runActiveMode = true;

    /* A BUSY mode owner queues later 0x6060 writes unless the FSA has already transferred ownership to safety. */
    if (axis->pendingExitMode != CO_402_MODE_NONE) {
        *runActiveMode = false;
        return continueModeExit(axis, controlword);
    }
    if (axis->pendingEnterMode != CO_402_MODE_NONE) {
        *runActiveMode = false;
        return continueModeEnter(axis, controlword);
    }

    if (!axis->requestedModeRecognized || !CO_402_modeFromRaw(axis->requestedModeRaw, &requestedMode)
        || !modeSupported(axis, requestedMode) || !modeTransitionAllowed(axis->state)) {
        return true;
    }
    if (axis->mode == requestedMode) {
        return true;
    }

    /* Never issue a mode command in a cycle already owned by mode entry/exit. */
    *runActiveMode = false;
    if (axis->mode != CO_402_MODE_NONE) {
        axis->pendingExitMode = axis->mode;
        return continueModeExit(axis, controlword);
    }

    if (requestedMode != CO_402_MODE_NONE) {
        axis->pendingEnterMode = requestedMode;
        return continueModeEnter(axis, controlword);
    }

    return true;
}

/* Mode commands are executable only while PDS owns the axis in Operation enabled. */
static bool processActiveMode(CO_402_device_axis_t *axis, uint16_t controlword)
{
    if (axis->mode == CO_402_MODE_NONE) {
        return true;
    }
    if (axis->state != CO_402_STATE_OPERATION_ENABLED) {
        /* PDS callbacks own physical stop/fault/quick-stop actions; only protocol transient state is reset here. */
        resetModeRuntime(axis, axis->mode, controlword);
        return true;
    }

    switch (axis->mode) {
#if CO_402_CONFIG_MODE_PP
        case CO_402_MODE_PROFILE_POSITION:
            return CO_402_mode_pp_process(axis, controlword);
#endif /* CO_402_CONFIG_MODE_PP */
#if CO_402_CONFIG_MODE_PV
        case CO_402_MODE_PROFILE_VELOCITY:
            return CO_402_mode_pv_process(axis, controlword);
#endif /* CO_402_CONFIG_MODE_PV */
#if CO_402_CONFIG_MODE_HM
        case CO_402_MODE_HOMING:
            return CO_402_mode_hm_process(axis, controlword);
#endif /* CO_402_CONFIG_MODE_HM */
#if CO_402_CONFIG_MODE_CSP
        case CO_402_MODE_CYCLIC_SYNC_POSITION:
#endif /* CO_402_CONFIG_MODE_CSP */
#if CO_402_CONFIG_MODE_CSV
        case CO_402_MODE_CYCLIC_SYNC_VELOCITY:
#endif /* CO_402_CONFIG_MODE_CSV */
#if CO_402_CONFIG_MODE_CST
        case CO_402_MODE_CYCLIC_SYNC_TORQUE:
#endif /* CO_402_CONFIG_MODE_CST */
#if CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST
            /* co_rt owns cyclic target/feedback execution; the supervisor only owns PDS/mode transitions. */
            return true;
#endif /* CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST */
        case CO_402_MODE_NONE:
        default:
            return false;
    }
}

/** Encode mode-specific Statusword bits without modifying the PDS state bits. */
static uint16_t modeStatusword(const CO_402_device_axis_t *axis)
{
    switch (axis->mode) {
#if CO_402_CONFIG_MODE_PP
        case CO_402_MODE_PROFILE_POSITION:
            return CO_402_mode_pp_statusword(&axis->pp);
#endif /* CO_402_CONFIG_MODE_PP */
#if CO_402_CONFIG_MODE_PV
        case CO_402_MODE_PROFILE_VELOCITY:
            return CO_402_mode_pv_statusword(&axis->pv);
#endif /* CO_402_CONFIG_MODE_PV */
#if CO_402_CONFIG_MODE_HM
        case CO_402_MODE_HOMING:
            return CO_402_mode_hm_statusword(&axis->hm);
#endif /* CO_402_CONFIG_MODE_HM */
#if CO_402_CONFIG_MODE_CSP
        case CO_402_MODE_CYCLIC_SYNC_POSITION:
#endif /* CO_402_CONFIG_MODE_CSP */
#if CO_402_CONFIG_MODE_CSV
        case CO_402_MODE_CYCLIC_SYNC_VELOCITY:
#endif /* CO_402_CONFIG_MODE_CSV */
#if CO_402_CONFIG_MODE_CST
        case CO_402_MODE_CYCLIC_SYNC_TORQUE:
#endif /* CO_402_CONFIG_MODE_CST */
#if CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST
            return CO_402_device_syncFastPathEligible(axis)
                       ? CO_402_device_syncStatuswordBits(&axis->syncRuntime)
                       : 0U;
#endif /* CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST */
        case CO_402_MODE_NONE:
        default:
            return 0U;
    }
}

CO_402_init_error_t CO_402_device_managerInit(CO_402_device_manager_t *manager, OD_t *od,
                                               CO_402_device_axis_t *axes,
                                               const CO_402_device_axis_config_t *configs,
                                               uint8_t axisCount, CO_402_init_diag_t *diag)
{
    CO_402_init_error_t result;
    uint8_t axisIndex;

    /* Clear caller-visible diagnostics before validating any input. */
    if (diag != NULL) {
        memset(diag, 0, sizeof(*diag));
    }
    if (manager == NULL || od == NULL || axes == NULL) {
        setDiag(diag, CO_402_INIT_CONFIG_MISMATCH, 0U);
        return CO_402_INIT_CONFIG_MISMATCH;
    }

    /* Validate the external configuration before the manager retains its persistent pointers. */
    result = validateConfigs(configs, axisCount, diag);
    if (result != CO_402_INIT_OK) {
        return result;
    }

    /* The application owns all storage; initialization only clears and links the supplied objects. */
    memset(manager, 0, sizeof(*manager));
    memset(axes, 0, sizeof(*axes) * axisCount);
    manager->od = od;
    manager->axes = axes;
    manager->configs = configs;
    manager->axisCount = axisCount;

    /* Build one independent runtime record for each configured logical device. */
    for (axisIndex = 0U; axisIndex < axisCount; axisIndex++) {
        CO_402_device_axis_t *axis = &axes[axisIndex];

        axis->logicalDevice = configs[axisIndex].logicalDevice;
        axis->odBase = CO_402_objectIndex(axis->logicalDevice, CO_402_PROFILE_INDEX_BASE);
        axis->state = CO_402_STATE_NOT_READY_TO_SWITCH_ON;
        axis->mode = CO_402_MODE_NONE;
        axis->requestedModeRaw = (CO_402_mode_raw_t)CO_402_MODE_NONE;
        axis->requestedModeRecognized = true;
        axis->drive = configs[axisIndex].drive;
        axis->driveObject = configs[axisIndex].driveObject;
        axis->faultResetBitPrevious = false;
        axis->faultResetInProgress = false;
        axis->pdsTransitionOperation = NULL;
        axis->pdsTransitionTargetState = CO_402_STATE_UNKNOWN;
        axis->pendingExitMode = CO_402_MODE_NONE;
        axis->pendingEnterMode = CO_402_MODE_NONE;
#if CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST
        axis->sync = configs[axisIndex].sync;
        axis->syncObject = configs[axisIndex].syncObject;
#endif /* CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST */
#if CO_402_CONFIG_DIAGNOSTICS
        axis->diag = configs[axisIndex].diag;
        axis->diagObject = configs[axisIndex].diagObject;
#endif /* CO_402_CONFIG_DIAGNOSTICS */
        axis->supportedModes = supportedModesForAxis(&configs[axisIndex]);
    }

    /* OD binding is performed only after every axis runtime record is valid. */
    result = CO_402_device_bindOD(manager, diag);
    if (result != CO_402_INIT_OK) {
        return result;
    }

    /* Publish a PDS-consistent initial OD image instead of inheriting generator defaults. */
    for (axisIndex = 0U; axisIndex < axisCount; axisIndex++) {
        (void)OD_set_u16(axes[axisIndex].od.statusword, 0U,
                         CO_402_statuswordForState(CO_402_STATE_NOT_READY_TO_SWITCH_ON), true);
        (void)OD_set_i8(axes[axisIndex].od.modesOfOperationDisplay, 0U, (int8_t)CO_402_MODE_NONE, true);
    }

    return CO_402_INIT_OK;
}

/*
 * Snapshot the requested operation mode without directly changing the active mode.
 *
 * The raw INTEGER8 request stays observable even when unsupported; processModeSelection()
 * owns the serialized acceptance/entry/exit transition.
 */
static void updateModeRequest(CO_402_device_axis_t *axis)
{
    CO_402_mode_raw_t raw = 0;
    CO_402_mode_t mode;

    /* Keep the raw INTEGER8 value so unsupported values remain observable to later policy code. */
    if (OD_get_i8(axis->od.modesOfOperation, 0U, &raw, true) != ODR_OK) {
        axis->requestedModeRecognized = false;
        return;
    }

    axis->requestedModeRaw = raw;
    axis->requestedModeRecognized = CO_402_modeFromRaw(raw, &mode);
}

/*
 * Copy optional slow DriveIF feedback whenever co_rt does not own publication.
 *
 * Stable cyclic operation remains generation-coherent through SyncIF. During a
 * BUSY PDS or mode transition, cyclic commands are suppressed, so the lower-
 * priority supervisor may refresh actual values from the existing DriveIF path.
 */
static void updateFeedback(CO_402_device_axis_t *axis)
{
#if CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST
    if (CO_402_device_syncFastPathEligible(axis)) {
        return;
    }
#endif /* CO_402_CONFIG_MODE_CSP || CO_402_CONFIG_MODE_CSV || CO_402_CONFIG_MODE_CST */

    /* Missing optional feedback callbacks leave the corresponding generated OD value unchanged. */
    if (axis->drive->getPosition != NULL) {
        (void)OD_set_i32(axis->od.positionActualValue, 0U, axis->drive->getPosition(axis->driveObject), true);
    }
    if (axis->drive->getVelocity != NULL) {
        (void)OD_set_i32(axis->od.velocityActualValue, 0U, axis->drive->getVelocity(axis->driveObject), true);
    }
#if CO_402_CONFIG_MODE_CST
    if (axis->mode == CO_402_MODE_CYCLIC_SYNC_TORQUE && axis->od.torqueActualValue != NULL
        && axis->drive->getTorque != NULL) {
        (void)OD_set_i16(axis->od.torqueActualValue, 0U, axis->drive->getTorque(axis->driveObject), true);
    }
#endif /* CO_402_CONFIG_MODE_CST */
}

void CO_402_device_process(CO_402_device_manager_t *manager)
{
    uint8_t axisIndex;

    /* A partially initialized manager must not touch cached OD pointers or DriveIF callbacks. */
    if (manager == NULL || !manager->odBound) {
        return;
    }

    /* Each axis is processed independently so one PDS/mode runtime cannot overwrite another axis. */
    for (axisIndex = 0U; axisIndex < manager->axisCount; axisIndex++) {
        CO_402_device_axis_t *axis = &manager->axes[axisIndex];
        uint16_t controlword = 0U;
        uint16_t statusword;
        bool runActiveMode = true;
#if CO_402_CONFIG_DIAGNOSTICS
        CO_402_device_fault_info_t productFault;

        if (CO_402_device_diag_pollProductFault(axis, &productFault)) {
            CO_402_device_axisEnterFaultReaction(axis, CO_402_FAULT_ORIGIN_PRODUCT, &productFault);
        }
#endif /* CO_402_CONFIG_DIAGNOSTICS */

        updateModeRequest(axis);

        /* Snapshot the same generated Controlword used by the mode handshake during this supervisor pass. */
        if (OD_get_u16(axis->od.controlword, 0U, &controlword, true) != ODR_OK) {
            /* Fault ownership transfer retires any BUSY mode/PDS owner; no mode callback runs in this pass. */
            CO_402_device_axisControlwordReadFailed(axis);
            runActiveMode = false;
        } else {
            CO_402_device_axisProcess(axis);
            if (axis->pdsTransitionOperation != NULL) {
                /* A BUSY PDS transition exclusively owns the drive; mode callbacks must not counteract it. */
                runActiveMode = false;
                if (axis->mode != CO_402_MODE_NONE) {
                    resetModeRuntime(axis, axis->mode, controlword);
                }
            } else if (!processModeSelection(axis, controlword, &runActiveMode)
                       || (runActiveMode && !processActiveMode(axis, controlword))) {
                CO_402_device_axisEnterFaultReaction(axis, CO_402_FAULT_ORIGIN_MODE_OPERATION, NULL);
            } else if (!runActiveMode && axis->state != CO_402_STATE_OPERATION_ENABLED
                       && axis->mode != CO_402_MODE_NONE) {
                /* PDS owns the physical stop while a paused mode transition retains only protocol ownership. */
                resetModeRuntime(axis, axis->mode, controlword);
            }
        }

        updateFeedback(axis);
        statusword = (uint16_t)(CO_402_statuswordForState(axis->state) | modeStatusword(axis));
        (void)OD_set_u16(axis->od.statusword, 0U, statusword, true);
        (void)OD_set_i8(axis->od.modesOfOperationDisplay, 0U, (int8_t)axis->mode, true);
    }
}
