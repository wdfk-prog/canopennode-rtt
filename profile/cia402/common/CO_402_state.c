/**
 * @file CO_402_state.c
 * @brief Stateless CiA 402 PDS Controlword and Statusword helpers.
 */

#include "CO_402_state.h"

/* Masks isolate only the PDS state bits; mode-specific status bits are ignored by this decoder. */
#define CO_402_STATUS_MASK_SHORT 0x004FU
#define CO_402_STATUS_MASK_LONG  0x006FU

/* Controlword masks are ordered from the most specific command pattern to the broadest one. */
#define CO_402_CONTROL_MASK_DISABLE_VOLTAGE 0x0082U
#define CO_402_CONTROL_MASK_QUICK_STOP      0x0086U
#define CO_402_CONTROL_MASK_SHUTDOWN        0x0087U
#define CO_402_CONTROL_MASK_STATE_COMMAND   0x008FU
#define CO_402_CONTROL_FAULT_RESET          0x0080U

/*
 * Decode the PDS state bits from a Statusword.
 *
 * The short mask is used for states whose decode does not depend on the extra
 * state bit covered by CO_402_STATUS_MASK_LONG. No runtime state is modified.
 */
CO_402_state_t CO_402_decodeStatusword(uint16_t statusword)
{
    /* Decode the two states represented by the short state mask first. */
    if ((statusword & CO_402_STATUS_MASK_SHORT) == 0x0000U) {
        return CO_402_STATE_NOT_READY_TO_SWITCH_ON;
    }
    if ((statusword & CO_402_STATUS_MASK_SHORT) == 0x0040U) {
        return CO_402_STATE_SWITCH_ON_DISABLED;
    }

    /* These four active-drive states require the longer state mask. */
    if ((statusword & CO_402_STATUS_MASK_LONG) == 0x0021U) {
        return CO_402_STATE_READY_TO_SWITCH_ON;
    }
    if ((statusword & CO_402_STATUS_MASK_LONG) == 0x0023U) {
        return CO_402_STATE_SWITCHED_ON;
    }
    if ((statusword & CO_402_STATUS_MASK_LONG) == 0x0027U) {
        return CO_402_STATE_OPERATION_ENABLED;
    }
    if ((statusword & CO_402_STATUS_MASK_LONG) == 0x0007U) {
        return CO_402_STATE_QUICK_STOP_ACTIVE;
    }

    /* Fault states intentionally ignore the longer-mask-only bit. */
    if ((statusword & CO_402_STATUS_MASK_SHORT) == 0x000FU) {
        return CO_402_STATE_FAULT_REACTION_ACTIVE;
    }
    if ((statusword & CO_402_STATUS_MASK_SHORT) == 0x0008U) {
        return CO_402_STATE_FAULT;
    }

    return CO_402_STATE_UNKNOWN;
}

/*
 * Encode only the PDS-owned Statusword state pattern; active modes add their own bits separately.
 *
 * Mode-specific and manufacturer-specific Statusword bits are outside this
 * helper and remain owned by their respective mode or product runtime.
 */
uint16_t CO_402_statuswordForState(CO_402_state_t state)
{
    switch (state) {
        case CO_402_STATE_NOT_READY_TO_SWITCH_ON:
            return 0x0000U;
        case CO_402_STATE_SWITCH_ON_DISABLED:
            return 0x0040U;
        case CO_402_STATE_READY_TO_SWITCH_ON:
            return 0x0021U;
        case CO_402_STATE_SWITCHED_ON:
            return 0x0023U;
        case CO_402_STATE_OPERATION_ENABLED:
            return 0x0027U;
        case CO_402_STATE_QUICK_STOP_ACTIVE:
            return 0x0007U;
        case CO_402_STATE_FAULT_REACTION_ACTIVE:
            return 0x000FU;
        case CO_402_STATE_FAULT:
            return 0x0008U;
        case CO_402_STATE_UNKNOWN:
        default:
            /* Unknown internal state must not publish arbitrary Statusword bits. */
            return 0x0000U;
    }
}

/*
 * Decode the PDS command pattern from a Controlword.
 *
 * More specific patterns are checked before broader masks so one Controlword
 * cannot be misclassified by a later, less restrictive comparison.
 */
CO_402_command_t CO_402_decodeControlword(uint16_t controlword)
{
    /* Fault reset has priority because bit 7 is orthogonal to the normal state-command mask. */
    if ((controlword & CO_402_CONTROL_FAULT_RESET) != 0U) {
        return CO_402_COMMAND_FAULT_RESET;
    }
    if ((controlword & CO_402_CONTROL_MASK_STATE_COMMAND) == 0x000FU) {
        return CO_402_COMMAND_ENABLE_OPERATION;
    }
    if ((controlword & CO_402_CONTROL_MASK_STATE_COMMAND) == 0x0007U) {
        return CO_402_COMMAND_SWITCH_ON_OR_DISABLE_OPERATION;
    }
    if ((controlword & CO_402_CONTROL_MASK_SHUTDOWN) == 0x0006U) {
        return CO_402_COMMAND_SHUTDOWN;
    }
    if ((controlword & CO_402_CONTROL_MASK_QUICK_STOP) == 0x0002U) {
        return CO_402_COMMAND_QUICK_STOP;
    }
    if ((controlword & CO_402_CONTROL_MASK_DISABLE_VOLTAGE) == 0x0000U) {
        return CO_402_COMMAND_DISABLE_VOLTAGE;
    }

    return CO_402_COMMAND_UNKNOWN;
}
