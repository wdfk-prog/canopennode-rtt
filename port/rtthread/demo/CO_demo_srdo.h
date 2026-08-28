/**
 * @file CO_demo_srdo.h
 * @brief Test-only single-instance J09/B09S SRDO diagnostic bridge.
 *
 * The fixture writes the generated global OD_RAM.x2306_srdo_diagnostic record
 * and therefore requires PKG_CANOPENNODE_CAN_BINDING_COUNT=1.
 */

#ifndef CO_DEMO_SRDO_H_
#define CO_DEMO_SRDO_H_

#include "CANopen.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the 0x2306 test-only diagnostic state. */
void CO_demo_srdo_init(void);

/**
 * Validate that the current stack exposes the deterministic two-SRDO profile.
 *
 * Binding also normalizes the TX completion sequence and sets the
 * 0x2306:0B request-result witness to INT32_MIN. Natural cyclic SRDO TX does not
 * change that witness; a serviced diagnostic request does.
 *
 * @param co Current CANopenNode object after CO_CANopenInitSRDO().
 * @return true when SRDO1 is RX and SRDO2 is TX, otherwise false.
 */
bool_t CO_demo_srdo_bind(CO_t *co);

/** Publish per-SRDO states and service a new 0x2306:09 TX request sequence. */
void CO_demo_srdo_process(CO_t *co);

/** Reset published diagnostic state before communication reset/rebind. */
void CO_demo_srdo_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* CO_DEMO_SRDO_H_ */
