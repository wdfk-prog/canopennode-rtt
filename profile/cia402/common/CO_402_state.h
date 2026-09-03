/**
 * @file CO_402_state.h
 * @brief Role-neutral CiA 402 PDS state and command definitions.
 */

#ifndef CO_402_STATE_H
#define CO_402_STATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Internal symbolic CiA 402 PDS state.
 *
 * Values are library identifiers and are not Statusword bit encodings.
 */
typedef enum {
    CO_402_STATE_NOT_READY_TO_SWITCH_ON = 0, /**< Device core is not ready for state commands. */
    CO_402_STATE_SWITCH_ON_DISABLED,         /**< Power stage switching is disabled. */
    CO_402_STATE_READY_TO_SWITCH_ON,         /**< Drive is ready to be switched on. */
    CO_402_STATE_SWITCHED_ON,                /**< Drive is switched on but operation is disabled. */
    CO_402_STATE_OPERATION_ENABLED,          /**< Drive operation is enabled. */
    CO_402_STATE_QUICK_STOP_ACTIVE,          /**< Quick-stop behavior is active. */
    CO_402_STATE_FAULT_REACTION_ACTIVE,      /**< Fault reaction is being executed. */
    CO_402_STATE_FAULT,                      /**< Drive is faulted and awaits reset. */
    CO_402_STATE_UNKNOWN = 0xFF              /**< Statusword could not be resolved to a known state. */
} CO_402_state_t;

/** Stateless interpretation of the PDS-related Controlword command bits. */
typedef enum {
    CO_402_COMMAND_DISABLE_VOLTAGE = 0,          /**< Disable-voltage command pattern. */
    CO_402_COMMAND_QUICK_STOP,                   /**< Quick-stop command pattern. */
    CO_402_COMMAND_SHUTDOWN,                     /**< Shutdown command pattern. */
    CO_402_COMMAND_SWITCH_ON_OR_DISABLE_OPERATION, /**< State-dependent 0x0007 command pattern. */
    CO_402_COMMAND_ENABLE_OPERATION,             /**< Enable-operation command pattern. */
    CO_402_COMMAND_FAULT_RESET,                  /**< Fault-reset bit is asserted. */
    CO_402_COMMAND_UNKNOWN                       /**< No supported PDS command pattern matched. */
} CO_402_command_t;

/**
 * @brief Decode the PDS state encoded in a Statusword.
 * @param statusword CiA 402 Statusword value.
 * @return Decoded state or CO_402_STATE_UNKNOWN.
 */
CO_402_state_t CO_402_decodeStatusword(uint16_t statusword);

/**
 * @brief Encode the PDS state bits for a Statusword.
 * @param state Symbolic PDS state.
 * @return Base Statusword value for @p state, or 0 for an unknown state.
 */
uint16_t CO_402_statuswordForState(CO_402_state_t state);

/**
 * @brief Decode the PDS-related command represented by a Controlword.
 * @param controlword CiA 402 Controlword value.
 * @return Stateless command identifier. State-dependent interpretation remains in the Device FSA.
 */
CO_402_command_t CO_402_decodeControlword(uint16_t controlword);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_STATE_H */
