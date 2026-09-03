/**
 * @file CO_402_device_od.c
 * @brief Generated-OD contract validation and binding for the CiA 402 Device core.
 */

#include <stddef.h>
#include <string.h>

#include "CO_402_device.h"

/** Profile objects used by A3 are scalar variables and therefore use sub-index zero. */
#define CO_402_OD_SUB_INDEX 0U

/** CANopenNode OD object-code bits used to reject non-scalar profile objects. */
#define CO_402_OD_OBJECT_TYPE_MASK 0x0FU
#define CO_402_OD_OBJECT_TYPE_VAR  0x01U

/** Runtime-visible OD attributes whose exact A3 contract is validated. */
#define CO_402_OD_ATTRIBUTE_CONTRACT_MASK (ODA_SDO_RW | ODA_TRPDO | ODA_TRSRDO | ODA_MB | ODA_STR)

/** Description of one generated OD object required by every configured logical device. */
typedef struct {
    uint16_t axis0Index;          /**< Axis-0 profile index used to derive the logical-device index. */
    OD_size_t length;             /**< Required object width in bytes. */
    OD_attr_t expectedAttributes; /**< Exact SDO/PDO/MB attributes required by the Device core. */
    size_t cacheOffset;           /**< Offset of the matching OD_entry_t pointer in CO_402_device_od_t. */
} CO_402_od_contract_t;

/*
 * Keep the runtime contract in one table so every logical device is validated
 * with identical widths and access/mapping requirements before any extension
 * is installed. The table does not create OD objects; the selected product XDD
 * remains the source of truth for object existence and mapping.
 */
static const CO_402_od_contract_t odContracts[] = {
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

/*
 * Populate an OD-specific initialization diagnostic.
 */
static void setDiag(CO_402_init_diag_t *diag, CO_402_init_error_t error, uint8_t logicalDevice, uint16_t index)
{
    if (diag == NULL) {
        return;
    }

    diag->error = error;
    diag->logicalDevice = logicalDevice;
    diag->index = index;
    diag->subIndex = CO_402_OD_SUB_INDEX;
}

/*
 * Validate and cache one generated OD object for one logical device.
 */
static CO_402_init_error_t validateOneObject(OD_t *od, CO_402_device_axis_t *axis,
                                             const CO_402_od_contract_t *contract, CO_402_init_diag_t *diag)
{
    const uint16_t index = (uint16_t)(axis->odBase + (contract->axis0Index - CO_402_PROFILE_INDEX_BASE));
    OD_entry_t *entry = OD_find(od, index);
    OD_IO_t io;

    /* Entry and sub-index must already exist in the generated OD; runtime never synthesizes profile objects. */
    if (entry == NULL) {
        setDiag(diag, CO_402_INIT_OD_MISSING, axis->logicalDevice, index);
        return CO_402_INIT_OD_MISSING;
    }
    if ((entry->odObjectType & CO_402_OD_OBJECT_TYPE_MASK) != CO_402_OD_OBJECT_TYPE_VAR
        || entry->subEntriesCount != 1U) {
        setDiag(diag, CO_402_INIT_OD_TYPE, axis->logicalDevice, index);
        return CO_402_INIT_OD_TYPE;
    }
    if (OD_getSub(entry, CO_402_OD_SUB_INDEX, &io, true) != ODR_OK) {
        setDiag(diag, CO_402_INIT_OD_MISSING, axis->logicalDevice, index);
        return CO_402_INIT_OD_MISSING;
    }
    if (io.stream.dataOrig == NULL) {
        setDiag(diag, CO_402_INIT_OD_MISSING, axis->logicalDevice, index);
        return CO_402_INIT_OD_MISSING;
    }

    /*
     * Runtime OD metadata exposes object code, byte width and access/mapping attributes, but not the XDD semantic
     * INTEGER/UNSIGNED type name. Semantic typing therefore remains an XDD/generated-artifact contract.
     */
    if (io.stream.dataLength != contract->length) {
        setDiag(diag, CO_402_INIT_OD_LENGTH, axis->logicalDevice, index);
        return CO_402_INIT_OD_LENGTH;
    }
    if ((io.stream.attribute & CO_402_OD_ATTRIBUTE_CONTRACT_MASK) != contract->expectedAttributes) {
        setDiag(diag, CO_402_INIT_OD_ACCESS, axis->logicalDevice, index);
        return CO_402_INIT_OD_ACCESS;
    }

    /* cacheOffset is produced with offsetof() against CO_402_device_od_t, avoiding a second switch table. */
    *(OD_entry_t **)((uint8_t *)&axis->od + contract->cacheOffset) = entry;
    return CO_402_INIT_OK;
}

/*
 * Reject required OD entries already extended by another subsystem.
 *
 * The Device core only accepts its own previously installed extension so a
 * rebind does not silently replace another component's callbacks.
 */
static CO_402_init_error_t validateExtensionOwnership(const CO_402_device_axis_t *axis, CO_402_init_diag_t *diag)
{
    if (axis->od.controlword->extension != NULL
        && axis->od.controlword->extension != &axis->od.controlwordExtension) {
        setDiag(diag, CO_402_INIT_OD_EXTENSION_CONFLICT, axis->logicalDevice, axis->od.controlword->index);
        return CO_402_INIT_OD_EXTENSION_CONFLICT;
    }
    if (axis->od.modesOfOperation->extension != NULL
        && axis->od.modesOfOperation->extension != &axis->od.modeExtension) {
        setDiag(diag, CO_402_INIT_OD_EXTENSION_CONFLICT, axis->logicalDevice, axis->od.modesOfOperation->index);
        return CO_402_INIT_OD_EXTENSION_CONFLICT;
    }

    return CO_402_INIT_OK;
}

/*
 * Install transparent OD extensions reserved for later Device callbacks.
 *
 * Binding happens before later PDO initialization so CANopenNode observes the
 * extension-backed OD IO from the beginning. A3 forwards reads/writes to the
 * generated storage and only establishes ownership/lifecycle ordering.
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

CO_402_init_error_t CO_402_device_bindOD(CO_402_device_manager_t *manager, CO_402_init_diag_t *diag)
{
    uint8_t axisIndex;
    size_t contractIndex;

    if (diag != NULL) {
        memset(diag, 0, sizeof(*diag));
    }
    if (manager == NULL || manager->od == NULL || manager->axes == NULL || manager->axisCount == 0U) {
        setDiag(diag, CO_402_INIT_CONFIG_MISMATCH, 0U, 0U);
        return CO_402_INIT_CONFIG_MISMATCH;
    }

    /* Validate the complete manager first so binding is all-or-nothing for configuration failures. */
    manager->odBound = false;

    for (axisIndex = 0U; axisIndex < manager->axisCount; axisIndex++) {
        CO_402_device_axis_t *axis = &manager->axes[axisIndex];

        /* Clear stale cache entries before validating a fresh OD binding pass. */
        axis->od.errorCode = NULL;
        axis->od.controlword = NULL;
        axis->od.statusword = NULL;
        axis->od.modesOfOperation = NULL;
        axis->od.modesOfOperationDisplay = NULL;
        axis->od.positionActualValue = NULL;
        axis->od.targetPosition = NULL;
        axis->od.velocityActualValue = NULL;
        axis->od.targetVelocity = NULL;
        axis->od.supportedDriveModes = NULL;

        /* One failing object aborts the pass before any new extension is installed. */
        for (contractIndex = 0U; contractIndex < (sizeof(odContracts) / sizeof(odContracts[0])); contractIndex++) {
            CO_402_init_error_t result = validateOneObject(manager->od, axis, &odContracts[contractIndex], diag);
            if (result != CO_402_INIT_OK) {
                return result;
            }
        }

        {
            CO_402_init_error_t result = validateExtensionOwnership(axis, diag);
            if (result != CO_402_INIT_OK) {
                return result;
            }
        }
    }

    /* Install extensions only after every axis has passed object and ownership validation. */
    for (axisIndex = 0U; axisIndex < manager->axisCount; axisIndex++) {
        installForwardingExtensions(&manager->axes[axisIndex]);
    }

    manager->odBound = true;
    return CO_402_INIT_OK;
}
