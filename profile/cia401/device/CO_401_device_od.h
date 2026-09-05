/**
 * @file CO_401_device_od.h
 * @brief Generated Object Dictionary binding cache for the CiA 401 Device core.
 */

#ifndef CO_401_DEVICE_OD_H
#define CO_401_DEVICE_OD_H

#include "301/CO_ODinterface.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Cached generated-OD entries required by enabled Stage-1 capabilities. */
typedef struct {
    OD_entry_t *deviceType;       /**< Mandatory Object 0x1000. */
    OD_entry_t *digitalInput8;    /**< Object 0x6000 when digital inputs are enabled. */
    OD_entry_t *digitalOutput8;   /**< Object 0x6200 when digital outputs are enabled. */
    OD_entry_t *analogInput16;    /**< Object 0x6401 when analogue inputs are enabled. */
    OD_entry_t *analogOutput16;   /**< Object 0x6411 when analogue outputs are enabled. */
} CO_401_device_od_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_401_DEVICE_OD_H */
