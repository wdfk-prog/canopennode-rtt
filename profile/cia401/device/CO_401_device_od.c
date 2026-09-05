/**
 * @file CO_401_device_od.c
 * @brief Fail-closed generated-OD validation and Stage-2 forwarding hooks for the CiA 401 Device core.
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

static bool attributesMatch(OD_attr_t attributes, OD_attr_t required, OD_attr_t allowed)
{
    const OD_attr_t contract = attributes & CO_401_OD_ATTRIBUTE_CONTRACT_MASK;

    return (contract & required) == required && (contract & (OD_attr_t)(~allowed)) == 0U;
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
    if (!attributesMatch(io.stream.attribute, ODA_SDO_R | ODA_MB, ODA_SDO_R | ODA_MB)) {
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

static CO_401_init_error_t validateArrayContract(OD_t *od, uint16_t index, uint8_t expectedCount,
                                                  OD_size_t elementLength, OD_attr_t requiredAttributes,
                                                  OD_attr_t allowedAttributes, OD_entry_t **cache,
                                                  CO_401_init_diag_t *diag)
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
    if (!attributesMatch(io.stream.attribute, ODA_SDO_R, ODA_SDO_R)) {
        setDiag(diag, CO_401_INIT_OD_ACCESS, index, 0U);
        return CO_401_INIT_OD_ACCESS;
    }
    if (OD_get_u8(entry, 0U, &subCount, true) != ODR_OK || subCount != expectedCount) {
        setDiag(diag, CO_401_INIT_OD_SUB_VALUE, index, 0U);
        return CO_401_INIT_OD_SUB_VALUE;
    }
    /*
     * CANopenNode runtime metadata exposes ARRAY shape, width and access/mapping
     * attributes, but not the XDD scalar semantic type name. Signedness remains
     * a generated-device-description contract.
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
     * must match the scalar storage width or channels can overlap.
     */
    const OD_obj_array_t *array = entry->odObject;
    if (array->dataElementSizeof != elementLength) {
        setDiag(diag, CO_401_INIT_OD_LENGTH, index, 1U);
        return CO_401_INIT_OD_LENGTH;
    }
    if (!attributesMatch(io.stream.attribute, requiredAttributes, allowedAttributes)) {
        setDiag(diag, CO_401_INIT_OD_ACCESS, index, 1U);
        return CO_401_INIT_OD_ACCESS;
    }

    *cache = entry;
    return CO_401_INIT_OK;
}

static CO_401_init_error_t validateArray(OD_t *od, uint16_t index, uint8_t expectedCount,
                                          OD_size_t elementLength, OD_attr_t expectedAttributes,
                                          OD_entry_t **cache, CO_401_init_diag_t *diag)
{
    return validateArrayContract(od, index, expectedCount, elementLength, expectedAttributes,
                                 expectedAttributes, cache, diag);
}

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS) \
    || defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
static CO_401_init_error_t validateOptionalMappingArray(OD_t *od, uint16_t index, uint8_t expectedCount,
                                                         OD_entry_t **cache, CO_401_init_diag_t *diag)
{
    return validateArrayContract(od, index, expectedCount, 1U, ODA_SDO_RW,
                                 ODA_SDO_RW | ODA_TRPDO, cache, diag);
}
#endif /* defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS) || defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) */

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
static CO_401_init_error_t validateVariable8(OD_t *od, uint16_t index, OD_attr_t expectedAttributes,
                                              OD_entry_t **cache, CO_401_init_diag_t *diag)
{
    OD_entry_t *entry = OD_find(od, index);
    OD_IO_t io;

    if (entry == NULL) {
        setDiag(diag, CO_401_INIT_OD_MISSING, index, 0U);
        return CO_401_INIT_OD_MISSING;
    }
    if ((entry->odObjectType & CO_401_OD_OBJECT_TYPE_MASK) != CO_401_OD_OBJECT_TYPE_VAR
        || entry->subEntriesCount != 1U) {
        setDiag(diag, CO_401_INIT_OD_TYPE, index, 0U);
        return CO_401_INIT_OD_TYPE;
    }
    if (OD_getSub(entry, 0U, &io, true) != ODR_OK || io.stream.dataOrig == NULL) {
        setDiag(diag, CO_401_INIT_OD_MISSING, index, 0U);
        return CO_401_INIT_OD_MISSING;
    }
    if (io.stream.dataLength != 1U) {
        setDiag(diag, CO_401_INIT_OD_LENGTH, index, 0U);
        return CO_401_INIT_OD_LENGTH;
    }
    if (!attributesMatch(io.stream.attribute, expectedAttributes, expectedAttributes)) {
        setDiag(diag, CO_401_INIT_OD_ACCESS, index, 0U);
        return CO_401_INIT_OD_ACCESS;
    }

    *cache = entry;
    return CO_401_INIT_OK;
}
#endif /* defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS) */

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

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS) \
    || defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
static CO_401_init_error_t validateOptionalArray(OD_t *od, bool enabled, uint16_t index,
                                                  uint8_t expectedCount, OD_entry_t **cache,
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

    return validateOptionalMappingArray(od, index, expectedCount, cache, diag);
}
#endif /* defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS) || defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) */

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
static ODR_t writeDigitalInputFilter(OD_stream_t *stream, const void *buf, OD_size_t count,
                                     OD_size_t *countWritten)
{
    CO_401_device_t *device;
    ODR_t result;

    if (stream == NULL) {
        return ODR_DEV_INCOMPAT;
    }
    device = stream->object;
    if (device == NULL) {
        return ODR_DEV_INCOMPAT;
    }

    result = OD_writeOriginal(stream, buf, count, countWritten);
    if (result == ODR_OK && stream->subIndex != 0U) {
        /* Defer the product callback so SDO/RPDO OD access never performs hardware I/O. */
        device->digitalInputFilterDirty = true;
    }
    return result;
}
#endif /* defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS) */

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS) \
    || defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
static CO_401_init_error_t validateExtensionSlot(OD_entry_t *entry, OD_extension_t *ownedExtension,
                                                  uint16_t index, CO_401_init_diag_t *diag)
{
    if (entry != NULL && entry->extension != NULL && entry->extension != ownedExtension) {
        setDiag(diag, CO_401_INIT_OD_ACCESS, index, 0U);
        return CO_401_INIT_OD_ACCESS;
    }
    return CO_401_INIT_OK;
}

static void initializeExtension(OD_extension_t *extension, void *object,
                                ODR_t (*write)(OD_stream_t *, const void *, OD_size_t, OD_size_t *))
{
    (void)memset(extension, 0, sizeof(*extension));
    extension->object = object;
    extension->read = OD_readOriginal;
    extension->write = write;
#if OD_FLAGS_PDO_SIZE > 0
    /* OD_requestTPDO() requests transmission by clearing a bit, so start with no pending requests. */
    (void)memset(extension->flagsPDO, 0xFF, sizeof(extension->flagsPDO));
#endif /* OD_FLAGS_PDO_SIZE > 0 */
}

static void detachOwnedExtensions(CO_401_device_t *device)
{
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    if (device->bound.digitalInput8 != NULL
        && device->bound.digitalInput8->extension == &device->digitalInput8Extension) {
        (void)OD_extension_init(device->bound.digitalInput8, NULL);
    }
    if (device->bound.digitalInputFilter8 != NULL
        && device->bound.digitalInputFilter8->extension == &device->digitalInputFilter8Extension) {
        (void)OD_extension_init(device->bound.digitalInputFilter8, NULL);
    }
    device->digitalInputFilterDirty = false;
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS */
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    if (device->bound.digitalOutput8 != NULL
        && device->bound.digitalOutput8->extension == &device->digitalOutput8Extension) {
        (void)OD_extension_init(device->bound.digitalOutput8, NULL);
    }
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE */
}
#endif /* defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS) || defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) */

CO_401_init_error_t CO_401_device_bindOD(CO_401_device_t *device, CO_401_init_diag_t *diag)
{
    CO_401_device_od_t candidate;
    CO_401_init_error_t result;
    const bool digitalInputEnabled = device != NULL && device->config.digitalInputBanks != 0U;
    const bool digitalOutputEnabled = device != NULL && device->config.digitalOutputBanks != 0U;

    if (device == NULL) {
        setDiag(diag, CO_401_INIT_BAD_ARGUMENT, 0U, 0U);
        return CO_401_INIT_BAD_ARGUMENT;
    }

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS) \
    || defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    /* A failed rebind must not leave forwarding hooks from the previous communication generation reachable. */
    detachOwnedExtensions(device);
#endif /* defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS) || defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) */
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

    result = validateCapabilityObject(device->od, digitalInputEnabled, CO_401_INDEX_DIGITAL_INPUT_8,
                                      device->config.digitalInputBanks, 1U, ODA_SDO_R | ODA_TPDO,
                                      &candidate.digitalInput8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
    result = validateCapabilityObject(device->od, digitalOutputEnabled, CO_401_INDEX_DIGITAL_OUTPUT_8,
                                      device->config.digitalOutputBanks, 1U, ODA_SDO_RW | ODA_RPDO,
                                      &candidate.digitalOutput8, diag);
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

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    result = validateOptionalArray(device->od, digitalInputEnabled, CO_401_INDEX_DIGITAL_INPUT_POLARITY_8,
                                   device->config.digitalInputBanks, &candidate.digitalInputPolarity8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
    result = validateOptionalArray(device->od, digitalInputEnabled, CO_401_INDEX_DIGITAL_INPUT_FILTER_8,
                                   device->config.digitalInputBanks, &candidate.digitalInputFilter8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
    if (digitalInputEnabled) {
        result = validateVariable8(device->od, CO_401_INDEX_DIGITAL_INTERRUPT_ENABLE,
                                   ODA_SDO_RW, &candidate.digitalInterruptEnable, diag);
    } else if (OD_find(device->od, CO_401_INDEX_DIGITAL_INTERRUPT_ENABLE) != NULL) {
        setDiag(diag, CO_401_INIT_OD_UNEXPECTED, CO_401_INDEX_DIGITAL_INTERRUPT_ENABLE, 0U);
        return CO_401_INIT_OD_UNEXPECTED;
    }
    if (result != CO_401_INIT_OK) {
        return result;
    }
    result = validateOptionalArray(device->od, digitalInputEnabled, CO_401_INDEX_DIGITAL_INTERRUPT_ANY_8,
                                   device->config.digitalInputBanks, &candidate.digitalInterruptAny8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
    result = validateOptionalArray(device->od, digitalInputEnabled, CO_401_INDEX_DIGITAL_INTERRUPT_RISING_8,
                                   device->config.digitalInputBanks, &candidate.digitalInterruptRising8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
    result = validateOptionalArray(device->od, digitalInputEnabled, CO_401_INDEX_DIGITAL_INTERRUPT_FALLING_8,
                                   device->config.digitalInputBanks, &candidate.digitalInterruptFalling8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
#endif /* defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS) */

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    result = validateOptionalArray(device->od, digitalOutputEnabled, CO_401_INDEX_DIGITAL_OUTPUT_POLARITY_8,
                                   device->config.digitalOutputBanks, &candidate.digitalOutputPolarity8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
    result = validateOptionalArray(device->od, digitalOutputEnabled, CO_401_INDEX_DIGITAL_OUTPUT_ERROR_MODE_8,
                                   device->config.digitalOutputBanks, &candidate.digitalOutputErrorMode8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
    result = validateOptionalArray(device->od, digitalOutputEnabled, CO_401_INDEX_DIGITAL_OUTPUT_ERROR_VALUE_8,
                                   device->config.digitalOutputBanks, &candidate.digitalOutputErrorValue8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
    result = validateOptionalArray(device->od, digitalOutputEnabled, CO_401_INDEX_DIGITAL_OUTPUT_FILTER_8,
                                   device->config.digitalOutputBanks, &candidate.digitalOutputFilter8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
#endif /* defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) */

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    if (digitalInputEnabled) {
        result = validateExtensionSlot(candidate.digitalInput8, &device->digitalInput8Extension,
                                       CO_401_INDEX_DIGITAL_INPUT_8, diag);
        if (result != CO_401_INIT_OK) {
            return result;
        }
        result = validateExtensionSlot(candidate.digitalInputFilter8, &device->digitalInputFilter8Extension,
                                       CO_401_INDEX_DIGITAL_INPUT_FILTER_8, diag);
        if (result != CO_401_INIT_OK) {
            return result;
        }
    }
#endif /* defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS) */
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    if (digitalOutputEnabled) {
        result = validateExtensionSlot(candidate.digitalOutput8, &device->digitalOutput8Extension,
                                       CO_401_INDEX_DIGITAL_OUTPUT_8, diag);
        if (result != CO_401_INIT_OK) {
            return result;
        }
    }
#endif /* defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) */

    device->bound = candidate;

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    if (digitalInputEnabled) {
        initializeExtension(&device->digitalInput8Extension, device, OD_writeOriginal);
        initializeExtension(&device->digitalInputFilter8Extension, device, writeDigitalInputFilter);
        (void)OD_extension_init(candidate.digitalInput8, &device->digitalInput8Extension);
        (void)OD_extension_init(candidate.digitalInputFilter8, &device->digitalInputFilter8Extension);
        device->digitalInputFilterDirty = true;
    }
#endif /* defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS) */
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    if (digitalOutputEnabled) {
        initializeExtension(&device->digitalOutput8Extension, device, OD_writeOriginal);
        (void)OD_extension_init(candidate.digitalOutput8, &device->digitalOutput8Extension);
    }
#endif /* defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) */

    device->odBound = true;
    setDiag(diag, CO_401_INIT_OK, 0U, 0U);
    return CO_401_INIT_OK;
}
