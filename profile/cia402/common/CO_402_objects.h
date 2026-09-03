/**
 * @file CO_402_objects.h
 * @brief Axis-0 CiA 402 Object Dictionary index constants shared by profile roles.
 */

#ifndef CO_402_OBJECTS_H
#define CO_402_OBJECTS_H

#include <stdint.h>

#include "CO_402_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Axis-0 Error code object. */
#define CO_402_INDEX_ERROR_CODE 0x603FU
/** Axis-0 Controlword object. */
#define CO_402_INDEX_CONTROLWORD 0x6040U
/** Axis-0 Statusword object. */
#define CO_402_INDEX_STATUSWORD 0x6041U
/** Axis-0 Modes of operation object. */
#define CO_402_INDEX_MODES_OF_OPERATION 0x6060U
/** Axis-0 Modes of operation display object. */
#define CO_402_INDEX_MODES_OF_OPERATION_DISPLAY 0x6061U
/** Axis-0 Position actual value object. */
#define CO_402_INDEX_POSITION_ACTUAL_VALUE 0x6064U
/** Axis-0 Target position object. */
#define CO_402_INDEX_TARGET_POSITION 0x607AU
/** Axis-0 Velocity actual value object. */
#define CO_402_INDEX_VELOCITY_ACTUAL_VALUE 0x606CU
/** Axis-0 Target velocity object. */
#define CO_402_INDEX_TARGET_VELOCITY 0x60FFU
/** Axis-0 Supported drive modes object. */
#define CO_402_INDEX_SUPPORTED_DRIVE_MODES 0x6502U

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_OBJECTS_H */
