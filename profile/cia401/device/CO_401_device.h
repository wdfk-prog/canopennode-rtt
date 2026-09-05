/**
 * @file CO_401_device.h
 * @brief Public Pure-C CiA 401 generic I/O Device core.
 *
 * The core owns no heap memory and has no RT-Thread or BSP dependency. Generated
 * Object Dictionary entries are the source of truth and must exist before the
 * runtime binds them. Stage 1 implements only the mandatory 8-bit digital and
 * 16-bit analogue process images selected by the configured capabilities.
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
    uint8_t digitalInputBanks;   /**< Number of 0x6000 8-bit banks; zero disables the capability. */
    uint8_t digitalOutputBanks;  /**< Number of 0x6200 8-bit banks; zero disables the capability. */
    uint8_t analogInputChannels; /**< Number of 0x6401 INTEGER16 channels; zero disables the capability. */
    uint8_t analogOutputChannels; /**< Number of 0x6411 INTEGER16 channels; zero disables the capability. */
} CO_401_device_config_t;

/** Caller-owned Stage-1 CiA 401 runtime state. */
typedef struct {
    OD_t *od;                    /**< Generated Object Dictionary supplied by the application. */
    CO_401_device_config_t config; /**< Copied immutable process-image/I/O configuration. */
    CO_401_capabilities_t capabilities; /**< Capability set derived from non-zero configured counts. */
    CO_401_device_od_t bound;    /**< Cached generated-OD entries after a successful bind. */
    bool odBound;                /**< True only while the complete Stage-1 OD contract is valid. */
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
 * @brief Validate and cache the complete Stage-1 generated-OD contract.
 *
 * Validation is transactional: the public cache is replaced only after every
 * enabled/disabled capability, Object 0x1000 and ARRAY contract has passed.
 *
 * @param device Initialized Device runtime containing OD/config information.
 * @param diag Optional failure location.
 * @return CO_401_INIT_OK when the generated OD exactly matches the configuration.
 */
CO_401_init_error_t CO_401_device_bindOD(CO_401_device_t *device, CO_401_init_diag_t *diag);

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
 * and are retried on later passes. Stage 1 intentionally has no event masks,
 * scaling, fault override or EMCY policy.
 *
 * @param device Successfully bound Device runtime.
 */
void CO_401_device_process(CO_401_device_t *device);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_401_DEVICE_H */
