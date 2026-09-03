/**
 * @file CO_402_device_RTT_demo.c
 * @brief Software-only CiA 402 Device factory for RT-Thread protocol validation.
 */

#include "CO_402_device_RTT.h"

#if (PKG_CANOPENNODE_CIA402_DEMO_AXIS_COUNT < 1) || (PKG_CANOPENNODE_CIA402_DEMO_AXIS_COUNT > 3)
#error "PKG_CANOPENNODE_CIA402_DEMO_AXIS_COUNT must be in the generated demo OD range 1..3"
#endif /* (PKG_CANOPENNODE_CIA402_DEMO_AXIS_COUNT < 1) || (PKG_CANOPENNODE_CIA402_DEMO_AXIS_COUNT > 3) */

/** Complete one demo PDS transition without touching physical hardware. */
static CO_402_drive_result_t CO_402_demoTransitionDone(void *object)
{
    (void)object;
    return CO_402_DRIVE_DONE;
}

/** Return deterministic zero position feedback for the software-only demo. */
static int32_t CO_402_demoGetPosition(void *object)
{
    (void)object;
    return 0;
}

/** Return deterministic zero velocity feedback for the software-only demo. */
static int32_t CO_402_demoGetVelocity(void *object)
{
    (void)object;
    return 0;
}

/** Return deterministic zero torque feedback for the software-only demo. */
static int16_t CO_402_demoGetTorque(void *object)
{
    (void)object;
    return 0;
}

/** Immediate-DONE DriveIF used only to exercise CiA 402 protocol/FSA/lifecycle behavior. */
static const CO_402_drive_if_t CO_402_demoDriveIf = {
    .shutdown = CO_402_demoTransitionDone,
    .switchOn = CO_402_demoTransitionDone,
    .enableOperation = CO_402_demoTransitionDone,
    .disableOperation = CO_402_demoTransitionDone,
    .quickStop = CO_402_demoTransitionDone,
    .faultReaction = CO_402_demoTransitionDone,
    .faultReset = CO_402_demoTransitionDone,
    .getPosition = CO_402_demoGetPosition,
    .getVelocity = CO_402_demoGetVelocity,
    .getTorque = CO_402_demoGetTorque,
    .disableVoltage = CO_402_demoTransitionDone,
};

/** Generated demo OD provides three consecutive local logical-device blocks. */
static const CO_402_device_axis_config_t CO_402_demoAxisConfigs[] = {
    { .logicalDevice = 0U, .drive = &CO_402_demoDriveIf, .driveObject = NULL },
    { .logicalDevice = 1U, .drive = &CO_402_demoDriveIf, .driveObject = NULL },
    { .logicalDevice = 2U, .drive = &CO_402_demoDriveIf, .driveObject = NULL },
};

CO_402_DEVICE_RTT_AUTOSTART_DEFINE(cia402_demo, CO_402_demoAxisConfigs,
                                    PKG_CANOPENNODE_CIA402_DEMO_AXIS_COUNT);
