/**
 * @file CO_402_drive.h
 * @brief Hardware-neutral asynchronous drive interface for the CiA 402 Device core.
 */

#ifndef CO_402_DRIVE_H
#define CO_402_DRIVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Result returned by a non-blocking DriveIF state transition request. */
typedef enum {
    CO_402_DRIVE_BUSY = 0, /**< Transition is still in progress; retry on the next supervisor cycle. */
    CO_402_DRIVE_DONE,     /**< Transition completed successfully. */
    CO_402_DRIVE_ERROR     /**< Transition failed and the PDS supervisor must enter fault reaction. */
} CO_402_drive_result_t;

/**
 * @brief Hardware-neutral drive operations used by the PDS supervisor.
 *
 * Transition callbacks must be non-blocking. Returning CO_402_DRIVE_BUSY keeps
 * the current PDS state. While the Controlword keeps requesting the same transition,
 * the same callback is retried on the next supervisor cycle. Implementations must
 * therefore tolerate repeated calls while one physical transition is still in progress.
 */
typedef struct {
    CO_402_drive_result_t (*shutdown)(void *object);         /**< Reach Ready to switch on without a voltage-off shortcut. */
    CO_402_drive_result_t (*switchOn)(void *object);         /**< Complete the transition to Switched on. */
    CO_402_drive_result_t (*enableOperation)(void *object);  /**< Enable product motion/power operation. */
    CO_402_drive_result_t (*disableOperation)(void *object); /**< Leave Operation enabled while retaining switch-on state. */
    CO_402_drive_result_t (*quickStop)(void *object);        /**< Start or continue the product quick-stop action. */
    CO_402_drive_result_t (*faultReaction)(void *object);    /**< Execute the product fault-reaction action. */
    CO_402_drive_result_t (*faultReset)(void *object);       /**< Clear the product fault when reset is allowed. */

    int32_t (*getPosition)(void *object); /**< Optional position feedback callback. */
    int32_t (*getVelocity)(void *object); /**< Optional velocity feedback callback. */
    int16_t (*getTorque)(void *object);   /**< Optional torque feedback callback reserved for later modes. */

    /* Appended in A3 so existing DriveIF member offsets remain unchanged. */
    CO_402_drive_result_t (*disableVoltage)(void *object); /**< Remove drive voltage for Switch on disabled. */
} CO_402_drive_if_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_DRIVE_H */
