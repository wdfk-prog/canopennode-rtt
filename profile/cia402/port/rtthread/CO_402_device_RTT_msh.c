/**
 * @file CO_402_device_RTT_msh.c
 * @brief RT-Thread MSH control frontend for the local CiA 402 Device supervisor.
 */

#include "CO_app_RTT.h"
#include "CO_402_device_RTT.h"

#if defined(PKG_CANOPENNODE_CIA402_DEVICE_RTT_MSH)

#include <finsh.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Singleton console binding for the default auto-attached CiA 402 Device runtime. */
typedef struct {
    CANopenNodeRTT *app;
    CO_402_device_RTT_t *runtime;
} CO_402_device_RTT_msh_t;

/* Values read under lifecycle/OD protection and printed after releasing both locks. */
typedef struct {
    uint8_t axis;
    CO_402_state_t state;
    CO_402_mode_t mode;
    CO_402_mode_raw_t requestedMode;
    uint32_t supportedModes;
    uint16_t controlword;
    uint16_t statusword;
    int32_t position;
    int32_t velocity;
    int32_t targetPosition;
    int32_t targetVelocity;
} CO_402_device_RTT_msh_status_t;

static CO_402_device_RTT_msh_t CO_402_msh;

static const char *CO_402_mshStateName(CO_402_state_t state)
{
    switch (state) {
        case CO_402_STATE_NOT_READY_TO_SWITCH_ON:
            return "not-ready";
        case CO_402_STATE_SWITCH_ON_DISABLED:
            return "switch-on-disabled";
        case CO_402_STATE_READY_TO_SWITCH_ON:
            return "ready-to-switch-on";
        case CO_402_STATE_SWITCHED_ON:
            return "switched-on";
        case CO_402_STATE_OPERATION_ENABLED:
            return "operation-enabled";
        case CO_402_STATE_QUICK_STOP_ACTIVE:
            return "quick-stop-active";
        case CO_402_STATE_FAULT_REACTION_ACTIVE:
            return "fault-reaction-active";
        case CO_402_STATE_FAULT:
            return "fault";
        default:
            return "unknown";
    }
}

static const char *CO_402_mshModeName(CO_402_mode_t mode)
{
    switch (mode) {
        case CO_402_MODE_NONE:
            return "none";
        case CO_402_MODE_PROFILE_POSITION:
            return "pp";
        case CO_402_MODE_PROFILE_VELOCITY:
            return "pv";
        case CO_402_MODE_HOMING:
            return "hm";
        case CO_402_MODE_CYCLIC_SYNC_POSITION:
            return "csp";
        case CO_402_MODE_CYCLIC_SYNC_VELOCITY:
            return "csv";
        case CO_402_MODE_CYCLIC_SYNC_TORQUE:
            return "cst";
        default:
            return "unknown";
    }
}

static bool CO_402_mshParseMagnitude(const char *text, bool allowSign, bool *negative, uint64_t *value)
{
    const char *cursor = text;
    uint64_t parsed = 0U;
    unsigned int base = 10U;
    bool hasDigit = false;

    if (text == NULL || negative == NULL || value == NULL || text[0] == '\0') {
        return false;
    }

    *negative = false;
    if (*cursor == '+' || *cursor == '-') {
        if (!allowSign) {
            return false;
        }
        *negative = *cursor == '-';
        cursor++;
    }
    if (cursor[0] == '0' && (cursor[1] == 'x' || cursor[1] == 'X')) {
        base = 16U;
        cursor += 2;
    }

    while (*cursor != '\0') {
        unsigned int digit;

        if (*cursor >= '0' && *cursor <= '9') {
            digit = (unsigned int)(*cursor - '0');
        } else if (base == 16U && *cursor >= 'a' && *cursor <= 'f') {
            digit = (unsigned int)(*cursor - 'a') + 10U;
        } else if (base == 16U && *cursor >= 'A' && *cursor <= 'F') {
            digit = (unsigned int)(*cursor - 'A') + 10U;
        } else {
            return false;
        }
        if (digit >= base || parsed > (UINT64_MAX - digit) / base) {
            return false;
        }

        parsed = parsed * base + digit;
        hasDigit = true;
        cursor++;
    }

    if (!hasDigit) {
        return false;
    }

    *value = parsed;
    return true;
}

static bool CO_402_mshParseLong(const char *text, long minimum, long maximum, long *value)
{
    uint64_t magnitude;
    bool negative;
    int64_t parsed;

    if (value == NULL || !CO_402_mshParseMagnitude(text, true, &negative, &magnitude)
        || magnitude > (uint64_t)INT32_MAX + 1U) {
        return false;
    }

    parsed = negative ? -(int64_t)magnitude : (int64_t)magnitude;
    if (parsed < (int64_t)minimum || parsed > (int64_t)maximum) {
        return false;
    }

    *value = (long)parsed;
    return true;
}

static bool CO_402_mshParseUnsignedLong(const char *text, unsigned long maximum, unsigned long *value)
{
    uint64_t parsed;
    bool negative;

    if (value == NULL || !CO_402_mshParseMagnitude(text, false, &negative, &parsed)
        || negative || parsed > (uint64_t)maximum) {
        return false;
    }

    *value = (unsigned long)parsed;
    return true;
}

static bool CO_402_mshParseAxis(const char *text, uint8_t *axis)
{
    unsigned long parsed;

    if (axis == NULL
        || !CO_402_mshParseUnsignedLong(text, CO_402_LOGICAL_DEVICE_COUNT_MAX - 1U, &parsed)) {
        return false;
    }

    *axis = (uint8_t)parsed;
    return true;
}

static bool CO_402_mshParseMode(const char *text, CO_402_mode_t *mode)
{
    if (text == NULL || mode == NULL) {
        return false;
    }
    if (strcmp(text, "none") == 0) {
        *mode = CO_402_MODE_NONE;
    } else if (strcmp(text, "pp") == 0) {
        *mode = CO_402_MODE_PROFILE_POSITION;
    } else if (strcmp(text, "pv") == 0) {
        *mode = CO_402_MODE_PROFILE_VELOCITY;
    } else if (strcmp(text, "hm") == 0) {
        *mode = CO_402_MODE_HOMING;
    } else {
        return false;
    }

    return true;
}

static void CO_402_mshPrintUsage(void)
{
    rt_kprintf("CiA 402 local supervisor debug commands:\n");
    rt_kprintf("  cia402 status [axis]\n");
    rt_kprintf("  cia402 cw <axis> <value>\n");
    rt_kprintf("  cia402 pds <axis> <disable|shutdown|switchon|enable|disableop|quickstop|faultreset>\n");
    rt_kprintf("  cia402 mode <axis> <none|pp|pv|hm>\n");
    rt_kprintf("  cia402 pp <axis> <target> <velocity> <accel> <decel> [relative:0|1] [immediate:0|1]\n");
    rt_kprintf("  cia402 pv <axis> <target-velocity> <accel> <decel>\n");
    rt_kprintf("  cia402 hm <axis> <method> <offset> <switch-speed> <zero-speed> <accel>\n");
    rt_kprintf("  cia402 start <axis>       # bit 4 edge; requires operation-enabled stable PP/HM\n");
    rt_kprintf("  cia402 stop <axis>        # clear bit 4; HM uses this as abort\n");
    rt_kprintf("  cia402 halt <axis> <0|1>  # Controlword bit 8\n");
}

/*
 * Serialize against stack recreation first, then take the OD lock in the same
 * lifecycleMutex -> OD order used by co_402 and co_rt. This keeps console writes
 * on the current communication generation without introducing a second owner.
 */
static rt_err_t CO_402_mshLock(CANopenNodeRTT **appOut, CO_402_device_RTT_t **runtimeOut, CO_t **coOut)
{
    CANopenNodeRTT *app = CO_402_msh.app;
    CO_402_device_RTT_t *runtime;
    CO_t *co;
    rt_err_t ret;

    /*
     * The MSH option is tied to the default auto-start application, whose app
     * storage outlives runtime teardown. Lock that stable owner before reading
     * the heap-owned CiA 402 runtime pointer.
     */
    if (app == NULL) {
        return -RT_ERROR;
    }

    ret = rt_mutex_take(&app->lifecycleMutex, RT_WAITING_FOREVER);
    if (ret != RT_EOK) {
        return ret;
    }

    runtime = CO_402_msh.runtime;
    if (app != CO_402_msh.app || runtime == NULL || runtime->managerInitialized != RT_TRUE
        || runtime->communicationReady != RT_TRUE || !runtime->manager.odBound) {
        (void)rt_mutex_release(&app->lifecycleMutex);
        return -RT_ERROR;
    }

    co = app->canOpenStack;
    if (co == NULL || co->CANmodule == NULL) {
        (void)rt_mutex_release(&app->lifecycleMutex);
        return -RT_ERROR;
    }

    CO_LOCK_OD(co->CANmodule);
    *appOut = app;
    *runtimeOut = runtime;
    *coOut = co;
    return RT_EOK;
}

static void CO_402_mshUnlock(CANopenNodeRTT *app, CO_t *co)
{
    CO_UNLOCK_OD(co->CANmodule);
    (void)rt_mutex_release(&app->lifecycleMutex);
}

/*
 * Publish OD changes before waking co_402, while lifecycle ownership still
 * pins the runtime and semaphore against final teardown.
 */
static void CO_402_mshPublishAndUnlock(CANopenNodeRTT *app, CO_402_device_RTT_t *runtime, CO_t *co)
{
    CO_UNLOCK_OD(co->CANmodule);
    if (runtime->semInitialized == RT_TRUE) {
        (void)rt_sem_release(&runtime->cia402Sem);
    }
    (void)rt_mutex_release(&app->lifecycleMutex);
}

static ODR_t CO_402_mshWriteControlword(CO_402_device_axis_t *axis, uint16_t controlword)
{
    return OD_set_u16(axis->od.controlword, 0U, controlword, false);
}

static int CO_402_mshStatusOne(uint8_t requestedAxis)
{
    CO_402_device_RTT_msh_status_t status;
    CANopenNodeRTT *app;
    CO_402_device_RTT_t *runtime;
    CO_402_device_axis_t *axis;
    CO_t *co;
    rt_err_t ret;
    ODR_t odRet = ODR_OK;

    ret = CO_402_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        rt_kprintf("cia402: runtime is not ready\n");
        return ret;
    }
    if (requestedAxis >= runtime->config.axisCount) {
        uint8_t axisCount = runtime->config.axisCount;

        CO_402_mshUnlock(app, co);
        rt_kprintf("cia402: axis %u is outside configured range 0..%u\n", requestedAxis, axisCount - 1U);
        return -RT_EINVAL;
    }

    axis = &runtime->manager.axes[requestedAxis];
    status.axis = requestedAxis;
    status.state = axis->state;
    status.mode = axis->mode;
    status.requestedMode = axis->requestedModeRaw;
    status.supportedModes = axis->supportedModes;

    if (OD_get_u16(axis->od.controlword, 0U, &status.controlword, false) != ODR_OK
        || OD_get_u16(axis->od.statusword, 0U, &status.statusword, false) != ODR_OK
        || OD_get_i32(axis->od.positionActualValue, 0U, &status.position, false) != ODR_OK
        || OD_get_i32(axis->od.velocityActualValue, 0U, &status.velocity, false) != ODR_OK
        || OD_get_i32(axis->od.targetPosition, 0U, &status.targetPosition, false) != ODR_OK
        || OD_get_i32(axis->od.targetVelocity, 0U, &status.targetVelocity, false) != ODR_OK) {
        odRet = ODR_DEV_INCOMPAT;
    }

    CO_402_mshUnlock(app, co);
    if (odRet != ODR_OK) {
        rt_kprintf("cia402: failed to read axis %u OD snapshot\n", requestedAxis);
        return -RT_ERROR;
    }

    rt_kprintf("axis=%u pds=%s cw=0x%04x sw=0x%04x mode=%s(%d) requested=%d supported=0x%08lx\n",
               status.axis, CO_402_mshStateName(status.state), status.controlword, status.statusword,
               CO_402_mshModeName(status.mode), (int)status.mode, (int)status.requestedMode,
               (unsigned long)status.supportedModes);
    rt_kprintf("  pos=%ld target-pos=%ld vel=%ld target-vel=%ld\n", (long)status.position,
               (long)status.targetPosition, (long)status.velocity, (long)status.targetVelocity);
    return RT_EOK;
}

static int CO_402_mshGetAxisCount(uint8_t *axisCount)
{
    CANopenNodeRTT *app;
    CO_402_device_RTT_t *runtime;
    CO_t *co;
    rt_err_t ret;

    ret = CO_402_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        return ret;
    }

    *axisCount = runtime->config.axisCount;
    CO_402_mshUnlock(app, co);
    return RT_EOK;
}

static int CO_402_mshStatus(int argc, char **argv)
{
    uint8_t axis;
    uint8_t axisCount;
    int ret;

    if (argc == 2) {
        ret = CO_402_mshGetAxisCount(&axisCount);
        if (ret != RT_EOK) {
            rt_kprintf("cia402: runtime is not ready\n");
            return ret;
        }
        for (axis = 0U; axis < axisCount; axis++) {
            ret = CO_402_mshStatusOne(axis);
            if (ret != RT_EOK) {
                return ret;
            }
        }
        return RT_EOK;
    }

    if (argc == 3 && CO_402_mshParseAxis(argv[2], &axis)) {
        return CO_402_mshStatusOne(axis);
    }

    rt_kprintf("usage: cia402 status [axis]\n");
    return -RT_EINVAL;
}

static int CO_402_mshQueueControlword(uint8_t axisIndex, uint16_t controlword)
{
    CANopenNodeRTT *app;
    CO_402_device_RTT_t *runtime;
    CO_t *co;
    rt_err_t ret;
    ODR_t odRet;

    ret = CO_402_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        rt_kprintf("cia402: runtime is not ready\n");
        return ret;
    }
    if (axisIndex >= runtime->config.axisCount) {
        CO_402_mshUnlock(app, co);
        return -RT_EINVAL;
    }

    odRet = CO_402_mshWriteControlword(&runtime->manager.axes[axisIndex], controlword);
    if (odRet != ODR_OK) {
        CO_402_mshUnlock(app, co);
        rt_kprintf("cia402: Controlword write failed (%d)\n", (int)odRet);
        return -RT_ERROR;
    }

    CO_402_mshPublishAndUnlock(app, runtime, co);
    rt_kprintf("cia402: axis=%u Controlword=0x%04x queued\n", axisIndex, controlword);
    return RT_EOK;
}

static int CO_402_mshControlword(int argc, char **argv)
{
    unsigned long parsed;
    uint8_t axisIndex;

    if (argc != 4 || !CO_402_mshParseAxis(argv[2], &axisIndex)
        || !CO_402_mshParseUnsignedLong(argv[3], UINT16_MAX, &parsed)) {
        rt_kprintf("usage: cia402 cw <axis> <value>\n");
        return -RT_EINVAL;
    }

    return CO_402_mshQueueControlword(axisIndex, (uint16_t)parsed);
}

static bool CO_402_mshPdsValue(const char *command, uint16_t *controlword)
{
    if (strcmp(command, "disable") == 0) {
        *controlword = 0x0000U;
    } else if (strcmp(command, "shutdown") == 0) {
        *controlword = 0x0006U;
    } else if (strcmp(command, "switchon") == 0 || strcmp(command, "disableop") == 0) {
        *controlword = 0x0007U;
    } else if (strcmp(command, "enable") == 0) {
        *controlword = 0x000FU;
    } else if (strcmp(command, "quickstop") == 0) {
        *controlword = 0x000BU;
    } else if (strcmp(command, "faultreset") == 0) {
        *controlword = 0x0080U;
    } else {
        return false;
    }

    return true;
}

static int CO_402_mshPds(int argc, char **argv)
{
    uint16_t controlword;
    uint8_t axisIndex;

    if (argc != 4 || !CO_402_mshParseAxis(argv[2], &axisIndex)
        || !CO_402_mshPdsValue(argv[3], &controlword)) {
        rt_kprintf("usage: cia402 pds <axis> <disable|shutdown|switchon|enable|disableop|quickstop|faultreset>\n");
        return -RT_EINVAL;
    }

    return CO_402_mshQueueControlword(axisIndex, controlword);
}

static int CO_402_mshMode(int argc, char **argv)
{
    CANopenNodeRTT *app;
    CO_402_device_RTT_t *runtime;
    CO_402_device_axis_t *axis;
    CO_402_mode_t mode;
    CO_t *co;
    uint8_t axisIndex;
    rt_err_t ret;
    ODR_t odRet;

    if (argc != 4 || !CO_402_mshParseAxis(argv[2], &axisIndex)
        || !CO_402_mshParseMode(argv[3], &mode)) {
        rt_kprintf("usage: cia402 mode <axis> <none|pp|pv|hm>\n");
        return -RT_EINVAL;
    }

    ret = CO_402_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        rt_kprintf("cia402: runtime is not ready\n");
        return ret;
    }
    if (axisIndex >= runtime->config.axisCount) {
        CO_402_mshUnlock(app, co);
        rt_kprintf("cia402: axis %u is outside configured range\n", axisIndex);
        return -RT_EINVAL;
    }

    axis = &runtime->manager.axes[axisIndex];
    if (mode != CO_402_MODE_NONE && (axis->supportedModes & CO_402_modeCapabilityBit(mode)) == 0U) {
        CO_402_mshUnlock(app, co);
        rt_kprintf("cia402: axis %u does not advertise mode %s\n", axisIndex, argv[3]);
        return -RT_ENOSYS;
    }

    odRet = OD_set_i8(axis->od.modesOfOperation, 0U, (int8_t)mode, false);
    if (odRet != ODR_OK) {
        CO_402_mshUnlock(app, co);
        rt_kprintf("cia402: mode request write failed (%d)\n", (int)odRet);
        return -RT_ERROR;
    }

    CO_402_mshPublishAndUnlock(app, runtime, co);
    rt_kprintf("cia402: axis=%u mode request=%s(%d) queued\n", axisIndex,
               CO_402_mshModeName(mode), (int)mode);
    return RT_EOK;
}

static int CO_402_mshSetBit(uint8_t axisIndex, uint16_t bit, bool set, bool requireRisingEdge)
{
    CANopenNodeRTT *app;
    CO_402_device_RTT_t *runtime;
    CO_402_device_axis_t *axis;
    CO_t *co;
    uint16_t controlword;
    rt_err_t ret;
    ODR_t odRet;

    ret = CO_402_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        return ret;
    }
    if (axisIndex >= runtime->config.axisCount) {
        CO_402_mshUnlock(app, co);
        return -RT_EINVAL;
    }

    axis = &runtime->manager.axes[axisIndex];
    odRet = OD_get_u16(axis->od.controlword, 0U, &controlword, false);
    if (odRet == ODR_OK && requireRisingEdge && (controlword & bit) != 0U) {
        CO_402_mshUnlock(app, co);
        rt_kprintf("cia402: axis %u bit is already high; clear it before requesting a new edge\n", axisIndex);
        return -RT_EBUSY;
    }
    if (odRet == ODR_OK && requireRisingEdge && bit == CO_402_CONTROLWORD_MODE_BIT4) {
        CO_402_mode_raw_t requestedRaw = 0;
        CO_402_mode_t requestedMode = CO_402_MODE_NONE;
        bool modeUsesBit4;
        bool previousHigh = false;

        /* A mode reset samples live bit 4; reject an early high level so the requested edge cannot be lost. */
        odRet = OD_get_i8(axis->od.modesOfOperation, 0U, &requestedRaw, false);
        modeUsesBit4 = axis->mode == CO_402_MODE_PROFILE_POSITION || axis->mode == CO_402_MODE_HOMING;
        if (odRet != ODR_OK || !CO_402_modeFromRaw(requestedRaw, &requestedMode)
            || requestedMode != axis->mode || !modeUsesBit4
            || axis->state != CO_402_STATE_OPERATION_ENABLED
            || axis->pendingExitMode != CO_402_MODE_NONE || axis->pendingEnterMode != CO_402_MODE_NONE) {
            CO_402_mshUnlock(app, co);
            rt_kprintf("cia402: axis %u is not in a stable operation-enabled PP/HM mode; retry start\n",
                       axisIndex);
            return -RT_EBUSY;
        }

#if CO_402_CONFIG_MODE_PP
        previousHigh = axis->mode == CO_402_MODE_PROFILE_POSITION && axis->pp.previousNewSetPoint;
#endif /* CO_402_CONFIG_MODE_PP */
#if CO_402_CONFIG_MODE_HM
        previousHigh = previousHigh || (axis->mode == CO_402_MODE_HOMING && axis->hm.previousStart);
#endif /* CO_402_CONFIG_MODE_HM */
        if (previousHigh) {
            CO_402_mshUnlock(app, co);
            rt_kprintf("cia402: axis %u supervisor has not consumed the bit-4 clear yet; retry start\n", axisIndex);
            return -RT_EBUSY;
        }
    }
    if (odRet == ODR_OK) {
        controlword = set ? (uint16_t)(controlword | bit) : (uint16_t)(controlword & (uint16_t)~bit);
        odRet = CO_402_mshWriteControlword(axis, controlword);
    }

    if (odRet != ODR_OK) {
        CO_402_mshUnlock(app, co);
        rt_kprintf("cia402: Controlword update failed (%d)\n", (int)odRet);
        return -RT_ERROR;
    }

    CO_402_mshPublishAndUnlock(app, runtime, co);
    return RT_EOK;
}

static int CO_402_mshStart(int argc, char **argv)
{
    uint8_t axisIndex;
    int ret;

    if (argc != 3 || !CO_402_mshParseAxis(argv[2], &axisIndex)) {
        rt_kprintf("usage: cia402 start <axis>\n");
        return -RT_EINVAL;
    }

    ret = CO_402_mshSetBit(axisIndex, CO_402_CONTROLWORD_MODE_BIT4, true, true);
    if (ret == RT_EOK) {
        rt_kprintf("cia402: axis=%u mode start/set-point edge queued\n", axisIndex);
    }
    return ret;
}

static int CO_402_mshStop(int argc, char **argv)
{
    uint8_t axisIndex;
    int ret;

    if (argc != 3 || !CO_402_mshParseAxis(argv[2], &axisIndex)) {
        rt_kprintf("usage: cia402 stop <axis>\n");
        return -RT_EINVAL;
    }

    ret = CO_402_mshSetBit(axisIndex, CO_402_CONTROLWORD_MODE_BIT4, false, false);
    if (ret == RT_EOK) {
        rt_kprintf("cia402: axis=%u mode bit 4 cleared\n", axisIndex);
    }
    return ret;
}

static int CO_402_mshHalt(int argc, char **argv)
{
    unsigned long value;
    uint8_t axisIndex;
    int ret;

    if (argc != 4 || !CO_402_mshParseAxis(argv[2], &axisIndex)
        || !CO_402_mshParseUnsignedLong(argv[3], 1U, &value)) {
        rt_kprintf("usage: cia402 halt <axis> <0|1>\n");
        return -RT_EINVAL;
    }

    ret = CO_402_mshSetBit(axisIndex, CO_402_CONTROLWORD_HALT, value != 0U, false);
    if (ret == RT_EOK) {
        rt_kprintf("cia402: axis=%u halt=%lu queued\n", axisIndex, value);
    }
    return ret;
}

static int CO_402_mshPp(int argc, char **argv)
{
    CANopenNodeRTT *app;
    CO_402_device_RTT_t *runtime;
    CO_402_device_axis_t *axis;
    CO_t *co;
    long target;
    unsigned long velocity;
    unsigned long acceleration;
    unsigned long deceleration;
    unsigned long relative = 0U;
    unsigned long immediate = 0U;
    uint8_t axisIndex;
    uint16_t controlword;
    rt_err_t ret;
    ODR_t odRet;

    if ((argc < 7 || argc > 9) || !CO_402_mshParseAxis(argv[2], &axisIndex)
        || !CO_402_mshParseLong(argv[3], INT32_MIN, INT32_MAX, &target)
        || !CO_402_mshParseUnsignedLong(argv[4], UINT32_MAX, &velocity)
        || !CO_402_mshParseUnsignedLong(argv[5], UINT32_MAX, &acceleration)
        || !CO_402_mshParseUnsignedLong(argv[6], UINT32_MAX, &deceleration)
        || (argc >= 8 && !CO_402_mshParseUnsignedLong(argv[7], 1U, &relative))
        || (argc >= 9 && !CO_402_mshParseUnsignedLong(argv[8], 1U, &immediate))) {
        rt_kprintf("usage: cia402 pp <axis> <target> <velocity> <accel> <decel> [relative:0|1] [immediate:0|1]\n");
        return -RT_EINVAL;
    }

    ret = CO_402_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        rt_kprintf("cia402: runtime is not ready\n");
        return ret;
    }
    if (axisIndex >= runtime->config.axisCount) {
        CO_402_mshUnlock(app, co);
        rt_kprintf("cia402: axis %u is outside configured range\n", axisIndex);
        return -RT_EINVAL;
    }

    axis = &runtime->manager.axes[axisIndex];
    if ((axis->supportedModes & CO_402_SUPPORTED_MODE_PP) == 0U || axis->od.profileVelocity == NULL
        || axis->od.profileAcceleration == NULL || axis->od.profileDeceleration == NULL) {
        CO_402_mshUnlock(app, co);
        rt_kprintf("cia402: PP is not available on axis %u\n", axisIndex);
        return -RT_ENOSYS;
    }

    odRet = OD_set_i32(axis->od.targetPosition, 0U, (int32_t)target, false);
    if (odRet == ODR_OK) {
        odRet = OD_set_u32(axis->od.profileVelocity, 0U, (uint32_t)velocity, false);
    }
    if (odRet == ODR_OK) {
        odRet = OD_set_u32(axis->od.profileAcceleration, 0U, (uint32_t)acceleration, false);
    }
    if (odRet == ODR_OK) {
        odRet = OD_set_u32(axis->od.profileDeceleration, 0U, (uint32_t)deceleration, false);
    }
    if (odRet == ODR_OK) {
        odRet = OD_get_u16(axis->od.controlword, 0U, &controlword, false);
    }
    if (odRet == ODR_OK) {
        /* PP parameter setup owns bits 5/6 only; shared bit 4 is controlled explicitly by stop/start. */
        controlword &= (uint16_t)~(CO_402_CONTROLWORD_PP_CHANGE_IMMEDIATELY
                                  | CO_402_CONTROLWORD_PP_RELATIVE);
        if (relative != 0U) {
            controlword |= CO_402_CONTROLWORD_PP_RELATIVE;
        }
        if (immediate != 0U) {
            controlword |= CO_402_CONTROLWORD_PP_CHANGE_IMMEDIATELY;
        }
        odRet = CO_402_mshWriteControlword(axis, controlword);
    }

    if (odRet != ODR_OK) {
        CO_402_mshUnlock(app, co);
        rt_kprintf("cia402: PP parameter write failed (%d)\n", (int)odRet);
        return -RT_ERROR;
    }

    CO_402_mshPublishAndUnlock(app, runtime, co);
    rt_kprintf("cia402: axis=%u PP parameters queued; use stop then start for a fresh set-point edge\n",
               axisIndex);
    return RT_EOK;
}

static int CO_402_mshPv(int argc, char **argv)
{
    CANopenNodeRTT *app;
    CO_402_device_RTT_t *runtime;
    CO_402_device_axis_t *axis;
    CO_t *co;
    long targetVelocity;
    unsigned long acceleration;
    unsigned long deceleration;
    uint8_t axisIndex;
    rt_err_t ret;
    ODR_t odRet;

    if (argc != 6 || !CO_402_mshParseAxis(argv[2], &axisIndex)
        || !CO_402_mshParseLong(argv[3], INT32_MIN, INT32_MAX, &targetVelocity)
        || !CO_402_mshParseUnsignedLong(argv[4], UINT32_MAX, &acceleration)
        || !CO_402_mshParseUnsignedLong(argv[5], UINT32_MAX, &deceleration)) {
        rt_kprintf("usage: cia402 pv <axis> <target-velocity> <accel> <decel>\n");
        return -RT_EINVAL;
    }

    ret = CO_402_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        rt_kprintf("cia402: runtime is not ready\n");
        return ret;
    }
    if (axisIndex >= runtime->config.axisCount) {
        CO_402_mshUnlock(app, co);
        rt_kprintf("cia402: axis %u is outside configured range\n", axisIndex);
        return -RT_EINVAL;
    }

    axis = &runtime->manager.axes[axisIndex];
    if ((axis->supportedModes & CO_402_SUPPORTED_MODE_PV) == 0U || axis->od.profileAcceleration == NULL
        || axis->od.profileDeceleration == NULL) {
        CO_402_mshUnlock(app, co);
        rt_kprintf("cia402: PV is not available on axis %u\n", axisIndex);
        return -RT_ENOSYS;
    }

    odRet = OD_set_i32(axis->od.targetVelocity, 0U, (int32_t)targetVelocity, false);
    if (odRet == ODR_OK) {
        odRet = OD_set_u32(axis->od.profileAcceleration, 0U, (uint32_t)acceleration, false);
    }
    if (odRet == ODR_OK) {
        odRet = OD_set_u32(axis->od.profileDeceleration, 0U, (uint32_t)deceleration, false);
    }

    if (odRet != ODR_OK) {
        CO_402_mshUnlock(app, co);
        rt_kprintf("cia402: PV parameter write failed (%d)\n", (int)odRet);
        return -RT_ERROR;
    }

    CO_402_mshPublishAndUnlock(app, runtime, co);
    rt_kprintf("cia402: axis=%u PV target=%ld queued\n", axisIndex, targetVelocity);
    return RT_EOK;
}

static int CO_402_mshHm(int argc, char **argv)
{
    CANopenNodeRTT *app;
    CO_402_device_RTT_t *runtime;
    CO_402_device_axis_t *axis;
    CO_t *co;
    long method;
    long offset;
    unsigned long switchSpeed;
    unsigned long zeroSpeed;
    unsigned long acceleration;
    uint8_t axisIndex;
    rt_err_t ret;
    ODR_t odRet;

    if (argc != 8 || !CO_402_mshParseAxis(argv[2], &axisIndex)
        || !CO_402_mshParseLong(argv[3], INT8_MIN, INT8_MAX, &method)
        || !CO_402_mshParseLong(argv[4], INT32_MIN, INT32_MAX, &offset)
        || !CO_402_mshParseUnsignedLong(argv[5], UINT32_MAX, &switchSpeed)
        || !CO_402_mshParseUnsignedLong(argv[6], UINT32_MAX, &zeroSpeed)
        || !CO_402_mshParseUnsignedLong(argv[7], UINT32_MAX, &acceleration)) {
        rt_kprintf("usage: cia402 hm <axis> <method> <offset> <switch-speed> <zero-speed> <accel>\n");
        return -RT_EINVAL;
    }

    ret = CO_402_mshLock(&app, &runtime, &co);
    if (ret != RT_EOK) {
        rt_kprintf("cia402: runtime is not ready\n");
        return ret;
    }
    if (axisIndex >= runtime->config.axisCount) {
        CO_402_mshUnlock(app, co);
        rt_kprintf("cia402: axis %u is outside configured range\n", axisIndex);
        return -RT_EINVAL;
    }

    axis = &runtime->manager.axes[axisIndex];
    if ((axis->supportedModes & CO_402_SUPPORTED_MODE_HM) == 0U || axis->od.homeOffset == NULL
        || axis->od.homingMethod == NULL || axis->od.homingSpeeds == NULL || axis->od.homingAcceleration == NULL) {
        CO_402_mshUnlock(app, co);
        rt_kprintf("cia402: HM is not available on axis %u\n", axisIndex);
        return -RT_ENOSYS;
    }

    odRet = OD_set_i8(axis->od.homingMethod, 0U, (int8_t)method, false);
    if (odRet == ODR_OK) {
        odRet = OD_set_i32(axis->od.homeOffset, 0U, (int32_t)offset, false);
    }
    if (odRet == ODR_OK) {
        odRet = OD_set_u32(axis->od.homingSpeeds, 1U, (uint32_t)switchSpeed, false);
    }
    if (odRet == ODR_OK) {
        odRet = OD_set_u32(axis->od.homingSpeeds, 2U, (uint32_t)zeroSpeed, false);
    }
    if (odRet == ODR_OK) {
        odRet = OD_set_u32(axis->od.homingAcceleration, 0U, (uint32_t)acceleration, false);
    }

    if (odRet != ODR_OK) {
        CO_402_mshUnlock(app, co);
        rt_kprintf("cia402: HM parameter write failed (%d)\n", (int)odRet);
        return -RT_ERROR;
    }

    CO_402_mshPublishAndUnlock(app, runtime, co);
    rt_kprintf("cia402: axis=%u HM parameters queued; use stop then start for a fresh homing edge\n",
               axisIndex);
    return RT_EOK;
}

static int cia402(int argc, char **argv)
{
    if (argc < 2 || strcmp(argv[1], "help") == 0) {
        CO_402_mshPrintUsage();
        return argc < 2 ? -RT_EINVAL : RT_EOK;
    }
    if (strcmp(argv[1], "status") == 0) {
        return CO_402_mshStatus(argc, argv);
    }
    if (strcmp(argv[1], "cw") == 0) {
        return CO_402_mshControlword(argc, argv);
    }
    if (strcmp(argv[1], "pds") == 0) {
        return CO_402_mshPds(argc, argv);
    }
    if (strcmp(argv[1], "mode") == 0) {
        return CO_402_mshMode(argc, argv);
    }
    if (strcmp(argv[1], "pp") == 0) {
        return CO_402_mshPp(argc, argv);
    }
    if (strcmp(argv[1], "pv") == 0) {
        return CO_402_mshPv(argc, argv);
    }
    if (strcmp(argv[1], "hm") == 0) {
        return CO_402_mshHm(argc, argv);
    }
    if (strcmp(argv[1], "start") == 0) {
        return CO_402_mshStart(argc, argv);
    }
    if (strcmp(argv[1], "stop") == 0) {
        return CO_402_mshStop(argc, argv);
    }
    if (strcmp(argv[1], "halt") == 0) {
        return CO_402_mshHalt(argc, argv);
    }

    CO_402_mshPrintUsage();
    return -RT_EINVAL;
}
MSH_CMD_EXPORT(cia402, control and inspect the local CiA 402 Device supervisor);

void CO_402_device_RTT_mshBind(CANopenNodeRTT *app, CO_402_device_RTT_t *runtime)
{
    CO_402_msh.app = app;
    CO_402_msh.runtime = runtime;
}

void CO_402_device_RTT_mshUnbind(CANopenNodeRTT *app, CO_402_device_RTT_t *runtime)
{
    if (CO_402_msh.app == app && CO_402_msh.runtime == runtime) {
        CO_402_msh.app = NULL;
        CO_402_msh.runtime = NULL;
    }
}

#endif /* defined(PKG_CANOPENNODE_CIA402_DEVICE_RTT_MSH) */
