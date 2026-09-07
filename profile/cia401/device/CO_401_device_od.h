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
    OD_entry_t *analogInterruptEnable; /**< Conditional-mandatory Object 0x6423 for analogue-input devices. */
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_EVENTS)
    OD_entry_t *analogInterruptTrigger; /**< Object 0x6421. */
    OD_entry_t *analogInterruptSource;  /**< Object 0x6422. */
    OD_entry_t *analogInterruptUpper32; /**< Object 0x6424. */
    OD_entry_t *analogInterruptLower32; /**< Object 0x6425. */
    OD_entry_t *analogInterruptDeltaU32; /**< Object 0x6426. */
    OD_entry_t *analogInterruptNegDeltaU32; /**< Object 0x6427. */
    OD_entry_t *analogInterruptPosDeltaU32; /**< Object 0x6428. */
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_EVENTS */
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE)
    OD_entry_t *analogOutputErrorMode; /**< Object 0x6443. */
    OD_entry_t *analogOutputErrorValue32; /**< Object 0x6444. */
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_OUTPUT_FAILSAFE */
#if defined(PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING)
    OD_entry_t *analogInputSiUnit; /**< Object 0x6430. */
    OD_entry_t *analogInputOffset32; /**< Object 0x6431. */
    OD_entry_t *analogInputPrescaling32; /**< Object 0x6432. */
    OD_entry_t *analogOutputOffset32; /**< Object 0x6446. */
    OD_entry_t *analogOutputScaling32; /**< Object 0x6447. */
    OD_entry_t *analogOutputSiUnit; /**< Object 0x6450. */
#endif /* PKG_CANOPENNODE_CIA401_ANALOG_CONDITIONING */
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
