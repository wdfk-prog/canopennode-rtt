/**
 * @file CO_402_device_od.c
 * @brief Generated-OD contract validation and binding for the CiA 402 Device core.
 */

#include <stddef.h>
#include <string.h>

#include "CO_402_device.h"

/* Scalar profile objects use sub-index zero; Homing speeds is validated separately as a RECORD. */
#define CO_402_OD_SUB_INDEX 0U

/* CANopenNode OD object-code bits used to reject incompatible generated objects. */
#define CO_402_OD_OBJECT_TYPE_VAR 0x01U
#define CO_402_OD_OBJECT_TYPE_REC 0x03U
#define CO_402_OD_OBJECT_TYPE_MASK 0x0FU

/* Validate only attributes represented by CANopenNode runtime metadata. */
#define CO_402_OD_ATTRIBUTE_CONTRACT_MASK (ODA_SDO_RW | ODA_TRPDO | ODA_TRSRDO | ODA_MB | ODA_STR)

/* Contract row for one scalar generated OD object. */
typedef struct {
    uint16_t axis0Index;          /**< Axis-0 profile index used to derive the logical-device index. */
    OD_size_t length;             /**< Required object width in bytes. */
    OD_attr_t expectedAttributes; /**< Exact runtime-visible access/mapping attributes. */
    size_t cacheOffset;           /**< Offset of the matching OD_entry_t pointer in CO_402_device_od_t. */
} CO_402_od_contract_t;

/*
 * Base contracts apply to every configured logical device. Mode-specific tables below
 * are required only when the build enables that mode and the axis DriveIF advertises it.
 * The selected product XDD remains the source of truth; runtime never creates OD objects.
 */
static const CO_402_od_contract_t baseContracts[] = {
    {CO_402_INDEX_ERROR_CODE, 2U, ODA_SDO_R | ODA_MB, offsetof(CO_402_device_od_t, errorCode)},
    {CO_402_INDEX_CONTROLWORD, 2U, ODA_SDO_RW | ODA_RPDO | ODA_MB, offsetof(CO_402_device_od_t, controlword)},
    {CO_402_INDEX_STATUSWORD, 2U, ODA_SDO_R | ODA_TPDO | ODA_MB, offsetof(CO_402_device_od_t, statusword)},
    {CO_402_INDEX_MODES_OF_OPERATION, 1U, ODA_SDO_RW | ODA_RPDO, offsetof(CO_402_device_od_t, modesOfOperation)},
    {CO_402_INDEX_MODES_OF_OPERATION_DISPLAY, 1U, ODA_SDO_R | ODA_TPDO,
     offsetof(CO_402_device_od_t, modesOfOperationDisplay)},
    {CO_402_INDEX_POSITION_ACTUAL_VALUE, 4U, ODA_SDO_R | ODA_TPDO | ODA_MB,
     offsetof(CO_402_device_od_t, positionActualValue)},
    {CO_402_INDEX_TARGET_POSITION, 4U, ODA_SDO_RW | ODA_RPDO | ODA_MB,
     offsetof(CO_402_device_od_t, targetPosition)},
    {CO_402_INDEX_VELOCITY_ACTUAL_VALUE, 4U, ODA_SDO_R | ODA_TPDO | ODA_MB,
     offsetof(CO_402_device_od_t, velocityActualValue)},
    {CO_402_INDEX_TARGET_VELOCITY, 4U, ODA_SDO_RW | ODA_RPDO | ODA_MB,
     offsetof(CO_402_device_od_t, targetVelocity)},
    {CO_402_INDEX_SUPPORTED_DRIVE_MODES, 4U, ODA_SDO_R | ODA_MB,
     offsetof(CO_402_device_od_t, supportedDriveModes)},
};

#if CO_402_CONFIG_MODE_PP
/* PP parameters are required only when this axis advertises PP and are edge-latched as one command. */
static const CO_402_od_contract_t ppContracts[] = {
    {CO_402_INDEX_PROFILE_VELOCITY, 4U, ODA_SDO_RW | ODA_MB, offsetof(CO_402_device_od_t, profileVelocity)},
    {CO_402_INDEX_PROFILE_ACCELERATION, 4U, ODA_SDO_RW | ODA_MB,
     offsetof(CO_402_device_od_t, profileAcceleration)},
    {CO_402_INDEX_PROFILE_DECELERATION, 4U, ODA_SDO_RW | ODA_MB,
     offsetof(CO_402_device_od_t, profileDeceleration)},
    {CO_402_INDEX_QUICK_STOP_DECELERATION, 4U, ODA_SDO_RW | ODA_MB,
     offsetof(CO_402_device_od_t, quickStopDeceleration)},
    {CO_402_INDEX_MOTION_PROFILE_TYPE, 2U, ODA_SDO_RW | ODA_MB,
     offsetof(CO_402_device_od_t, motionProfileType)},
};
#endif /* CO_402_CONFIG_MODE_PP */

#if CO_402_CONFIG_MODE_PV
/* PV parameters are required only when this axis advertises PV and remain live per supervisor pass. */
static const CO_402_od_contract_t pvContracts[] = {
    {CO_402_INDEX_PROFILE_ACCELERATION, 4U, ODA_SDO_RW | ODA_MB,
     offsetof(CO_402_device_od_t, profileAcceleration)},
    {CO_402_INDEX_PROFILE_DECELERATION, 4U, ODA_SDO_RW | ODA_MB,
     offsetof(CO_402_device_od_t, profileDeceleration)},
    {CO_402_INDEX_QUICK_STOP_DECELERATION, 4U, ODA_SDO_RW | ODA_MB,
     offsetof(CO_402_device_od_t, quickStopDeceleration)},
};
#endif /* CO_402_CONFIG_MODE_PV */

#if CO_402_CONFIG_MODE_HM
/* HM scalar parameters are start-edge latched; 0x6099 is validated separately because it is a RECORD. */
static const CO_402_od_contract_t hmContracts[] = {
    {CO_402_INDEX_HOME_OFFSET, 4U, ODA_SDO_RW | ODA_MB, offsetof(CO_402_device_od_t, homeOffset)},
    {CO_402_INDEX_HOMING_METHOD, 1U, ODA_SDO_RW, offsetof(CO_402_device_od_t, homingMethod)},
    {CO_402_INDEX_HOMING_ACCELERATION, 4U, ODA_SDO_RW | ODA_MB,
     offsetof(CO_402_device_od_t, homingAcceleration)},
};
#endif /* CO_402_CONFIG_MODE_HM */

#if CO_402_CONFIG_MODE_CST
/* CST requires its RPDO target and TPDO actual torque only when the axis advertises CST. */
static const CO_402_od_contract_t cstContracts[] = {
    {CO_402_INDEX_TARGET_TORQUE, 2U, ODA_SDO_RW | ODA_RPDO | ODA_MB,
     offsetof(CO_402_device_od_t, targetTorque)},
    {CO_402_INDEX_TORQUE_ACTUAL_VALUE, 2U, ODA_SDO_R | ODA_TPDO | ODA_MB,
     offsetof(CO_402_device_od_t, torqueActualValue)},
};
#endif /* CO_402_CONFIG_MODE_CST */

/* Populate an OD-specific initialization diagnostic, including RECORD sub-index when relevant. */
static void setDiag(CO_402_init_diag_t *diag, CO_402_init_error_t error, uint8_t logicalDevice,
                    uint16_t index, uint8_t subIndex)
{
    if (diag == NULL) {
        return;
    }

    diag->error = error;
    diag->logicalDevice = logicalDevice;
    diag->index = index;
    diag->subIndex = subIndex;
}

/*
 * Validate and cache one scalar generated OD object for one logical device.
 */
static CO_402_init_error_t validateOneScalar(OD_t *od, CO_402_device_axis_t *axis,
                                             const CO_402_od_contract_t *contract, CO_402_init_diag_t *diag)
{
    const uint16_t index = (uint16_t)(axis->odBase + (contract->axis0Index - CO_402_PROFILE_INDEX_BASE));
    OD_entry_t *entry = OD_find(od, index);
    OD_IO_t io;

    /* Entry and sub-index must already exist in the generated OD; runtime never synthesizes profile objects. */
    if (entry == NULL) {
        setDiag(diag, CO_402_INIT_OD_MISSING, axis->logicalDevice, index, CO_402_OD_SUB_INDEX);
        return CO_402_INIT_OD_MISSING;
    }
    if ((entry->odObjectType & CO_402_OD_OBJECT_TYPE_MASK) != CO_402_OD_OBJECT_TYPE_VAR || entry->subEntriesCount != 1U) {
        setDiag(diag, CO_402_INIT_OD_TYPE, axis->logicalDevice, index, CO_402_OD_SUB_INDEX);
        return CO_402_INIT_OD_TYPE;
    }
    if (OD_getSub(entry, CO_402_OD_SUB_INDEX, &io, true) != ODR_OK || io.stream.dataOrig == NULL) {
        setDiag(diag, CO_402_INIT_OD_MISSING, axis->logicalDevice, index, CO_402_OD_SUB_INDEX);
        return CO_402_INIT_OD_MISSING;
    }
    /*
     * Runtime metadata exposes object code, byte width and access/mapping attributes, but not the XDD semantic
     * INTEGER/UNSIGNED type name. Semantic typing therefore remains an XDD/generated-artifact contract.
     */
    if (io.stream.dataLength != contract->length) {
        setDiag(diag, CO_402_INIT_OD_LENGTH, axis->logicalDevice, index, CO_402_OD_SUB_INDEX);
        return CO_402_INIT_OD_LENGTH;
    }
    if ((io.stream.attribute & CO_402_OD_ATTRIBUTE_CONTRACT_MASK) != contract->expectedAttributes) {
        setDiag(diag, CO_402_INIT_OD_ACCESS, axis->logicalDevice, index, CO_402_OD_SUB_INDEX);
        return CO_402_INIT_OD_ACCESS;
    }

    /* cacheOffset comes from offsetof(CO_402_device_od_t, ...), avoiding a second index-to-cache switch. */
    *(OD_entry_t **)((uint8_t *)&axis->od + contract->cacheOffset) = entry;
    return CO_402_INIT_OK;
}

/* Apply one contract table without partially accepting a failed row. */
static CO_402_init_error_t validateScalarContracts(OD_t *od, CO_402_device_axis_t *axis,
                                                   const CO_402_od_contract_t *contracts, size_t count,
                                                   CO_402_init_diag_t *diag)
{
    size_t contractIndex;

    for (contractIndex = 0U; contractIndex < count; contractIndex++) {
        CO_402_init_error_t result = validateOneScalar(od, axis, &contracts[contractIndex], diag);
        if (result != CO_402_INIT_OK) {
            return result;
        }
    }

    return CO_402_INIT_OK;
}

#if CO_402_CONFIG_MODE_HM
/* 0x6099 is a RECORD, so validate its header and both speed sub-entries before caching it. */
static CO_402_init_error_t validateHomingSpeeds(OD_t *od, CO_402_device_axis_t *axis, CO_402_init_diag_t *diag)
{
    const uint16_t index = (uint16_t)(axis->odBase + (CO_402_INDEX_HOMING_SPEEDS - CO_402_PROFILE_INDEX_BASE));
    OD_entry_t *entry = OD_find(od, index);
    OD_IO_t io;
    uint8_t subIndex;

    if (entry == NULL) {
        setDiag(diag, CO_402_INIT_OD_MISSING, axis->logicalDevice, index, 0U);
        return CO_402_INIT_OD_MISSING;
    }
    if ((entry->odObjectType & CO_402_OD_OBJECT_TYPE_MASK) != CO_402_OD_OBJECT_TYPE_REC || entry->subEntriesCount != 3U) {
        setDiag(diag, CO_402_INIT_OD_TYPE, axis->logicalDevice, index, 0U);
        return CO_402_INIT_OD_TYPE;
    }

    for (subIndex = 0U; subIndex < 3U; subIndex++) {
        const OD_size_t expectedLength = subIndex == 0U ? 1U : 4U;
        const OD_attr_t expectedAttributes = subIndex == 0U ? ODA_SDO_R : (ODA_SDO_RW | ODA_MB);

        if (OD_getSub(entry, subIndex, &io, true) != ODR_OK || io.stream.dataOrig == NULL) {
            setDiag(diag, CO_402_INIT_OD_MISSING, axis->logicalDevice, index, subIndex);
            return CO_402_INIT_OD_MISSING;
        }
        if (io.stream.dataLength != expectedLength) {
            setDiag(diag, CO_402_INIT_OD_LENGTH, axis->logicalDevice, index, subIndex);
            return CO_402_INIT_OD_LENGTH;
        }
        if ((io.stream.attribute & CO_402_OD_ATTRIBUTE_CONTRACT_MASK) != expectedAttributes) {
            setDiag(diag, CO_402_INIT_OD_ACCESS, axis->logicalDevice, index, subIndex);
            return CO_402_INIT_OD_ACCESS;
        }
    }

    axis->od.homingSpeeds = entry;
    return CO_402_INIT_OK;
}
#endif /* CO_402_CONFIG_MODE_HM */

/* Do not require optional OD objects for a mode that this axis cannot execute. */
static CO_402_init_error_t validateModeObjects(OD_t *od, CO_402_device_axis_t *axis, CO_402_init_diag_t *diag)
{
#if CO_402_CONFIG_MODE_PP
    if ((axis->supportedModes & CO_402_SUPPORTED_MODE_PP) != 0U) {
        CO_402_init_error_t result =
            validateScalarContracts(od, axis, ppContracts, sizeof(ppContracts) / sizeof(ppContracts[0]), diag);
        if (result != CO_402_INIT_OK) {
            return result;
        }
    }
#endif /* CO_402_CONFIG_MODE_PP */
#if CO_402_CONFIG_MODE_PV
    if ((axis->supportedModes & CO_402_SUPPORTED_MODE_PV) != 0U) {
        CO_402_init_error_t result =
            validateScalarContracts(od, axis, pvContracts, sizeof(pvContracts) / sizeof(pvContracts[0]), diag);
        if (result != CO_402_INIT_OK) {
            return result;
        }
    }
#endif /* CO_402_CONFIG_MODE_PV */
#if CO_402_CONFIG_MODE_HM
    if ((axis->supportedModes & CO_402_SUPPORTED_MODE_HM) != 0U) {
        CO_402_init_error_t result =
            validateScalarContracts(od, axis, hmContracts, sizeof(hmContracts) / sizeof(hmContracts[0]), diag);
        if (result != CO_402_INIT_OK) {
            return result;
        }
        result = validateHomingSpeeds(od, axis, diag);
        if (result != CO_402_INIT_OK) {
            return result;
        }
    }
#endif /* CO_402_CONFIG_MODE_HM */
#if CO_402_CONFIG_MODE_CST
    if ((axis->supportedModes & CO_402_SUPPORTED_MODE_CST) != 0U) {
        CO_402_init_error_t result =
            validateScalarContracts(od, axis, cstContracts, sizeof(cstContracts) / sizeof(cstContracts[0]), diag);
        if (result != CO_402_INIT_OK) {
            return result;
        }
    }
#endif /* CO_402_CONFIG_MODE_CST */

    (void)od;
    (void)axis;
    (void)diag;
    return CO_402_INIT_OK;
}

/*
 * Reject required OD entries already extended by another subsystem.
 *
 * A rebind may reuse this axis' own extension, but must never replace another
 * component's callbacks because that would silently transfer OD ownership.
 */
static CO_402_init_error_t validateExtensionOwnership(const CO_402_device_axis_t *axis, CO_402_init_diag_t *diag)
{
    if (axis->od.controlword->extension != NULL
        && axis->od.controlword->extension != &axis->od.controlwordExtension) {
        setDiag(diag, CO_402_INIT_OD_EXTENSION_CONFLICT, axis->logicalDevice, axis->od.controlword->index, 0U);
        return CO_402_INIT_OD_EXTENSION_CONFLICT;
    }
    if (axis->od.modesOfOperation->extension != NULL
        && axis->od.modesOfOperation->extension != &axis->od.modeExtension) {
        setDiag(diag, CO_402_INIT_OD_EXTENSION_CONFLICT, axis->logicalDevice, axis->od.modesOfOperation->index, 0U);
        return CO_402_INIT_OD_EXTENSION_CONFLICT;
    }

    return CO_402_INIT_OK;
}

/*
 * Install transparent OD extensions owned by the Device axis.
 *
 * Binding happens before PDO initialization, so later communication objects cache
 * the extension-backed IO. Reads/writes still forward to generated storage.
 */
static void installForwardingExtensions(CO_402_device_axis_t *axis)
{
    memset(&axis->od.controlwordExtension, 0, sizeof(axis->od.controlwordExtension));
    axis->od.controlwordExtension.object = axis;
    axis->od.controlwordExtension.read = OD_readOriginal;
    axis->od.controlwordExtension.write = OD_writeOriginal;
    (void)OD_extension_init(axis->od.controlword, &axis->od.controlwordExtension);

    memset(&axis->od.modeExtension, 0, sizeof(axis->od.modeExtension));
    axis->od.modeExtension.object = axis;
    axis->od.modeExtension.read = OD_readOriginal;
    axis->od.modeExtension.write = OD_writeOriginal;
    (void)OD_extension_init(axis->od.modesOfOperation, &axis->od.modeExtension);
}

/*
 * Clear cached OD pointers before a fresh bind while retaining this axis' extension objects.
 * Communication Reset detaches old entries first; preserving extension storage keeps ownership identity stable.
 */
static void clearODCache(CO_402_device_axis_t *axis)
{
    OD_extension_t controlwordExtension = axis->od.controlwordExtension;
    OD_extension_t modeExtension = axis->od.modeExtension;

    memset(&axis->od, 0, sizeof(axis->od));
    axis->od.controlwordExtension = controlwordExtension;
    axis->od.modeExtension = modeExtension;
}

CO_402_init_error_t CO_402_device_bindOD(CO_402_device_manager_t *manager, CO_402_init_diag_t *diag)
{
    uint8_t axisIndex;

    if (diag != NULL) {
        memset(diag, 0, sizeof(*diag));
    }
    if (manager == NULL || manager->od == NULL || manager->axes == NULL || manager->axisCount == 0U) {
        setDiag(diag, CO_402_INIT_CONFIG_MISMATCH, 0U, 0U, 0U);
        return CO_402_INIT_CONFIG_MISMATCH;
    }

    /* Validate the complete manager before installing any new extension so binding remains all-or-nothing. */
    manager->odBound = false;

    for (axisIndex = 0U; axisIndex < manager->axisCount; axisIndex++) {
        CO_402_device_axis_t *axis = &manager->axes[axisIndex];
        CO_402_init_error_t result;

        /* Clear stale cache entries before validating the replacement/generated OD. */
        clearODCache(axis);
        result = validateScalarContracts(manager->od, axis, baseContracts,
                                         sizeof(baseContracts) / sizeof(baseContracts[0]), diag);
        if (result != CO_402_INIT_OK) {
            return result;
        }
        result = validateModeObjects(manager->od, axis, diag);
        if (result != CO_402_INIT_OK) {
            return result;
        }
        result = validateExtensionOwnership(axis, diag);
        if (result != CO_402_INIT_OK) {
            return result;
        }
    }

    /* Publish writable profile state before extension installation so binding remains all-or-nothing on failure. */
    for (axisIndex = 0U; axisIndex < manager->axisCount; axisIndex++) {
        CO_402_device_axis_t *axis = &manager->axes[axisIndex];

        if (OD_set_u32(axis->od.supportedDriveModes, 0U, axis->supportedModes, true) != ODR_OK) {
            setDiag(diag, CO_402_INIT_OD_ACCESS, axis->logicalDevice, axis->od.supportedDriveModes->index, 0U);
            return CO_402_INIT_OD_ACCESS;
        }
#if CO_402_CONFIG_DIAGNOSTICS
        /* Rebinding restores 0x603F state and queues active-fault replay before any forwarding extension is published. */
        if (!CO_402_device_diag_restoreAxis(axis, true)) {
            setDiag(diag, CO_402_INIT_DIAG_OD, axis->logicalDevice, axis->od.errorCode->index, 0U);
            return CO_402_INIT_DIAG_OD;
        }
#endif /* CO_402_CONFIG_DIAGNOSTICS */
    }

    for (axisIndex = 0U; axisIndex < manager->axisCount; axisIndex++) {
        installForwardingExtensions(&manager->axes[axisIndex]);
    }

    manager->odBound = true;
    return CO_402_INIT_OK;
}
