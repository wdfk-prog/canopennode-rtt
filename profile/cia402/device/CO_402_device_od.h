/**
 * @file CO_402_device_od.h
 * @brief Object Dictionary binding contract for one local CiA 402 logical device.
 */

#ifndef CO_402_DEVICE_OD_H
#define CO_402_DEVICE_OD_H

#include "301/CO_ODinterface.h"
#include "CO_402_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Cached OD entries and forwarding extensions owned by one Device axis. */
typedef struct {
    OD_entry_t *errorCode;                /**< Error code object for this logical device. */
    OD_entry_t *controlword;              /**< PDS/mode command source sampled once per axis supervisor pass. */
    OD_entry_t *statusword;               /**< PDS state plus active-mode result bits published by the supervisor. */
    OD_entry_t *modesOfOperation;         /**< Requested mode; changes are serialized through mode exit/entry. */
    OD_entry_t *modesOfOperationDisplay;  /**< Mode actually active after any non-blocking transition completes. */
    OD_entry_t *positionActualValue;      /**< Position feedback refreshed from DriveIF when that callback exists. */
    OD_entry_t *velocityActualValue;      /**< Velocity feedback refreshed from DriveIF when that callback exists. */
    OD_entry_t *targetPosition;           /**< PP target captured only when a fresh set-point edge is accepted. */
    OD_entry_t *targetVelocity;           /**< PV target sampled as a live command on each enabled supervisor pass. */
    OD_entry_t *supportedDriveModes;      /**< 0x6502 mask derived from compiled modes and per-axis DriveIF callbacks. */

    /* Mode-specific entries are cached only when the axis advertises the corresponding mode capability. */
    OD_entry_t *homeOffset;               /**< HM offset captured when a new homing action is accepted. */
    OD_entry_t *profileVelocity;          /**< PP profile-velocity limit captured with a new set-point. */
    OD_entry_t *profileAcceleration;      /**< PP/PV acceleration parameter consumed by the active mode. */
    OD_entry_t *profileDeceleration;      /**< PP/PV deceleration parameter consumed by the active mode. */
    OD_entry_t *quickStopDeceleration;    /**< PP/PV 0x6085 parameter forwarded to the DriveIF stop policy. */
    OD_entry_t *motionProfileType;        /**< PP profile-generator type captured with the accepted set-point. */
    OD_entry_t *homingMethod;             /**< HM procedure selector captured on an accepted start edge. */
    OD_entry_t *homingSpeeds;             /**< HM RECORD containing switch-search and zero-search speeds. */
    OD_entry_t *homingAcceleration;       /**< HM acceleration captured for the active homing procedure. */

    OD_extension_t controlwordExtension;  /**< Axis-owned extension installed on Controlword before PDO init. */
    OD_extension_t modeExtension;         /**< Axis-owned extension installed on Modes of operation. */
} CO_402_device_od_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_DEVICE_OD_H */
