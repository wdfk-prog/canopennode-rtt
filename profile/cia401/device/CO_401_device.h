/**
 * @file CO_401_device.h
 * @brief Public Pure-C CiA 401 generic I/O Device core.
 *
 * The core owns no heap memory and has no RT-Thread or BSP dependency. Generated
 * Object Dictionary entries are the source of truth and must exist before the
 * runtime binds them. The mandatory core supports 8-bit digital and 16-bit
 * analogue process images; optional Stage-2 digital semantics are selected by
 * Kconfig.
 */

#ifndef CO_401_DEVICE_H
#define CO_401_DEVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "301/CO_ODinterface.h"
#include "CO_401_defs.h"
#include "CO_401_device_od.h"
#include "CO_401_io.h"
#include "CO_401_objects.h"
#include "CO_401_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Fail-closed result of Device configuration and generated-OD validation. */
typedef enum {
    CO_401_INIT_OK = 0,
    CO_401_INIT_BAD_ARGUMENT,
    CO_401_INIT_CONFIG,
    CO_401_INIT_IO_IF,
    CO_401_INIT_OD_MISSING,
    CO_401_INIT_OD_UNEXPECTED,
    CO_401_INIT_OD_TYPE,
    CO_401_INIT_OD_LENGTH,
    CO_401_INIT_OD_ACCESS,
    CO_401_INIT_OD_SUB_COUNT,
    CO_401_INIT_OD_SUB_VALUE,
    CO_401_INIT_DEVICE_TYPE
} CO_401_init_error_t;

/** Location associated with a CiA 401 initialization failure. */
typedef struct {
    CO_401_init_error_t error;
    uint16_t index;
    uint8_t subIndex;
} CO_401_init_diag_t;

/** Immutable product configuration copied into the Device runtime during initialization. */
typedef struct {
    const CO_401_io_if_t *io; /**< Persistent callback table for the enabled capabilities. */
    void *ioObject;           /**< Product-owned callback context. */
    uint8_t digitalInputBanks;   /**< Number of 0x6000 8-bit banks; zero disables the capability.
                                   *   With digital events enabled, the highest sub-index must fit
                                   *   in CANopenNode's OD_FLAGS_PDO_SIZE request bitmap. */
    uint8_t digitalOutputBanks;  /**< Number of 0x6200 8-bit banks; zero disables the capability. */
    uint8_t analogInputChannels; /**< Number of 0x6401 INTEGER16 channels; zero disables the capability. */
    uint8_t analogOutputChannels; /**< Number of 0x6411 INTEGER16 channels; zero disables the capability. */
} CO_401_device_config_t;

/** Caller-owned CiA 401 runtime state. */
typedef struct {
    OD_t *od;                    /**< Generated Object Dictionary supplied by the application. */
    CO_401_device_config_t config; /**< Copied immutable process-image/I/O configuration. */
    CO_401_capabilities_t capabilities; /**< Capability set derived from non-zero configured counts. */
    CO_401_device_od_t bound;    /**< Cached generated-OD entries after a successful bind. */
    bool odBound;                /**< True only while the complete enabled OD contract is valid. */
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    OD_extension_t digitalInput8Extension; /**< TPDO request flags for Object 0x6000. */
    OD_extension_t digitalInputFilter8Extension; /**< Forwarding hook for Object 0x6003 writes. */
    bool digitalInputFilterDirty; /**< Product filter bridge must be refreshed on the next process pass. */
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS */
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    OD_extension_t digitalOutput8Extension; /**< OD I/O hook retained for Object 0x6200 mapped writes. */
    bool digitalOutputFaultActive; /**< Selects 0x6206/0x6207 physical fail-safe processing. */
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE */
} CO_401_device_t;

/**
 * @brief Initialize one local CiA 401 generic I/O Device and bind its generated OD.
 *
 * @param device Caller-owned runtime storage.
 * @param od Generated CANopenNode Object Dictionary.
 * @param config Product capability and IOIF configuration.
 * @param diag Optional failure location.
 * @return CO_401_INIT_OK on success; otherwise the runtime remains fail-closed.
 */
CO_401_init_error_t CO_401_device_init(CO_401_device_t *device, OD_t *od,
                                        const CO_401_device_config_t *config, CO_401_init_diag_t *diag);

/**
 * @brief Validate and cache the complete enabled generated-OD contract.
 *
 * Validation is transactional: the public cache is replaced only after every
 * enabled/disabled capability, Object 0x1000 and required ARRAY/VAR contract
 * has passed. When Stage-2 digital options are enabled, this function also
 * installs OD extensions on 0x6000/0x6003/0x6200. It must therefore run before
 * PDO initialization so PDO mapping caches the extension I/O and TPDO flags.
 *
 * @param device Initialized Device runtime containing OD/config information.
 * @param diag Optional failure location.
 * @return CO_401_INIT_OK when the generated OD exactly matches the configuration.
 */
CO_401_init_error_t CO_401_device_bindOD(CO_401_device_t *device, CO_401_init_diag_t *diag);

#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
/**
 * @brief Select or clear the CiA 401 digital-output fault state.
 *
 * When active, 0x6206 selects per-bit keep-current versus 0x6207 error-value
 * behavior. Object 0x6200 remains the logical command image and is not replaced
 * by the fail-safe value. The product masked-write callback preserves physical
 * keep-current bits. The bridge that observes internal device failure or Stop
 * Remote Node is responsible for calling this function.
 *
 * The caller must serialize this state change with CO_401_device_process() and
 * direct output-helper calls.
 *
 * @param device Device runtime; NULL is ignored.
 * @param active True while digital outputs shall use fault behavior.
 */
void CO_401_device_setDigitalOutputFault(CO_401_device_t *device, bool active);
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE */

/**
 * @brief Execute one bounded process-image exchange with the product IOIF.
 *
 * The caller must serialize this function against concurrent SDO/PDO access to
 * the same mapped OD entries. IOIF callbacks execute inside that caller-owned
 * serialization window and must not recursively acquire it.
 *
 * Successful DI/AI reads refresh 0x6000/0x6401. DO/AO command values are read
 * from 0x6200/0x6411 and offered to the backend on each pass. BUSY/ERROR input
 * operations preserve the last OD input image; output commands remain in the OD
 * and are retried on later passes. Optional Stage-2 digital input events apply
 * 0x6002/0x6003/0x6005..0x6008 and request TPDO transmission through the mapped
 * 0x6000 OD entry. Optional digital output semantics keep the full 0x6200 command
 * image, then apply 0x6206/0x6207, 0x6202 and finally the 0x6208 mask to the
 * physical output path.
 *
 * @param device Successfully bound Device runtime.
 */
void CO_401_device_process(CO_401_device_t *device);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_401_DEVICE_H */
