/**
 * @file CO_401_device_od.c
 * @brief Fail-closed generated-OD validation for the Stage-1 CiA 401 Device core.
 */

#include <string.h>

/* Expose CANopenNode OD definition metadata so ARRAY storage stride can be validated. */
#define OD_DEFINITION
#include "CO_401_device.h"
#undef OD_DEFINITION

#define CO_401_OD_OBJECT_TYPE_VAR 0x01U
#define CO_401_OD_OBJECT_TYPE_ARRAY 0x02U
#define CO_401_OD_OBJECT_TYPE_MASK 0x0FU
#define CO_401_OD_ATTRIBUTE_CONTRACT_MASK (ODA_SDO_RW | ODA_TRPDO | ODA_TRSRDO | ODA_MB | ODA_STR)

static void setDiag(CO_401_init_diag_t *diag, CO_401_init_error_t error, uint16_t index, uint8_t subIndex)
{
    if (diag != NULL) {
        diag->error = error;
        diag->index = index;
        diag->subIndex = subIndex;
    }
}

static CO_401_init_error_t validateDeviceType(OD_t *od, CO_401_capabilities_t capabilities,
                                               CO_401_device_od_t *bound, CO_401_init_diag_t *diag)
{
    OD_entry_t *entry = OD_find(od, CO_401_INDEX_DEVICE_TYPE);
    OD_IO_t io;
    uint32_t value;

    if (entry == NULL) {
        setDiag(diag, CO_401_INIT_OD_MISSING, CO_401_INDEX_DEVICE_TYPE, 0U);
        return CO_401_INIT_OD_MISSING;
    }
    if ((entry->odObjectType & CO_401_OD_OBJECT_TYPE_MASK) != CO_401_OD_OBJECT_TYPE_VAR
        || entry->subEntriesCount != 1U) {
        setDiag(diag, CO_401_INIT_OD_TYPE, CO_401_INDEX_DEVICE_TYPE, 0U);
        return CO_401_INIT_OD_TYPE;
    }
    if (OD_getSub(entry, 0U, &io, true) != ODR_OK || io.stream.dataOrig == NULL) {
        setDiag(diag, CO_401_INIT_OD_MISSING, CO_401_INDEX_DEVICE_TYPE, 0U);
        return CO_401_INIT_OD_MISSING;
    }
    if (io.stream.dataLength != sizeof(value)) {
        setDiag(diag, CO_401_INIT_OD_LENGTH, CO_401_INDEX_DEVICE_TYPE, 0U);
        return CO_401_INIT_OD_LENGTH;
    }
    if ((io.stream.attribute & CO_401_OD_ATTRIBUTE_CONTRACT_MASK) != (ODA_SDO_R | ODA_MB)) {
        setDiag(diag, CO_401_INIT_OD_ACCESS, CO_401_INDEX_DEVICE_TYPE, 0U);
        return CO_401_INIT_OD_ACCESS;
    }
    if (OD_get_u32(entry, 0U, &value, true) != ODR_OK
        || value != CO_401_deviceTypeForCapabilities(capabilities)) {
        setDiag(diag, CO_401_INIT_DEVICE_TYPE, CO_401_INDEX_DEVICE_TYPE, 0U);
        return CO_401_INIT_DEVICE_TYPE;
    }

    bound->deviceType = entry;
    return CO_401_INIT_OK;
}

static CO_401_init_error_t validateArray(OD_t *od, uint16_t index, uint8_t expectedCount,
                                          OD_size_t elementLength, OD_attr_t expectedAttributes,
                                          OD_entry_t **cache, CO_401_init_diag_t *diag)
{
    OD_entry_t *entry = OD_find(od, index);
    OD_IO_t io;
    uint8_t subCount;

    if (entry == NULL) {
        setDiag(diag, CO_401_INIT_OD_MISSING, index, 0U);
        return CO_401_INIT_OD_MISSING;
    }
    if ((entry->odObjectType & CO_401_OD_OBJECT_TYPE_MASK) != CO_401_OD_OBJECT_TYPE_ARRAY) {
        setDiag(diag, CO_401_INIT_OD_TYPE, index, 0U);
        return CO_401_INIT_OD_TYPE;
    }
    if (entry->subEntriesCount != (uint8_t)(expectedCount + 1U)) {
        setDiag(diag, CO_401_INIT_OD_SUB_COUNT, index, 0U);
        return CO_401_INIT_OD_SUB_COUNT;
    }
    if (OD_getSub(entry, 0U, &io, true) != ODR_OK || io.stream.dataOrig == NULL) {
        setDiag(diag, CO_401_INIT_OD_MISSING, index, 0U);
        return CO_401_INIT_OD_MISSING;
    }
    if (io.stream.dataLength != 1U) {
        setDiag(diag, CO_401_INIT_OD_LENGTH, index, 0U);
        return CO_401_INIT_OD_LENGTH;
    }
    if ((io.stream.attribute & CO_401_OD_ATTRIBUTE_CONTRACT_MASK) != ODA_SDO_R) {
        setDiag(diag, CO_401_INIT_OD_ACCESS, index, 0U);
        return CO_401_INIT_OD_ACCESS;
    }
    if (OD_get_u8(entry, 0U, &subCount, true) != ODR_OK || subCount != expectedCount) {
        setDiag(diag, CO_401_INIT_OD_SUB_VALUE, index, 0U);
        return CO_401_INIT_OD_SUB_VALUE;
    }
    /*
     * CANopenNode runtime metadata exposes ARRAY shape, width and access/mapping
     * attributes, but not the XDD UNSIGNED8 versus INTEGER16 semantic type name.
     * Signedness therefore remains a generated-device-description contract.
     */
    if (OD_getSub(entry, 1U, &io, true) != ODR_OK || io.stream.dataOrig == NULL) {
        setDiag(diag, CO_401_INIT_OD_MISSING, index, 1U);
        return CO_401_INIT_OD_MISSING;
    }
    if (io.stream.dataLength != elementLength) {
        setDiag(diag, CO_401_INIT_OD_LENGTH, index, 1U);
        return CO_401_INIT_OD_LENGTH;
    }
    /*
     * OD_getSub() reports dataElementLength as stream length, but uses
     * dataElementSizeof as the address stride for sub-indexes above one. Both
     * must match the Stage-1 scalar storage width or channels can overlap.
     */
    const OD_obj_array_t *array = entry->odObject;
    if (array->dataElementSizeof != elementLength) {
        setDiag(diag, CO_401_INIT_OD_LENGTH, index, 1U);
        return CO_401_INIT_OD_LENGTH;
    }
    if ((io.stream.attribute & CO_401_OD_ATTRIBUTE_CONTRACT_MASK) != expectedAttributes) {
        setDiag(diag, CO_401_INIT_OD_ACCESS, index, 1U);
        return CO_401_INIT_OD_ACCESS;
    }

    *cache = entry;
    return CO_401_INIT_OK;
}

static CO_401_init_error_t validateCapabilityObject(OD_t *od, bool enabled, uint16_t index,
                                                     uint8_t expectedCount, OD_size_t elementLength,
                                                     OD_attr_t expectedAttributes, OD_entry_t **cache,
                                                     CO_401_init_diag_t *diag)
{
    if (!enabled) {
        if (OD_find(od, index) != NULL) {
            setDiag(diag, CO_401_INIT_OD_UNEXPECTED, index, 0U);
            return CO_401_INIT_OD_UNEXPECTED;
        }
        *cache = NULL;
        return CO_401_INIT_OK;
    }

    return validateArray(od, index, expectedCount, elementLength, expectedAttributes, cache, diag);
}

CO_401_init_error_t CO_401_device_bindOD(CO_401_device_t *device, CO_401_init_diag_t *diag)
{
    CO_401_device_od_t candidate;
    CO_401_init_error_t result;

    if (device == NULL) {
        setDiag(diag, CO_401_INIT_BAD_ARGUMENT, 0U, 0U);
        return CO_401_INIT_BAD_ARGUMENT;
    }

    device->odBound = false;
    if (device->od == NULL) {
        setDiag(diag, CO_401_INIT_BAD_ARGUMENT, 0U, 0U);
        return CO_401_INIT_BAD_ARGUMENT;
    }
    (void)memset(&candidate, 0, sizeof(candidate));

    result = validateDeviceType(device->od, device->capabilities, &candidate, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }

    result = validateCapabilityObject(device->od, device->config.digitalInputBanks != 0U,
                                      CO_401_INDEX_DIGITAL_INPUT_8, device->config.digitalInputBanks,
                                      1U, ODA_SDO_R | ODA_TPDO, &candidate.digitalInput8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
    result = validateCapabilityObject(device->od, device->config.digitalOutputBanks != 0U,
                                      CO_401_INDEX_DIGITAL_OUTPUT_8, device->config.digitalOutputBanks,
                                      1U, ODA_SDO_RW | ODA_RPDO, &candidate.digitalOutput8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
    result = validateCapabilityObject(device->od, device->config.analogInputChannels != 0U,
                                      CO_401_INDEX_ANALOG_INPUT_16, device->config.analogInputChannels,
                                      2U, ODA_SDO_R | ODA_TPDO | ODA_MB, &candidate.analogInput16, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
    result = validateCapabilityObject(device->od, device->config.analogOutputChannels != 0U,
                                      CO_401_INDEX_ANALOG_OUTPUT_16, device->config.analogOutputChannels,
                                      2U, ODA_SDO_RW | ODA_RPDO | ODA_MB, &candidate.analogOutput16, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }

    device->bound = candidate;
    device->odBound = true;
    setDiag(diag, CO_401_INIT_OK, 0U, 0U);
    return CO_401_INIT_OK;
}
