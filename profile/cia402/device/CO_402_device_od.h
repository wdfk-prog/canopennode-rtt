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
    OD_entry_t *controlword;              /**< PDS Controlword command object. */
    OD_entry_t *statusword;               /**< PDS Statusword state object. */
    OD_entry_t *modesOfOperation;         /**< Requested INTEGER8 operation-mode object. */
    OD_entry_t *modesOfOperationDisplay;  /**< Active INTEGER8 operation-mode display object. */
    OD_entry_t *positionActualValue;      /**< Position feedback object. */
    OD_entry_t *targetPosition;           /**< Position command object reserved for mode runtimes. */
    OD_entry_t *velocityActualValue;      /**< Velocity feedback object. */
    OD_entry_t *targetVelocity;           /**< Velocity command object reserved for mode runtimes. */
    OD_entry_t *supportedDriveModes;      /**< Bit-mask object advertising implemented operation modes. */

    OD_extension_t controlwordExtension;  /**< Axis-owned extension installed on Controlword before PDO init. */
    OD_extension_t modeExtension;         /**< Axis-owned extension installed on Modes of operation. */
} CO_402_device_od_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_DEVICE_OD_H */
