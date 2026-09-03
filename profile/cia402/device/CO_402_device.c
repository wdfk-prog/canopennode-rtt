/**
 * @file CO_402_device.c
 * @brief Pure-C multi-axis CiA 402 Device manager.
 */

#include <string.h>

#include "CO_402_device.h"
#include "CO_402_device_fsa.h"

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
 * Feedback callbacks remain optional because the Device core can expose the
 * generated OD even when a product does not provide all measurements yet.
 */
static bool driveInterfaceValid(const CO_402_drive_if_t *drive)
{
    return drive != NULL && drive->shutdown != NULL && drive->switchOn != NULL && drive->enableOperation != NULL
           && drive->disableVoltage != NULL && drive->disableOperation != NULL && drive->quickStop != NULL
           && drive->faultReaction != NULL && drive->faultReset != NULL;
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
        /* Every axis must reference one valid logical-device block and a complete DriveIF. */
        if (configs[axisIndex].logicalDevice >= CO_402_LOGICAL_DEVICE_COUNT_MAX
            || !driveInterfaceValid(configs[axisIndex].drive)) {
            setDiag(diag, CO_402_INIT_BAD_AXIS, configs[axisIndex].logicalDevice);
            return CO_402_INIT_BAD_AXIS;
        }

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
 * Snapshot the requested operation mode without activating mode runtime.
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
    if (axis->requestedModeRecognized && mode == CO_402_MODE_NONE) {
        axis->mode = CO_402_MODE_NONE;
    }

    /* A3 implements the PDS supervisor only; mode activation remains owned by A5/A6. */
    (void)OD_set_i8(axis->od.modesOfOperationDisplay, 0U, (int8_t)axis->mode, true);
}

/*
 * Copy optional drive feedback into the generated OD process image.
 */
static void updateFeedback(CO_402_device_axis_t *axis)
{
    /* Missing optional feedback callbacks leave the corresponding generated OD value unchanged. */
    if (axis->drive->getPosition != NULL) {
        (void)OD_set_i32(axis->od.positionActualValue, 0U, axis->drive->getPosition(axis->driveObject), true);
    }
    if (axis->drive->getVelocity != NULL) {
        (void)OD_set_i32(axis->od.velocityActualValue, 0U, axis->drive->getVelocity(axis->driveObject), true);
    }
}

void CO_402_device_process(CO_402_device_manager_t *manager)
{
    uint8_t axisIndex;

    /* A partially initialized manager must not touch cached OD pointers or DriveIF callbacks. */
    if (manager == NULL || !manager->odBound) {
        return;
    }

    /* Each axis is processed independently so one PDS state cannot overwrite another axis runtime. */
    for (axisIndex = 0U; axisIndex < manager->axisCount; axisIndex++) {
        CO_402_device_axis_t *axis = &manager->axes[axisIndex];

        updateModeRequest(axis);
        CO_402_device_axisProcess(axis);
        updateFeedback(axis);
        (void)OD_set_u16(axis->od.statusword, 0U, CO_402_statuswordForState(axis->state), true);
    }
}
