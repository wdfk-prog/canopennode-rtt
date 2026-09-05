/**
 * @file CO_402_drive.h
 * @brief Hardware-neutral asynchronous drive interface for the CiA 402 Device core.
 */

#ifndef CO_402_DRIVE_H
#define CO_402_DRIVE_H

#include <stdbool.h>
#include <stdint.h>

#include "CO_402_mode.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Result returned by a non-blocking PDS transition or operation-mode request. */
typedef enum {
    CO_402_DRIVE_BUSY = 0, /**< Operation is accepted/in progress; retry or poll on the next supervisor cycle. */
    CO_402_DRIVE_DONE,     /**< Operation completed successfully. */
    CO_402_DRIVE_ERROR     /**< Operation failed and the Device supervisor must enter fault reaction. */
} CO_402_drive_result_t;

/**
 * Profile-position request passed to the product DriveIF. Parameters are
 * latched when a new set-point edge is accepted; only live control flags are
 * refreshed while an in-flight command is polled.
 */
typedef struct {
    int32_t targetPosition;         /**< Latched position request; relative selects absolute or relative interpretation. */
    uint32_t profileVelocity;       /**< Latched velocity limit used to generate the accepted position profile. */
    uint32_t profileAcceleration;   /**< Latched acceleration used while approaching the accepted target. */
    uint32_t profileDeceleration;   /**< Latched deceleration used while completing the accepted profile. */
    uint32_t quickStopDeceleration; /**< Latched 0x6085 value supplied to the backend for its stop policy. */
    int16_t motionProfileType;      /**< Latched profile-generator type associated with the accepted set-point. */
    bool newSetPoint;               /**< True only on the supervisor pass that accepts a bit-4 rising edge. */
    bool changeImmediately;         /**< Latched bit 5 request to replace an active positioning target immediately. */
    bool relative;                  /**< Latched bit 6; true interprets targetPosition as a relative displacement. */
    bool halt;                      /**< Live bit 8 level requesting mode-level motion halt without changing the snapshot. */
} CO_402_profile_position_command_t;

/** Current profile-velocity request passed to the product DriveIF once per enabled pass. */
typedef struct {
    int32_t targetVelocity;         /**< Current commanded velocity sampled from 0x60FF for this supervisor pass. */
    uint32_t profileAcceleration;   /**< Current acceleration limit supplied with the velocity command. */
    uint32_t profileDeceleration;   /**< Current deceleration limit supplied with the velocity command. */
    uint32_t quickStopDeceleration; /**< Current 0x6085 value supplied to the backend for its stop policy. */
    bool halt;                      /**< Live bit 8 level requesting mode-level velocity halt. */
} CO_402_profile_velocity_command_t;

/**
 * Homing request passed to the product DriveIF. Homing parameters are latched
 * on an accepted start edge and retained through completion or abort.
 */
typedef struct {
    int32_t homeOffset;        /**< Latched home-position offset applied by the selected homing procedure. */
    int8_t homingMethod;       /**< Latched homing procedure selector read when the start edge is accepted. */
    uint32_t speedSwitch;      /**< Latched search speed used while locating the homing switch/reference. */
    uint32_t speedZero;        /**< Latched low-speed search used to establish the final home reference. */
    uint32_t acceleration;     /**< Latched acceleration limit for the active homing procedure. */
    bool start;                /**< Live start level; forced false while an accepted homing action is aborting. */
    bool startEdge;            /**< True only on the supervisor pass that accepts a fresh homing start edge. */
    bool halt;                 /**< Live bit 8 level forwarded while homing or abort is being polled. */
} CO_402_homing_command_t;

/**
 * @brief Hardware-neutral drive operations used by the PDS and operation-mode supervisors.
 *
 * All callbacks must be non-blocking and follow the same BUSY/DONE/ERROR
 * polling contract. Operation-mode callbacks run only from the Pure-C Device
 * supervisor and are not part of the RT-Thread synchronous realtime path.
 *
 * For normal PDS transitions, CO_402_DRIVE_BUSY retains exclusive ownership of
 * the originally accepted callback and target state for ordinary requests. A later
 * Quick-stop or Disable-voltage command may transfer ownership at a supervisor
 * callback boundary; faultReaction has still higher priority. The incoming safety
 * callback must synchronously supersede/cancel any physical action left BUSY by
 * the retired owner before it returns BUSY or DONE. If takeover returns ERROR,
 * the supervisor invokes faultReaction immediately in the same supervisor pass.
 * The supervisor never executes two DriveIF callbacks concurrently and never
 * invents a timeout for BUSY work.
 * Controlword read failure uses the same ownership-transfer rule for faultReaction,
 * including when Fault Reset or mode entry/exit was BUSY.
 *
 * For PP/PV/HM, CO_402_DRIVE_BUSY means the command is accepted but the mode
 * target has not yet been reached, CO_402_DRIVE_DONE means the mode-specific
 * target/completion condition is satisfied, and CO_402_DRIVE_ERROR causes PDS
 * fault reaction. PP and HM callbacks receive parameters captured by the
 * accepted Controlword bit-4 edge; BUSY polls retain that snapshot even if the
 * backing OD changes, while live Controlword fields such as halt/start are
 * refreshed. PV is sampled from the current OD on every Operation-enabled pass.
 *
 * A BUSY modeEnter/modeExit callback retains exclusive DriveIF ownership across
 * ordinary 0x6060/PDS changes. Quick-stop, Disable-voltage, or fault reaction may
 * retire that software token and take over at the next supervisor callback boundary;
 * the incoming safety callback must then synchronously supersede the unfinished mode
 * action. modeExit must still cancel/retire mode-owned motion before returning DONE
 * when no safety transfer occurs.
 */
typedef struct {
    CO_402_drive_result_t (*shutdown)(void *object);         /**< Reach Ready to switch on without a voltage-off shortcut. */
    CO_402_drive_result_t (*switchOn)(void *object);         /**< Complete the transition to Switched on. */
    CO_402_drive_result_t (*enableOperation)(void *object);  /**< Enable product motion/power operation. */
    CO_402_drive_result_t (*disableOperation)(void *object); /**< Leave Operation enabled while retaining switch-on state. */
    CO_402_drive_result_t (*quickStop)(void *object);        /**< Safety owner; first call may supersede lower-priority BUSY work. */
    CO_402_drive_result_t (*faultReaction)(void *object);    /**< Highest-priority safety owner; may supersede any BUSY work. */
    CO_402_drive_result_t (*faultReset)(void *object);       /**< Clear the product fault when reset is allowed. */

    int32_t (*getPosition)(void *object); /**< Optional position feedback callback. */
    int32_t (*getVelocity)(void *object); /**< Optional velocity feedback callback. */
    int16_t (*getTorque)(void *object);   /**< Optional torque feedback callback for torque-oriented modes. */

    /* Keep this established member position stable; later mode callbacks remain append-only for ABI compatibility. */
    CO_402_drive_result_t (*disableVoltage)(void *object); /**< Safety owner; may supersede Quick-stop or ordinary BUSY work. */

    /* New mode callbacks are append-only so existing positional/designated DriveIF initializers keep their layout. */
    CO_402_drive_result_t (*modeEnter)(void *object, CO_402_mode_t mode); /**< Begin/continue setup; BUSY owns entry. */
    CO_402_drive_result_t (*modeExit)(void *object, CO_402_mode_t mode);  /**< Retire mode motion; BUSY owns exit. */
    /** Execute/poll a PP set-point. BUSY keeps it active; DONE asserts target reached; ERROR faults the axis. */
    CO_402_drive_result_t (*profilePosition)(void *object, const CO_402_profile_position_command_t *command);
    /** Execute/poll the current PV command once per Operation-enabled supervisor pass. */
    CO_402_drive_result_t (*profileVelocity)(void *object, const CO_402_profile_velocity_command_t *command);
    /** Execute/poll HM start or an active HM abort. DONE with start=false acknowledges the abort. */
    CO_402_drive_result_t (*homing)(void *object, const CO_402_homing_command_t *command);
} CO_402_drive_if_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_DRIVE_H */
