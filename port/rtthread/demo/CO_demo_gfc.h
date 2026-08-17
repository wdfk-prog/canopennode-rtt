/**
 * @file CO_demo_gfc.h
 * @brief GFC consumer diagnostic and producer trigger demo state.
 */

#ifndef CO_DEMO_GFC_H_
#define CO_DEMO_GFC_H_

#include "CANopen.h"

#if !defined(PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC)
#error "CO_demo_gfc requires PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC"
#endif /* !defined(PKG_CANOPENNODE_DEMO_GFC_DIAGNOSTIC) */

#include <rtatomic.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** Long-lived state for the optional GFC protocol diagnostic demo. */
typedef struct {
    rt_atomic_t rxCount; /**< Accepted GFC consumer callbacks. */
    rt_atomic_t safeRequested; /**< Sticky nonzero indication of a valid GFC. */
    uint32_t producerCompleteSeq; /**< Last producer request handled by mainline. */
    int32_t producerResult; /**< Return value from the latest CO_GFCsend(). */
} CO_demo_gfc_t;

/**
 * @brief Initialize GFC demo state.
 *
 * @param demo GFC demo state.
 */
void CO_demo_gfc_init(CO_demo_gfc_t *demo);

/**
 * @brief Bind the GFC consumer callback to a newly initialized stack.
 *
 * The callback is rebound before CAN normal mode after every communication
 * reset. Pending producer requests are synchronized to the current OD value so
 * an old request is not replayed on the newly created stack.
 *
 * @param demo GFC demo state.
 * @param co Current CANopenNode object.
 * @return true when the GFC object exists and the callback was bound.
 */
bool_t CO_demo_gfc_bind(CO_demo_gfc_t *demo, CO_t *co);

/**
 * @brief Publish diagnostics and process one producer request from mainline.
 *
 * @param demo GFC demo state.
 * @param co Current CANopenNode object.
 */
void CO_demo_gfc_process(CO_demo_gfc_t *demo, CO_t *co);

/**
 * @brief Prepare GFC demo state for communication reset.
 *
 * Consumer diagnostics intentionally survive communication reset. The current
 * producer request is marked complete so it cannot be replayed after rebinding.
 *
 * @param demo GFC demo state.
 */
void CO_demo_gfc_reset(CO_demo_gfc_t *demo);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_DEMO_GFC_H_ */
