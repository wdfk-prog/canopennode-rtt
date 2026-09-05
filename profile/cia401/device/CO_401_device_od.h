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

/** Cached generated-OD entries required by the enabled CiA 401 capabilities. */
typedef struct {
    OD_entry_t *deviceType;       /**< Mandatory Object 0x1000. */
    OD_entry_t *digitalInput8;    /**< Object 0x6000 when digital inputs are enabled. */
    OD_entry_t *digitalOutput8;   /**< Object 0x6200 when digital outputs are enabled. */
    OD_entry_t *analogInput16;    /**< Object 0x6401 when analogue inputs are enabled. */
    OD_entry_t *analogOutput16;   /**< Object 0x6411 when analogue outputs are enabled. */
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS)
    OD_entry_t *digitalInputPolarity8;   /**< Object 0x6002. */
    OD_entry_t *digitalInputFilter8;     /**< Object 0x6003. */
    OD_entry_t *digitalInterruptEnable;  /**< Object 0x6005. */
    OD_entry_t *digitalInterruptAny8;    /**< Object 0x6006. */
    OD_entry_t *digitalInterruptRising8; /**< Object 0x6007. */
    OD_entry_t *digitalInterruptFalling8; /**< Object 0x6008. */
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_EVENTS */
#if defined(PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE)
    OD_entry_t *digitalOutputPolarity8;   /**< Object 0x6202. */
    OD_entry_t *digitalOutputErrorMode8;  /**< Object 0x6206. */
    OD_entry_t *digitalOutputErrorValue8; /**< Object 0x6207. */
    OD_entry_t *digitalOutputFilter8;     /**< Object 0x6208. */
#endif /* PKG_CANOPENNODE_CIA401_DIGITAL_OUTPUT_FAILSAFE */
} CO_401_device_od_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_401_DEVICE_OD_H */
