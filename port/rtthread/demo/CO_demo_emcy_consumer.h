/**
 * @file CO_demo_emcy_consumer.h
 * @brief EMCY consumer diagnostic demo state and hooks.
 */

#ifndef CO_DEMO_EMCY_CONSUMER_H_
#define CO_DEMO_EMCY_CONSUMER_H_

#include "CANopen.h"

#if !defined(PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC)
#error "CO_demo_emcy_consumer requires PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC"
#endif /* !defined(PKG_CANOPENNODE_DEMO_EMCY_CONSUMER_DIAGNOSTIC) */

#include <rtatomic.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Runtime state for the optional EMCY consumer diagnostic demo. */
typedef struct {
    rt_atomic_t sequence; /**< Odd while the receive callback updates the snapshot, even when stable. */
    rt_atomic_t remoteRxCount; /**< Number of remote EMCY messages observed by the receive callback. */
    rt_atomic_t lastSourceNodeId; /**< Node-ID derived from the most recent remote EMCY CAN-ID. */
    rt_atomic_t lastCobId; /**< CAN-ID of the most recent remote EMCY message. */
    rt_atomic_t lastErrorCode; /**< Error code from the most recent remote EMCY message. */
    rt_atomic_t lastErrorRegister; /**< Error register from the most recent remote EMCY message. */
    rt_atomic_t lastErrorBit; /**< Callback errorBit (EMCY byte 3) from the most recent remote EMCY. */
    rt_atomic_t lastInfoCode; /**< Manufacturer-specific info code from the most recent remote EMCY message. */
} CO_demo_emcy_consumer_t;

/**
 * @brief Initialize EMCY consumer diagnostic runtime state.
 *
 * All atomics start at zero so OD 0x2301 reports an empty diagnostic snapshot
 * until the first remote EMCY message is received.
 *
 * @param demo EMCY consumer diagnostic state.
 */
void CO_demo_emcy_consumer_init(CO_demo_emcy_consumer_t *demo);

/**
 * @brief Bind the EMCY receive callback to a newly initialized CANopenNode stack.
 *
 * CANopenNode's EMCY receive callback does not provide an application object
 * pointer. The demo therefore binds one long-lived dispatcher state before CAN
 * normal mode is enabled and reuses it across communication reset.
 *
 * @param demo EMCY consumer diagnostic state.
 * @param co Current CANopenNode object.
 * @return true when the callback is bound, otherwise false.
 */
bool_t CO_demo_emcy_consumer_bind(CO_demo_emcy_consumer_t *demo, CO_t *co);

/**
 * @brief Publish one coherent remote EMCY snapshot to demo OD 0x2301.
 *
 * @param demo EMCY consumer diagnostic state.
 */
void CO_demo_emcy_consumer_process(CO_demo_emcy_consumer_t *demo);

/**
 * @brief Handle a CANopen application communication reset.
 *
 * The accumulated receive count and last remote EMCY snapshot intentionally
 * survive communication reset. CO_demo_emcy_consumer_bind() attaches the same
 * long-lived state to the newly created Emergency object.
 *
 * @param demo EMCY consumer diagnostic state.
 */
void CO_demo_emcy_consumer_reset(CO_demo_emcy_consumer_t *demo);

/**
 * @brief Release the singleton EMCY diagnostic callback owner.
 *
 * The caller must stop CAN receive callbacks before releasing the owner. This
 * hook is used by application initialization rollback after the CANopen stack
 * has been disabled and deleted.
 *
 * @param demo EMCY consumer diagnostic state.
 */
void CO_demo_emcy_consumer_deinit(CO_demo_emcy_consumer_t *demo);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_DEMO_EMCY_CONSUMER_H_ */
