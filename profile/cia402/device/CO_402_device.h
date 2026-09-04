/**
 * @file CO_402_device.h
 * @brief Public Pure-C multi-axis CiA 402 Device core.
 *
 * The Device core owns no heap memory and has no RT-Thread or BSP dependency.
 * Applications provide persistent manager, axis configuration and axis runtime
 * storage. Object Dictionary entries must already exist in the generated OD.
 */

#ifndef CO_402_DEVICE_H
#define CO_402_DEVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "301/CO_ODinterface.h"
#include "CO_402_defs.h"
#include "CO_402_device_od.h"
#include "CO_402_drive.h"
#include "CO_402_mode.h"
#include "CO_402_objects.h"
#include "CO_402_state.h"

#if CO_402_CONFIG_MODE_PP
#include "modes/CO_402_mode_pp.h"
#endif /* CO_402_CONFIG_MODE_PP */
#if CO_402_CONFIG_MODE_PV
#include "modes/CO_402_mode_pv.h"
#endif /* CO_402_CONFIG_MODE_PV */
#if CO_402_CONFIG_MODE_HM
#include "modes/CO_402_mode_hm.h"
#endif /* CO_402_CONFIG_MODE_HM */

#ifdef __cplusplus
extern "C" {
#endif

/** Result of manager configuration and OD contract validation. */
typedef enum {
    CO_402_INIT_OK = 0,                 /**< Configuration and OD binding are valid. */
    CO_402_INIT_BAD_AXIS,               /**< Logical-device index or DriveIF is invalid. */
    CO_402_INIT_DUPLICATE_AXIS,         /**< Two configurations select the same logical device. */
    CO_402_INIT_OD_MISSING,             /**< Required generated OD entry/sub-index is missing. */
    CO_402_INIT_OD_LENGTH,              /**< Required OD object has an unexpected width. */
    CO_402_INIT_OD_ACCESS,              /**< Required SDO/PDO/MB attributes do not match the runtime contract. */
    CO_402_INIT_OD_EXTENSION_CONFLICT,  /**< Another subsystem already owns a required OD extension. */
    CO_402_INIT_MODE_OD_MISMATCH,       /**< Enabled/supported mode is missing one of its required OD objects. */
    CO_402_INIT_CONFIG_MISMATCH,        /**< Manager storage or axis-count configuration is inconsistent. */
    CO_402_INIT_OD_TYPE                 /**< Required OD entry has an unexpected VAR/RECORD object type. */
} CO_402_init_error_t;

/** Detailed location associated with an initialization error. */
typedef struct {
    CO_402_init_error_t error; /**< Error category returned by the initialization or OD binding operation. */
    uint8_t logicalDevice;     /**< Zero-based logical device that caused the error. */
    uint16_t index;            /**< OD index related to the error, or zero for configuration-only failures. */
    uint8_t subIndex;          /**< OD sub-index related to the error. */
} CO_402_init_diag_t;

/** Configuration for one local logical-device axis. */
typedef struct {
    uint8_t logicalDevice;           /**< Zero-based logical-device number used to derive the profile OD block. */
    const CO_402_drive_if_t *drive;  /**< Persistent non-blocking hardware abstraction for this axis. */
    void *driveObject;               /**< Product-owned object passed unchanged to every DriveIF callback. */
} CO_402_device_axis_config_t;

/** Runtime state for one local logical-device axis. */
typedef struct CO_402_device_axis {
    uint8_t logicalDevice;                  /**< Zero-based logical-device number owned by this runtime axis. */
    uint16_t odBase;                        /**< Base index of this logical device's standardized profile block. */
    CO_402_state_t state;                   /**< Current symbolic PDS state. */
    CO_402_mode_t mode;                     /**< Currently active semantic operation mode. */
    CO_402_mode_raw_t requestedModeRaw;     /**< Last INTEGER8 value read from Modes of operation. */
    bool requestedModeRecognized;           /**< True when requestedModeRaw maps to a known semantic mode. */

    CO_402_device_od_t od;                  /**< Cached generated-OD entries and axis-owned OD extensions. */
    const CO_402_drive_if_t *drive;         /**< Persistent DriveIF selected by the immutable axis config. */
    void *driveObject;                      /**< Product-owned DriveIF context; lifetime must cover the manager. */

    /* Keep PDS edge state in its established position so published member offsets do not move. */
    bool faultResetBitPrevious;             /**< Last sampled bit 7; retained across all PDS states for edge detection. */
    bool faultResetInProgress;              /**< Accepted reset edge remains latched while the DriveIF returns BUSY. */

    /* Mode runtime remains after the established PDS fields so previously published member offsets stay unchanged. */
    CO_402_mode_t pendingExitMode;           /**< Mode whose BUSY exit must finish before another request can run. */
    CO_402_mode_t pendingEnterMode;          /**< Mode whose BUSY entry must finish before a newer 0x6060 request is consumed. */
    uint32_t supportedModes;                 /**< 0x6502 mask for modes enabled by build configuration and this axis DriveIF. */

#if CO_402_CONFIG_MODE_PP
    CO_402_mode_pp_t pp;                    /**< PP edge-handshake and accepted set-point ownership state. */
#endif /* CO_402_CONFIG_MODE_PP */
#if CO_402_CONFIG_MODE_PV
    CO_402_mode_pv_t pv;                    /**< PV result state for the command sampled on the current supervisor pass. */
#endif /* CO_402_CONFIG_MODE_PV */
#if CO_402_CONFIG_MODE_HM
    CO_402_mode_hm_t hm;                    /**< HM start/abort ownership and completion/error state. */
#endif /* CO_402_CONFIG_MODE_HM */
} CO_402_device_axis_t;

/** Multi-axis Device manager. */
typedef struct CO_402_device {
    OD_t *od;                                      /**< Generated Object Dictionary shared by all local axes. */
    CO_402_device_axis_t *axes;                    /**< Persistent caller-owned runtime array. */
    const CO_402_device_axis_config_t *configs;    /**< Persistent caller-owned axis configuration array. */
    uint8_t axisCount;                             /**< Number of valid entries in axes/configs. */
    bool odBound;                                  /**< True after all required OD entries/extensions are bound. */
} CO_402_device_t;

/** Role-oriented alias for code that treats the Device object as a multi-axis manager. */
typedef CO_402_device_t CO_402_device_manager_t;

/** Zero-based logical-device/axis index. */
typedef uint8_t CO_402_device_axis_index_t;

/**
 * @brief Initialize a multi-axis Device manager and bind its generated OD.
 *
 * @param manager Persistent manager storage supplied by the application.
 * @param od Generated CANopenNode Object Dictionary.
 * @param axes Persistent axis runtime array with @p axisCount elements.
 * @param configs Persistent axis configuration array with @p axisCount elements.
 * @param axisCount Number of configured logical devices.
 * @param diag Optional initialization diagnostic destination.
 * @return Initialization result. CO_402_INIT_OK means configuration and OD binding succeeded.
 */
CO_402_init_error_t CO_402_device_managerInit(CO_402_device_manager_t *manager, OD_t *od,
                                               CO_402_device_axis_t *axes,
                                               const CO_402_device_axis_config_t *configs,
                                               uint8_t axisCount, CO_402_init_diag_t *diag);

/**
 * @brief Validate and bind all configured axis Object Dictionary entries.
 *
 * The function validates every axis before installing any forwarding OD
 * extension, so a validation failure does not leave a partially bound manager.
 * Mode-specific objects are required only for modes which are compiled in
 * and actually advertised by the axis DriveIF.
 *
 * @param manager Initialized manager with valid axis/config storage.
 * @param diag Optional initialization diagnostic destination.
 * @return OD binding result.
 */
CO_402_init_error_t CO_402_device_bindOD(CO_402_device_manager_t *manager, CO_402_init_diag_t *diag);

/**
 * @brief Process one non-blocking PDS and non-cyclic mode supervisor cycle for all configured axes.
 * @param manager Bound multi-axis Device manager.
 */
void CO_402_device_process(CO_402_device_manager_t *manager);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_DEVICE_H */
