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
#define CO_401_DEVICE_TYPE_MULTIPLE_DEVICE_MASK UINT32_C(0xFFFF0000)
#define CO_401_DEVICE_TYPE_PROFILE_MASK UINT32_C(0x0000FFFF)

static uint16_t profileIndex(const CO_401_device_t *device, uint16_t canonicalIndex)
{
    return CO_401_objectIndex(device->logicalDevice, canonicalIndex);
}

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

static CO_401_init_error_t validateDeviceTypeEntry(OD_t *od, uint16_t index, uint32_t expectedValue,
                                                    uint32_t valueMask, OD_entry_t **cache,
                                                    CO_401_init_diag_t *diag)
{
    OD_entry_t *entry = OD_find(od, index);
    OD_IO_t io;
    uint32_t value;

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
    if (io.stream.dataLength != sizeof(value)) {
        setDiag(diag, CO_401_INIT_OD_LENGTH, index, 0U);
        return CO_401_INIT_OD_LENGTH;
    }
    if (!attributesMatch(io.stream.attribute, ODA_SDO_R | ODA_MB, ODA_SDO_R | ODA_MB)) {
        setDiag(diag, CO_401_INIT_OD_ACCESS, index, 0U);
        return CO_401_INIT_OD_ACCESS;
    }
    if (OD_get_u32(entry, 0U, &value, true) != ODR_OK || (value & valueMask) != expectedValue) {
        setDiag(diag, CO_401_INIT_DEVICE_TYPE, index, 0U);
        return CO_401_INIT_DEVICE_TYPE;
    }

    if (cache != NULL) {
        *cache = entry;
    }
    return CO_401_INIT_OK;
}

static CO_401_init_error_t validateDeviceType(CO_401_device_t *device, CO_401_device_od_t *bound,
                                               CO_401_init_diag_t *diag)
{
    const uint32_t expected = CO_401_deviceTypeForCapabilities(device->capabilities);
    const uint16_t logicalTypeIndex = profileIndex(device, CO_401_INDEX_LOGICAL_DEVICE_TYPE);
    OD_entry_t *logicalType = OD_find(device->od, logicalTypeIndex);
    CO_401_init_error_t result;

    /*
     * Standalone CiA 401 devices keep the established Object 0x1000 contract.
     * In a multiple-device module, 0x1000 identifies the module (FFFFh in bits 16..31)
     * while 0x67FF + slot*0x800 owns the exact CiA 401 capability-specific Device type.
     * Slot 0 is treated as multiple-device only when its canonical 0x67FF type is present.
     */
    if (device->logicalDevice == 0U && logicalType == NULL) {
        return validateDeviceTypeEntry(device->od, CO_401_INDEX_DEVICE_TYPE, expected, UINT32_MAX,
                                       &bound->deviceType, diag);
    }

    if (device->logicalDevice == 0U) {
        result = validateDeviceTypeEntry(
            device->od, CO_401_INDEX_DEVICE_TYPE, CO_401_DEVICE_TYPE_MULTIPLE_DEVICE_MASK
                                                   | (expected & CO_401_DEVICE_TYPE_PROFILE_MASK),
            UINT32_MAX, NULL, diag);
    } else {
        result = validateDeviceTypeEntry(device->od, CO_401_INDEX_DEVICE_TYPE,
                                         CO_401_DEVICE_TYPE_MULTIPLE_DEVICE_MASK,
                                         CO_401_DEVICE_TYPE_MULTIPLE_DEVICE_MASK, NULL, diag);
    }
    if (result != CO_401_INIT_OK) {
        return result;
    }

    return validateDeviceTypeEntry(device->od, logicalTypeIndex, expected, UINT32_MAX,
                                   &bound->deviceType, diag);
}

static CO_401_init_error_t validateArrayContract(const CO_401_device_t *device, uint16_t canonicalIndex,
                                                  uint8_t expectedCount, OD_size_t elementLength,
                                                  OD_attr_t requiredAttributes,
                                                  OD_attr_t allowedAttributes, OD_entry_t **cache,
                                                  CO_401_init_diag_t *diag)
{
    const uint16_t index = profileIndex(device, canonicalIndex);
    OD_entry_t *entry = OD_find(device->od, index);
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

static CO_401_init_error_t validateArray(const CO_401_device_t *device, uint16_t canonicalIndex,
                                          uint8_t expectedCount, OD_size_t elementLength,
                                          OD_attr_t expectedAttributes,
                                          OD_entry_t **cache, CO_401_init_diag_t *diag)
{
    return validateArrayContract(device, canonicalIndex, expectedCount, elementLength, expectedAttributes,
                                 expectedAttributes, cache, diag);
}

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS) \
    || defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING)
static CO_401_init_error_t validateOptionalMappingArraySized(const CO_401_device_t *device,
                                                              uint16_t canonicalIndex, uint8_t expectedCount,
                                                              OD_size_t elementLength, OD_attr_t required,
                                                              OD_attr_t allowed, OD_entry_t **cache,
                                                              CO_401_init_diag_t *diag)
{
    return validateArrayContract(device, canonicalIndex, expectedCount, elementLength, required, allowed, cache, diag);
}

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS) \
    || defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
static CO_401_init_error_t validateOptionalMappingArray(const CO_401_device_t *device,
                                                         uint16_t canonicalIndex, uint8_t expectedCount,
                                                         OD_entry_t **cache, CO_401_init_diag_t *diag)
{
    return validateOptionalMappingArraySized(device, canonicalIndex, expectedCount, 1U, ODA_SDO_RW,
                                              ODA_SDO_RW | ODA_TRPDO, cache, diag);
}
#endif /* optional 8-bit mapping arrays */
#endif /* optional mapping-array validators */

static CO_401_init_error_t validateVariable8(const CO_401_device_t *device, uint16_t canonicalIndex,
                                              OD_attr_t requiredAttributes, OD_attr_t allowedAttributes,
                                              OD_entry_t **cache,
                                              CO_401_init_diag_t *diag)
{
    const uint16_t index = profileIndex(device, canonicalIndex);
    OD_entry_t *entry = OD_find(device->od, index);
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
    if (!attributesMatch(io.stream.attribute, requiredAttributes, allowedAttributes)) {
        setDiag(diag, CO_401_INIT_OD_ACCESS, index, 0U);
        return CO_401_INIT_OD_ACCESS;
    }

    *cache = entry;
    return CO_401_INIT_OK;
}

static CO_401_init_error_t validateCapabilityObject(const CO_401_device_t *device, bool enabled,
                                                     uint16_t canonicalIndex, uint8_t expectedCount,
                                                     OD_size_t elementLength,
                                                     OD_attr_t expectedAttributes, OD_entry_t **cache,
                                                     CO_401_init_diag_t *diag)
{
    const uint16_t index = profileIndex(device, canonicalIndex);

    if (!enabled) {
        if (OD_find(device->od, index) != NULL) {
            setDiag(diag, CO_401_INIT_OD_UNEXPECTED, index, 0U);
            return CO_401_INIT_OD_UNEXPECTED;
        }
        *cache = NULL;
        return CO_401_INIT_OK;
    }

    return validateArray(device, canonicalIndex, expectedCount, elementLength, expectedAttributes, cache, diag);
}

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS) \
    || defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
static CO_401_init_error_t validateOptionalArray(const CO_401_device_t *device, bool enabled,
                                                  uint16_t canonicalIndex, uint8_t expectedCount,
                                                  OD_entry_t **cache,
                                                  CO_401_init_diag_t *diag)
{
    const uint16_t index = profileIndex(device, canonicalIndex);

    if (!enabled) {
        if (OD_find(device->od, index) != NULL) {
            setDiag(diag, CO_401_INIT_OD_UNEXPECTED, index, 0U);
            return CO_401_INIT_OD_UNEXPECTED;
        }
        *cache = NULL;
        return CO_401_INIT_OK;
    }

    return validateOptionalMappingArray(device, canonicalIndex, expectedCount, cache, diag);
}
#endif /* optional 8-bit arrays */

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE) \
    || defined(PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING)
static CO_401_init_error_t validateOptionalArraySized(const CO_401_device_t *device, bool enabled,
                                                       uint16_t canonicalIndex, uint8_t expectedCount,
                                                       OD_size_t elementLength,
                                                       OD_attr_t required, OD_attr_t allowed,
                                                       OD_entry_t **cache, CO_401_init_diag_t *diag)
{
    const uint16_t index = profileIndex(device, canonicalIndex);

    if (!enabled) {
        if (OD_find(device->od, index) != NULL) {
            setDiag(diag, CO_401_INIT_OD_UNEXPECTED, index, 0U);
            return CO_401_INIT_OD_UNEXPECTED;
        }
        *cache = NULL;
        return CO_401_INIT_OK;
    }
    return validateOptionalMappingArraySized(device, canonicalIndex, expectedCount, elementLength, required, allowed,
                                              cache, diag);
}

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING)
static CO_401_init_error_t validatePresentOptionalArraySized(const CO_401_device_t *device, bool enabled,
                                                              uint16_t canonicalIndex, uint8_t expectedCount,
                                                              OD_size_t elementLength,
                                                              OD_attr_t required, OD_attr_t allowed,
                                                              OD_entry_t **cache, CO_401_init_diag_t *diag)
{
    const uint16_t index = profileIndex(device, canonicalIndex);

    if (!enabled) {
        if (OD_find(device->od, index) != NULL) {
            setDiag(diag, CO_401_INIT_OD_UNEXPECTED, index, 0U);
            return CO_401_INIT_OD_UNEXPECTED;
        }
        *cache = NULL;
        return CO_401_INIT_OK;
    }
    if (OD_find(device->od, index) == NULL) {
        *cache = NULL;
        return CO_401_INIT_OK;
    }
    return validateOptionalMappingArraySized(device, canonicalIndex, expectedCount, elementLength, required, allowed,
                                              cache, diag);
}
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING */
#endif /* analogue optional objects */

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


#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
static bool isSdoRead(const CO_401_device_t *device, const OD_stream_t *stream)
{
    return device != NULL && stream != NULL && device->sdoReadMatch != NULL
        && device->sdoReadMatch(device->sdoReadMatchObject, stream);
}

static ODR_t readAnalogInput16(OD_stream_t *stream, void *buf, OD_size_t count, OD_size_t *countRead)
{
    CO_401_device_t *device = stream != NULL ? stream->object : NULL;
    ODR_t result = OD_readOriginal(stream, buf, count, countRead);

    if (result == ODR_OK && stream != NULL && stream->subIndex != 0U && countRead != NULL
        && *countRead == sizeof(int16_t) && isSdoRead(device, stream)) {
        int16_t communicated;

        /* OD_readOriginal() still exposes native OD byte order here; SDO endian conversion happens afterwards. */
        (void)memcpy(&communicated, buf, sizeof(communicated));
        CO_401_device_commitAnalogInputCommunication(device, stream->subIndex, communicated);
    }
    return result;
}

static ODR_t readAnalogInterruptSource(OD_stream_t *stream, void *buf, OD_size_t count, OD_size_t *countRead)
{
    CO_401_device_t *device = stream != NULL ? stream->object : NULL;
    ODR_t result = OD_readOriginal(stream, buf, count, countRead);

    if (result == ODR_OK && stream != NULL && stream->subIndex != 0U && countRead != NULL
        && *countRead == sizeof(uint32_t) && isSdoRead(device, stream)) {
        uint32_t communicatedBits;

        /* SDO consumes the 0x6422 snapshot at OD-read completion, independent of later CAN transmission success. */
        (void)memcpy(&communicatedBits, buf, sizeof(communicatedBits));
        CO_401_device_commitAnalogSourceCommunication(device, stream->subIndex, communicatedBits);
    }
    return result;
}
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */

static ODR_t writeOutputCommand(OD_stream_t *stream, const void *buf, OD_size_t count, OD_size_t *countWritten)
{
    CO_401_device_t *device;

    if (stream == NULL || (device = stream->object) == NULL) {
        return ODR_DEV_INCOMPAT;
    }
    if (stream->subIndex != 0U && !device->outputSupervisionReady) {
        /*
         * Close the scheduler window between the qualifying Heartbeat/guard event and
         * this network write. The probe runs under the caller's OD serialization and
         * may only publish the monotonic ready latch; it must not acquire locks itself.
         */
        if (device->outputSupervisionProbe != NULL
            && device->outputSupervisionProbe(device->outputSupervisionProbeObject)) {
            CO_401_device_notifyOutputSupervision(device);
        }
        if (!device->outputSupervisionReady) {
            if (countWritten != NULL) {
                *countWritten = 0U;
            }
            /* SDO gets the standard present-device-state abort; RPDO ignores the rejected write result. */
            return ODR_DATA_DEV_STATE;
        }
    }
    return OD_writeOriginal(stream, buf, count, countWritten);
}

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
                                ODR_t (*read)(OD_stream_t *, void *, OD_size_t, OD_size_t *),
                                ODR_t (*write)(OD_stream_t *, const void *, OD_size_t, OD_size_t *))
{
    (void)memset(extension, 0, sizeof(*extension));
    extension->object = object;
    extension->read = read;
    extension->write = write;
#if OD_FLAGS_PDO_SIZE > 0
    /* OD_requestTPDO() requests transmission by clearing a bit, so start with no pending requests. */
    (void)memset(extension->flagsPDO, 0xFF, sizeof(extension->flagsPDO));
#endif /* OD_FLAGS_PDO_SIZE > 0 */
}

static void detachOwnedExtensions(CO_401_device_t *device)
{
    if (device->bound.digitalOutput8 != NULL
        && device->bound.digitalOutput8->extension == &device->digitalOutput8Extension) {
        (void)OD_extension_init(device->bound.digitalOutput8, NULL);
    }
    if (device->bound.analogOutput16 != NULL
        && device->bound.analogOutput16->extension == &device->analogOutput16Extension) {
        (void)OD_extension_init(device->bound.analogOutput16, NULL);
    }
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
    (void)memset(device->digitalInputEventTpdoPending, 0, sizeof(device->digitalInputEventTpdoPending));
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS */
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
    if (device->bound.analogInput16 != NULL
        && device->bound.analogInput16->extension == &device->analogInput16Extension) {
        (void)OD_extension_init(device->bound.analogInput16, NULL);
    }
    if (device->bound.analogInterruptSource != NULL
        && device->bound.analogInterruptSource->extension == &device->analogInterruptSourceExtension) {
        (void)OD_extension_init(device->bound.analogInterruptSource, NULL);
    }
    /* Communication-generation-local delta references and unsent event retries never cross an OD rebind. */
    (void)memset(device->analogLastCommunicatedValid, 0, sizeof(device->analogLastCommunicatedValid));
    (void)memset(device->analogInputEventTpdoPending, 0, sizeof(device->analogInputEventTpdoPending));
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */
}

CO_401_init_error_t CO_401_device_bindOD(CO_401_device_t *device, CO_401_init_diag_t *diag)
{
    CO_401_device_od_t candidate;
    CO_401_init_error_t result;
    const bool digitalInputEnabled = device != NULL && device->config.digitalInputBanks != 0U;
    const bool digitalOutputEnabled = device != NULL && device->config.digitalOutputBanks != 0U;
    const bool analogInputEnabled = device != NULL && device->config.analogInputChannels != 0U;

    if (diag != NULL) {
        diag->logicalDevice = device != NULL ? device->logicalDevice : 0U;
    }
    if (device == NULL) {
        setDiag(diag, CO_401_INIT_BAD_ARGUMENT, 0U, 0U);
        return CO_401_INIT_BAD_ARGUMENT;
    }

    /* A failed rebind must not leave forwarding hooks from the previous communication generation reachable. */
    detachOwnedExtensions(device);
    device->odBound = false;
    if (device->od == NULL) {
        setDiag(diag, CO_401_INIT_BAD_ARGUMENT, 0U, 0U);
        return CO_401_INIT_BAD_ARGUMENT;
    }
    (void)memset(&candidate, 0, sizeof(candidate));

    result = validateDeviceType(device, &candidate, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }

    result = validateCapabilityObject(device, digitalInputEnabled, CO_401_INDEX_DIGITAL_INPUT_8,
                                      device->config.digitalInputBanks, 1U, ODA_SDO_R | ODA_TPDO,
                                      &candidate.digitalInput8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
    result = validateCapabilityObject(device, digitalOutputEnabled, CO_401_INDEX_DIGITAL_OUTPUT_8,
                                      device->config.digitalOutputBanks, 1U, ODA_SDO_RW | ODA_RPDO,
                                      &candidate.digitalOutput8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
    result = validateCapabilityObject(device, analogInputEnabled,
                                      CO_401_INDEX_ANALOG_INPUT_16, device->config.analogInputChannels,
                                      2U, ODA_SDO_R | ODA_TPDO | ODA_MB, &candidate.analogInput16, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }

    /* CiA 401 0x6423 is Conditional: Device with analogue input, not an optional comparator feature. */
    if (analogInputEnabled) {
        result = validateVariable8(device, CO_401_INDEX_ANALOG_INTERRUPT_ENABLE, ODA_SDO_RW,
                                   ODA_SDO_RW | ODA_TRPDO, &candidate.analogInterruptEnable, diag);
    } else {
        if (OD_find(device->od, profileIndex(device, CO_401_INDEX_ANALOG_INTERRUPT_ENABLE)) != NULL) {
            setDiag(diag, CO_401_INIT_OD_UNEXPECTED, profileIndex(device, CO_401_INDEX_ANALOG_INTERRUPT_ENABLE), 0U);
            return CO_401_INIT_OD_UNEXPECTED;
        }
        candidate.analogInterruptEnable = NULL;
        result = CO_401_INIT_OK;
    }
    if (result != CO_401_INIT_OK) {
        return result;
    }

    result = validateCapabilityObject(device, device->config.analogOutputChannels != 0U,
                                      CO_401_INDEX_ANALOG_OUTPUT_16, device->config.analogOutputChannels,
                                      2U, ODA_SDO_RW | ODA_RPDO | ODA_MB, &candidate.analogOutput16, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    result = validateOptionalArray(device, digitalInputEnabled, CO_401_INDEX_DIGITAL_INPUT_POLARITY_8,
                                   device->config.digitalInputBanks, &candidate.digitalInputPolarity8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
    result = validateOptionalArray(device, digitalInputEnabled, CO_401_INDEX_DIGITAL_INPUT_FILTER_8,
                                   device->config.digitalInputBanks, &candidate.digitalInputFilter8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
    if (digitalInputEnabled) {
        result = validateVariable8(device, CO_401_INDEX_DIGITAL_INTERRUPT_ENABLE,
                                   ODA_SDO_RW, ODA_SDO_RW, &candidate.digitalInterruptEnable, diag);
    } else if (OD_find(device->od, profileIndex(device, CO_401_INDEX_DIGITAL_INTERRUPT_ENABLE)) != NULL) {
        setDiag(diag, CO_401_INIT_OD_UNEXPECTED, profileIndex(device, CO_401_INDEX_DIGITAL_INTERRUPT_ENABLE), 0U);
        return CO_401_INIT_OD_UNEXPECTED;
    }
    if (result != CO_401_INIT_OK) {
        return result;
    }
    result = validateOptionalArray(device, digitalInputEnabled, CO_401_INDEX_DIGITAL_INTERRUPT_ANY_8,
                                   device->config.digitalInputBanks, &candidate.digitalInterruptAny8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
    result = validateOptionalArray(device, digitalInputEnabled, CO_401_INDEX_DIGITAL_INTERRUPT_RISING_8,
                                   device->config.digitalInputBanks, &candidate.digitalInterruptRising8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
    result = validateOptionalArray(device, digitalInputEnabled, CO_401_INDEX_DIGITAL_INTERRUPT_FALLING_8,
                                   device->config.digitalInputBanks, &candidate.digitalInterruptFalling8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
#endif /* defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS) */

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    result = validateOptionalArray(device, digitalOutputEnabled, CO_401_INDEX_DIGITAL_OUTPUT_POLARITY_8,
                                   device->config.digitalOutputBanks, &candidate.digitalOutputPolarity8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
    result = validateOptionalArray(device, digitalOutputEnabled, CO_401_INDEX_DIGITAL_OUTPUT_ERROR_MODE_8,
                                   device->config.digitalOutputBanks, &candidate.digitalOutputErrorMode8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
    result = validateOptionalArray(device, digitalOutputEnabled, CO_401_INDEX_DIGITAL_OUTPUT_ERROR_VALUE_8,
                                   device->config.digitalOutputBanks, &candidate.digitalOutputErrorValue8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
    result = validateOptionalArray(device, digitalOutputEnabled, CO_401_INDEX_DIGITAL_OUTPUT_FILTER_8,
                                   device->config.digitalOutputBanks, &candidate.digitalOutputFilter8, diag);
    if (result != CO_401_INIT_OK) {
        return result;
    }
#endif /* defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE) */

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
    {
        const uint8_t sourceBanks = (uint8_t)((device->config.analogInputChannels + 31U) / 32U);

        result = validateOptionalArray(device, analogInputEnabled, CO_401_INDEX_ANALOG_INTERRUPT_TRIGGER,
                                       device->config.analogInputChannels, &candidate.analogInterruptTrigger, diag);
        if (result != CO_401_INIT_OK) return result;
        result = validateOptionalArraySized(device, analogInputEnabled, CO_401_INDEX_ANALOG_INTERRUPT_SOURCE,
                                            sourceBanks, 4U, ODA_SDO_R, ODA_SDO_R | ODA_TPDO | ODA_MB,
                                            &candidate.analogInterruptSource, diag);
        if (result != CO_401_INIT_OK) return result;
        result = validateOptionalArraySized(device, analogInputEnabled, CO_401_INDEX_ANALOG_INTERRUPT_UPPER_32,
                                            device->config.analogInputChannels, 4U, ODA_SDO_RW,
                                            ODA_SDO_RW | ODA_TRPDO | ODA_MB, &candidate.analogInterruptUpper32, diag);
        if (result != CO_401_INIT_OK) return result;
        result = validateOptionalArraySized(device, analogInputEnabled, CO_401_INDEX_ANALOG_INTERRUPT_LOWER_32,
                                            device->config.analogInputChannels, 4U, ODA_SDO_RW,
                                            ODA_SDO_RW | ODA_TRPDO | ODA_MB, &candidate.analogInterruptLower32, diag);
        if (result != CO_401_INIT_OK) return result;
        result = validateOptionalArraySized(device, analogInputEnabled, CO_401_INDEX_ANALOG_INTERRUPT_DELTA_U32,
                                            device->config.analogInputChannels, 4U, ODA_SDO_RW,
                                            ODA_SDO_RW | ODA_TRPDO | ODA_MB, &candidate.analogInterruptDeltaU32, diag);
        if (result != CO_401_INIT_OK) return result;
        result = validateOptionalArraySized(device, analogInputEnabled, CO_401_INDEX_ANALOG_INTERRUPT_NEG_DELTA_U32,
                                            device->config.analogInputChannels, 4U, ODA_SDO_RW,
                                            ODA_SDO_RW | ODA_TRPDO | ODA_MB, &candidate.analogInterruptNegDeltaU32, diag);
        if (result != CO_401_INIT_OK) return result;
        result = validateOptionalArraySized(device, analogInputEnabled, CO_401_INDEX_ANALOG_INTERRUPT_POS_DELTA_U32,
                                            device->config.analogInputChannels, 4U, ODA_SDO_RW,
                                            ODA_SDO_RW | ODA_TRPDO | ODA_MB, &candidate.analogInterruptPosDeltaU32, diag);
        if (result != CO_401_INIT_OK) return result;
    }
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    result = validateOptionalArray(device, device->config.analogOutputChannels != 0U,
                                   CO_401_INDEX_ANALOG_OUTPUT_ERROR_MODE, device->config.analogOutputChannels,
                                   &candidate.analogOutputErrorMode, diag);
    if (result != CO_401_INIT_OK) return result;
    result = validateOptionalArraySized(device, device->config.analogOutputChannels != 0U,
                                        CO_401_INDEX_ANALOG_OUTPUT_ERROR_VALUE_32, device->config.analogOutputChannels,
                                        4U, ODA_SDO_RW, ODA_SDO_RW | ODA_TRPDO | ODA_MB,
                                        &candidate.analogOutputErrorValue32, diag);
    if (result != CO_401_INIT_OK) return result;
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING)
    /* 0x6430/0x6450 are Optional in CiA 401; validate their OD contract only when the object is present. */
    result = validatePresentOptionalArraySized(device, device->config.analogInputChannels != 0U,
                                               CO_401_INDEX_ANALOG_INPUT_SI_UNIT,
                                               device->config.analogInputChannels, 4U, ODA_SDO_RW,
                                               ODA_SDO_RW | ODA_TRPDO | ODA_MB,
                                               &candidate.analogInputSiUnit, diag);
    if (result != CO_401_INIT_OK) return result;
    result = validateOptionalArraySized(device, device->config.analogInputChannels != 0U,
                                        CO_401_INDEX_ANALOG_INPUT_OFFSET_32, device->config.analogInputChannels,
                                        4U, ODA_SDO_RW, ODA_SDO_RW | ODA_TRPDO | ODA_MB,
                                        &candidate.analogInputOffset32, diag);
    if (result != CO_401_INIT_OK) return result;
    result = validateOptionalArraySized(device, device->config.analogInputChannels != 0U,
                                        CO_401_INDEX_ANALOG_INPUT_PRESCALING_32, device->config.analogInputChannels,
                                        4U, ODA_SDO_RW, ODA_SDO_RW | ODA_TRPDO | ODA_MB,
                                        &candidate.analogInputPrescaling32, diag);
    if (result != CO_401_INIT_OK) return result;
    result = validateOptionalArraySized(device, device->config.analogOutputChannels != 0U,
                                        CO_401_INDEX_ANALOG_OUTPUT_OFFSET_32, device->config.analogOutputChannels,
                                        4U, ODA_SDO_RW, ODA_SDO_RW | ODA_TRPDO | ODA_MB,
                                        &candidate.analogOutputOffset32, diag);
    if (result != CO_401_INIT_OK) return result;
    result = validateOptionalArraySized(device, device->config.analogOutputChannels != 0U,
                                        CO_401_INDEX_ANALOG_OUTPUT_SCALING_32, device->config.analogOutputChannels,
                                        4U, ODA_SDO_RW, ODA_SDO_RW | ODA_TRPDO | ODA_MB,
                                        &candidate.analogOutputScaling32, diag);
    if (result != CO_401_INIT_OK) return result;
    result = validatePresentOptionalArraySized(device, device->config.analogOutputChannels != 0U,
                                               CO_401_INDEX_ANALOG_OUTPUT_SI_UNIT,
                                               device->config.analogOutputChannels, 4U, ODA_SDO_RW,
                                               ODA_SDO_RW | ODA_TRPDO | ODA_MB,
                                               &candidate.analogOutputSiUnit, diag);
    if (result != CO_401_INIT_OK) return result;
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING */

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    if (digitalInputEnabled) {
        result = validateExtensionSlot(candidate.digitalInput8, &device->digitalInput8Extension,
                                       profileIndex(device, CO_401_INDEX_DIGITAL_INPUT_8), diag);
        if (result != CO_401_INIT_OK) {
            return result;
        }
        result = validateExtensionSlot(candidate.digitalInputFilter8, &device->digitalInputFilter8Extension,
                                       profileIndex(device, CO_401_INDEX_DIGITAL_INPUT_FILTER_8), diag);
        if (result != CO_401_INIT_OK) {
            return result;
        }
    }
#endif /* defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS) */
    if (digitalOutputEnabled) {
        result = validateExtensionSlot(candidate.digitalOutput8, &device->digitalOutput8Extension,
                                       profileIndex(device, CO_401_INDEX_DIGITAL_OUTPUT_8), diag);
        if (result != CO_401_INIT_OK) {
            return result;
        }
    }
    if (device->config.analogOutputChannels != 0U) {
        result = validateExtensionSlot(candidate.analogOutput16, &device->analogOutput16Extension,
                                       profileIndex(device, CO_401_INDEX_ANALOG_OUTPUT_16), diag);
        if (result != CO_401_INIT_OK) {
            return result;
        }
    }

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
    if (analogInputEnabled) {
        result = validateExtensionSlot(candidate.analogInput16, &device->analogInput16Extension,
                                       profileIndex(device, CO_401_INDEX_ANALOG_INPUT_16), diag);
        if (result != CO_401_INIT_OK) return result;
        result = validateExtensionSlot(candidate.analogInterruptSource, &device->analogInterruptSourceExtension,
                                       profileIndex(device, CO_401_INDEX_ANALOG_INTERRUPT_SOURCE), diag);
        if (result != CO_401_INIT_OK) return result;
    }
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */

    device->bound = candidate;

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    if (digitalInputEnabled) {
        initializeExtension(&device->digitalInput8Extension, device, OD_readOriginal, OD_writeOriginal);
        initializeExtension(&device->digitalInputFilter8Extension, device, OD_readOriginal, writeDigitalInputFilter);
        (void)OD_extension_init(candidate.digitalInput8, &device->digitalInput8Extension);
        (void)OD_extension_init(candidate.digitalInputFilter8, &device->digitalInputFilter8Extension);
        device->digitalInputFilterDirty = true;
        (void)memset(device->digitalInputEventTpdoPending, 0, sizeof(device->digitalInputEventTpdoPending));
    }
#endif /* defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS) */
    if (digitalOutputEnabled) {
        initializeExtension(&device->digitalOutput8Extension, device, OD_readOriginal, writeOutputCommand);
        (void)OD_extension_init(candidate.digitalOutput8, &device->digitalOutput8Extension);
    }
    if (device->config.analogOutputChannels != 0U) {
        initializeExtension(&device->analogOutput16Extension, device, OD_readOriginal, writeOutputCommand);
        (void)OD_extension_init(candidate.analogOutput16, &device->analogOutput16Extension);
    }

#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
    if (analogInputEnabled) {
        initializeExtension(&device->analogInput16Extension, device, readAnalogInput16, OD_writeOriginal);
        initializeExtension(&device->analogInterruptSourceExtension, device, readAnalogInterruptSource,
                            OD_writeOriginal);
        (void)OD_extension_init(candidate.analogInput16, &device->analogInput16Extension);
        (void)OD_extension_init(candidate.analogInterruptSource, &device->analogInterruptSourceExtension);
        /* Publish the new OD generation with no communication baseline or inherited retry state. */
        (void)memset(device->analogLastCommunicatedValid, 0, sizeof(device->analogLastCommunicatedValid));
        (void)memset(device->analogInputEventTpdoPending, 0, sizeof(device->analogInputEventTpdoPending));
    }
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */

    device->odBound = true;
    setDiag(diag, CO_401_INIT_OK, 0U, 0U);
    return CO_401_INIT_OK;
}
