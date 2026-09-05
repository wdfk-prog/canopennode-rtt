/**
 * @file CO_402_device_fsa.h
 * @brief Pure-C PDS finite-state supervisor for one local CiA 402 axis.
 */

#ifndef CO_402_DEVICE_FSA_H
#define CO_402_DEVICE_FSA_H

#include "CO_402_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Process one non-blocking PDS supervisor cycle for an already-bound axis.
 *
 * @param axis Axis runtime with a valid DriveIF and bound Controlword/Statusword entries.
 */
void CO_402_device_axisProcess(CO_402_device_axis_t *axis);

/**
 * @brief Transfer all current owners to fault reaction and latch the originating diagnostic.
 *
 * @p fault supplies an already sampled product fault when origin is PRODUCT;
 * internal fault paths pass NULL so DiagIF maps the origin. Diagnostics never
 * weakens the fail-closed PDS transition when a mapping is missing.
 *
 * @param axis Axis runtime with a valid DriveIF.
 * @param origin Fault path which acquired fault ownership.
 * @param fault Optional already sampled product fault mapping.
 */
void CO_402_device_axisEnterFaultReaction(CO_402_device_axis_t *axis,
                                          CO_402_device_fault_origin_t origin,
                                          const CO_402_device_fault_info_t *fault);

/**
 * @brief Transfer a failed Controlword path to the fault-reaction safety owner.
 *
 * Any BUSY PDS, Fault Reset, or mode owner is retired at a supervisor callback
 * boundary before faultReaction runs. The DriveIF faultReaction callback must
 * synchronously supersede the physical action left by that retired owner, so
 * the control-path failure cannot be starved without executing callbacks in parallel.
 *
 * @param axis Axis runtime with a valid DriveIF.
 */
void CO_402_device_axisControlwordReadFailed(CO_402_device_axis_t *axis);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_DEVICE_FSA_H */
