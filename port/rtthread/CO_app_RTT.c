/**
 * @file CO_app_RTT.c
 * @brief CANopenNode RT-Thread application runtime wrapper.
 * @details This file implements the CANopenNode application lifecycle, communication reset,
 *          realtime processing timer, realtime object processing, and optional LED/storage
 *          glue for RT-Thread.
 * @author wdfk-prog ()
 * @version 1.0.0
 * @date 2026.07.04
 *
 * @copyright Copyright (c) 2026
 *
 * @note :
 * @par Change Log:
 * Date       Version Author      Description
 * 2026.07.04 1.0.0   wdfk-prog   first version
 */

/* Private define ------------------------------------------------------------*/
#define LOG_TAG                         "canopen.app"
#define LOG_LVL                         LOG_LVL_DBG

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/
#include "CO_app_RTT.h"
#include "co_rtt_log.h"
#include "OD.h"

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0
#include "CO_storage_RTT.h"
#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */

#include <rthw.h>
#if defined(PKG_CANOPENNODE_LEDS_USING_RTT_PIN)
#include <drivers/pin.h>
#endif /* defined(PKG_CANOPENNODE_LEDS_USING_RTT_PIN) */
#include <stdint.h>
#include <string.h>

#define CO_APP_RTT_NMT_CONTROL          (CO_NMT_STARTUP_TO_OPERATIONAL \
                                       | CO_NMT_ERR_ON_ERR_REG \
                                       | CO_ERR_REG_GENERIC_ERR \
                                       | CO_ERR_REG_COMMUNICATION)

#ifndef CO_APP_RTT_OD_STATUS_BITS
#define CO_APP_RTT_OD_STATUS_BITS       NULL
#endif /* CO_APP_RTT_OD_STATUS_BITS */

#if defined(PKG_CANOPENNODE_APP_SDO_CLI_BLOCK) && (((CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK) != 0)
#define CO_APP_RTT_SDO_CLI_BLOCK        true
#else
#define CO_APP_RTT_SDO_CLI_BLOCK        false
#endif /* defined(PKG_CANOPENNODE_APP_SDO_CLI_BLOCK) && (((CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK) != 0) */

#if defined(PKG_CANOPENNODE_LEDS_USING_RTT_PIN)
#if defined(PKG_CANOPENNODE_LEDS_RTT_PIN_RUN_ACTIVE_HIGH)
#define CO_APP_RTT_LED_RUN_ON_LEVEL     PIN_HIGH
#define CO_APP_RTT_LED_RUN_OFF_LEVEL    PIN_LOW
#else
#define CO_APP_RTT_LED_RUN_ON_LEVEL     PIN_LOW
#define CO_APP_RTT_LED_RUN_OFF_LEVEL    PIN_HIGH
#endif /* defined(PKG_CANOPENNODE_LEDS_RTT_PIN_RUN_ACTIVE_HIGH) */
#if defined(PKG_CANOPENNODE_LEDS_RTT_PIN_ERROR_ACTIVE_HIGH)
#define CO_APP_RTT_LED_ERROR_ON_LEVEL   PIN_HIGH
#define CO_APP_RTT_LED_ERROR_OFF_LEVEL  PIN_LOW
#else
#define CO_APP_RTT_LED_ERROR_ON_LEVEL   PIN_LOW
#define CO_APP_RTT_LED_ERROR_OFF_LEVEL  PIN_HIGH
#endif /* defined(PKG_CANOPENNODE_LEDS_RTT_PIN_ERROR_ACTIVE_HIGH) */
#endif /* defined(PKG_CANOPENNODE_LEDS_USING_RTT_PIN) */

/* Private function prototypes -----------------------------------------------*/

#ifdef CO_MULTIPLE_OD
/**
 * @brief Complete generated OD configuration with RT-Thread wrapper object counts.
 *
 * OD_INIT_CONFIG() is generated from the demo Object Dictionary and may omit
 * CANopenNode objects that do not own dedicated OD entries, such as LEDs and LSS.
 * This function keeps those counts aligned with the final CO_CONFIG_* bitmasks.
 *
 * @param config CANopenNode runtime configuration to update.
 */
static void co_app_rtt_apply_wrapper_config(CO_config_t *config)
{
#if ((CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE) != 0
    config->CNT_LEDS = 1U;
#endif /* ((CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE) != 0 */

#if ((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0
    config->CNT_LSS_SLV = 1U;
#endif /* ((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0 */

#if ((CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER) != 0
    config->CNT_LSS_MST = 1U;
#endif /* ((CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER) != 0 */
}
#endif /* CO_MULTIPLE_OD */

/**
 * @brief Convert configured realtime period to at least one RT-Thread tick.
 *
 * The configured period is converted directly from microseconds with 64-bit
 * arithmetic and rounded up to whole OS ticks. The effective runtime period is
 * still limited by RT-Thread tick resolution.
 *
 * @param actualPeriodUs Actual period represented by the returned tick count, in microseconds.
 * @return Equivalent RT-Thread tick count rounded up to at least one tick.
 */
static rt_tick_t co_app_rtt_timer_period_ticks(uint32_t *actualPeriodUs)
{
    rt_tick_t ticks;
    uint64_t numerator = ((uint64_t)PKG_CANOPENNODE_TIMER_PERIOD_US * RT_TICK_PER_SECOND) + 999999ULL;

    ticks = (rt_tick_t)(numerator / 1000000ULL);
    if (ticks == 0U) {
        ticks = 1U;
    }

    if (actualPeriodUs != NULL) {
        *actualPeriodUs = (uint32_t)((((uint64_t)ticks * 1000000ULL) + RT_TICK_PER_SECOND - 1ULL) / RT_TICK_PER_SECOND);
    }

    return ticks;
}

#if defined(PKG_CANOPENNODE_LEDS_USING_RTT_PIN)
/**
 * @brief Check whether an RT-Thread PIN number is configured.
 *
 * @param pin RT-Thread PIN number from Kconfig.
 * @return true if @p pin is configured, otherwise false.
 */
static rt_bool_t co_app_rtt_led_pin_is_valid(int pin)
{
    return (pin >= 0) ? RT_TRUE : RT_FALSE;
}

/**
 * @brief Configure one CANopen LED PIN output.
 *
 * @param pin RT-Thread PIN number from Kconfig.
 * @param offLevel Electrical level for the logical LED-off state.
 */
static void co_app_rtt_led_pin_init_one(int pin, rt_base_t offLevel)
{
    rt_pin_mode(pin, PIN_MODE_OUTPUT);
    rt_pin_write(pin, offLevel);
}

/**
 * @brief Configure all enabled CANopen LED PIN outputs.
 */
static void co_app_rtt_led_pin_init(void)
{
    if (co_app_rtt_led_pin_is_valid(PKG_CANOPENNODE_LEDS_RTT_PIN_RUN) == RT_TRUE) {
        co_app_rtt_led_pin_init_one(PKG_CANOPENNODE_LEDS_RTT_PIN_RUN, CO_APP_RTT_LED_RUN_OFF_LEVEL);
    }
    if (co_app_rtt_led_pin_is_valid(PKG_CANOPENNODE_LEDS_RTT_PIN_ERROR) == RT_TRUE) {
        co_app_rtt_led_pin_init_one(PKG_CANOPENNODE_LEDS_RTT_PIN_ERROR, CO_APP_RTT_LED_ERROR_OFF_LEVEL);
    }
}

/**
 * @brief Write one CANopen LED PIN output.
 *
 * @param pin RT-Thread PIN number from Kconfig.
 * @param on Non-zero turns the logical CANopen LED on.
 * @param onLevel Electrical level for the logical LED-on state.
 * @param offLevel Electrical level for the logical LED-off state.
 */
static void co_app_rtt_led_pin_write_one(int pin, uint8_t on, rt_base_t onLevel, rt_base_t offLevel)
{
    rt_pin_write(pin, (on != 0U) ? onLevel : offLevel);
}

/**
 * @brief Drive configured RT-Thread PIN outputs from CANopen LED state.
 *
 * @param app CANopenNode RT-Thread application instance.
 */
static void co_app_rtt_led_pin_update(const CANopenNodeRTT *app)
{
    if (co_app_rtt_led_pin_is_valid(PKG_CANOPENNODE_LEDS_RTT_PIN_RUN) == RT_TRUE) {
        co_app_rtt_led_pin_write_one(PKG_CANOPENNODE_LEDS_RTT_PIN_RUN, app->outStatusLEDGreen,
                                     CO_APP_RTT_LED_RUN_ON_LEVEL, CO_APP_RTT_LED_RUN_OFF_LEVEL);
    }
    if (co_app_rtt_led_pin_is_valid(PKG_CANOPENNODE_LEDS_RTT_PIN_ERROR) == RT_TRUE) {
        co_app_rtt_led_pin_write_one(PKG_CANOPENNODE_LEDS_RTT_PIN_ERROR, app->outStatusLEDRed,
                                     CO_APP_RTT_LED_ERROR_ON_LEVEL, CO_APP_RTT_LED_ERROR_OFF_LEVEL);
    }
}
#else
#define co_app_rtt_led_pin_init()       do { } while (0)
#define co_app_rtt_led_pin_update(app)  do { (void)(app); } while (0)
#endif /* defined(PKG_CANOPENNODE_LEDS_USING_RTT_PIN) */

/**
 * @brief Create the CANopenNode object for an application instance.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
static rt_err_t co_app_rtt_new_stack(CANopenNodeRTT *app)
{
    CO_config_t *config_ptr = NULL;
    uint32_t heap_memory_used = 0U;

#ifdef CO_MULTIPLE_OD
    memset(&app->coConfig, 0, sizeof(app->coConfig));
    OD_INIT_CONFIG(app->coConfig);
    co_app_rtt_apply_wrapper_config(&app->coConfig);
    config_ptr = &app->coConfig;
#endif /* CO_MULTIPLE_OD */

    app->canOpenStack = CO_new(config_ptr, &heap_memory_used);
    if (app->canOpenStack == NULL) {
        CO_RTT_LOG_E("CO_new failed: dev=%s", app->canName);
        return -RT_ENOMEM;
    }

    CO_RTT_LOG_I("allocated %lu bytes for CANopen objects: dev=%s", (unsigned long)heap_memory_used, app->canName);

    return RT_EOK;
}

#if (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0)
/**
 * @brief Initialize the LSS slave object for an application instance.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @return CO_ERROR_NO on success, otherwise a CANopenNode error code.
 */
static CO_ReturnError_t co_app_rtt_lss_init(CANopenNodeRTT *app)
{
    CO_LSS_address_t lss_address;
    CO_ReturnError_t err;

    memset(&lss_address, 0, sizeof(lss_address));
    (void)OD_get_u32(OD_ENTRY_H1018, 1U, &lss_address.identity.vendorID, true);
    (void)OD_get_u32(OD_ENTRY_H1018, 2U, &lss_address.identity.productCode, true);
    (void)OD_get_u32(OD_ENTRY_H1018, 3U, &lss_address.identity.revisionNumber, true);
    (void)OD_get_u32(OD_ENTRY_H1018, 4U, &lss_address.identity.serialNumber, true);

    err = CO_LSSinit(app->canOpenStack, &lss_address, &app->lssPendingNodeID, &app->lssPendingBitrate);
    if (err == CO_ERROR_NO) {
        app->activeNodeID = app->lssPendingNodeID;
        app->baudrate = app->lssPendingBitrate;
    }

    return err;
}
#endif /* (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0) */

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0
/**
 * @brief Initialize RT-Thread storage for the selected generated persistent OD groups.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @param storageInitError Optional error detail. Stores a one-based failed entry index on read failure.
 * @return CO_ERROR_NO on success, otherwise a CANopenNode error code.
 */
static CO_ReturnError_t co_app_rtt_storage_init(CANopenNodeRTT *app, uint32_t *storageInitError)
{
    CO_ReturnError_t err;
    uint8_t entriesCount = 0U;
    uint32_t storageInitErrorLocal = 0U;

    if (storageInitError == NULL) {
        storageInitError = &storageInitErrorLocal;
    }
    *storageInitError = 0U;

#if defined(PKG_CANOPENNODE_STORAGE_PERSIST_COMM)
    co_storage_rtt_entry_init(&app->storageEntries[entriesCount], &OD_PERSIST_COMM, sizeof(OD_PERSIST_COMM),
                              CO_APP_RTT_STORAGE_SUB_INDEX_COMM, CO_storage_cmd | CO_storage_restore);
    entriesCount++;
#endif /* defined(PKG_CANOPENNODE_STORAGE_PERSIST_COMM) */

#if defined(PKG_CANOPENNODE_STORAGE_PERSIST_APP)
    co_storage_rtt_entry_init(&app->storageEntries[entriesCount], &OD_PERSIST_APP, sizeof(OD_PERSIST_APP),
                              CO_APP_RTT_STORAGE_SUB_INDEX_APP, CO_storage_cmd | CO_storage_restore);
    entriesCount++;
#endif /* defined(PKG_CANOPENNODE_STORAGE_PERSIST_APP) */

#if defined(PKG_CANOPENNODE_STORAGE_PERSIST_MANU)
    co_storage_rtt_entry_init(&app->storageEntries[entriesCount], &OD_PERSIST_MANU, sizeof(OD_PERSIST_MANU),
                              CO_APP_RTT_STORAGE_SUB_INDEX_MANU, CO_storage_cmd | CO_storage_restore);
    entriesCount++;
#endif /* defined(PKG_CANOPENNODE_STORAGE_PERSIST_MANU) */

    err = co_storage_rtt_init(&app->storage, app->canOpenStack->CANmodule, OD_ENTRY_H1010, OD_ENTRY_H1011,
                              app->storageEntries, entriesCount, app->canName, storageInitError);
    if (err != CO_ERROR_NO) {
        CO_RTT_LOG_E("CO storage init failed: dev=%s err=%d entry=%lu", app->canName, err,
                     (unsigned long)*storageInitError);
    }

    return err;
}
#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */

/**
 * @brief Reset CANopen communication for an application instance.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
static rt_err_t co_app_rtt_reset_communication(CANopenNodeRTT *app)
{
    CO_t *co = app->canOpenStack;
    CO_ReturnError_t err;
    uint32_t err_info = 0U;
#if (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0)
    uint16_t can_bitrate = app->lssPendingBitrate;
#else
    uint16_t can_bitrate = app->baudrate;
#endif /* (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0) */

    CO_RTT_LOG_W("CANopen reset communication: dev=%s node=%u bitrate=%u", app->canName, app->desiredNodeID,
                 can_bitrate);

    co->CANmodule->CANnormal = false;
    CO_CANsetConfigurationMode((void *)app->canName);
    CO_CANmodule_disable(co->CANmodule);

    err = CO_CANinit(co, (void *)app->canName, can_bitrate);
    if (err != CO_ERROR_NO) {
        CO_RTT_LOG_E("CO_CANinit failed: dev=%s err=%d", app->canName, err);
        return -RT_ERROR;
    }

#if (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0)
    err = co_app_rtt_lss_init(app);
    if (err != CO_ERROR_NO) {
        CO_RTT_LOG_E("CO_LSSinit failed: dev=%s err=%d", app->canName, err);
        return -RT_ERROR;
    }
#else
    app->activeNodeID = app->desiredNodeID;
#endif /* (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0) */

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0
    bool_t storageDataCorrupt = false;
    uint32_t storageInitError = 0U;
    err = co_app_rtt_storage_init(app, &storageInitError);
    if (err == CO_ERROR_DATA_CORRUPT) {
        storageDataCorrupt = true;
        CO_RTT_LOG_W("CO storage data corrupt: dev=%s entry=%lu", app->canName, (unsigned long)storageInitError);
    } else if (err != CO_ERROR_NO) {
        return -RT_ERROR;
    }
#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */

    err = CO_CANopenInit(co, NULL, NULL, OD, CO_APP_RTT_OD_STATUS_BITS, CO_APP_RTT_NMT_CONTROL,
                         PKG_CANOPENNODE_APP_FIRST_HB_TIME_MS, PKG_CANOPENNODE_APP_SDO_SRV_TIMEOUT_MS,
                         PKG_CANOPENNODE_APP_SDO_CLI_TIMEOUT_MS, CO_APP_RTT_SDO_CLI_BLOCK, app->activeNodeID,
                         &err_info);
    if ((err != CO_ERROR_NO) && (err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS)) {
        CO_RTT_LOG_E("CO_CANopenInit failed: dev=%s err=%d info=0x%08lx", app->canName, err,
                     (unsigned long)err_info);
        return -RT_ERROR;
    }
#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0
    if (storageDataCorrupt && (err == CO_ERROR_NO)) {
        CO_errorReport(co->em, CO_EM_NON_VOLATILE_MEMORY, CO_EMC_HARDWARE, storageInitError);
    }
#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */

#if (((CO_CONFIG_GFC) & CO_CONFIG_GFC_ENABLE) != 0) || (((CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE) != 0)
    err_info = 0U;
    err = CO_CANopenInitSRDO(co, co->em, OD, app->activeNodeID, &err_info);
    if ((err != CO_ERROR_NO) && (err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS)) {
        CO_RTT_LOG_E("CO_CANopenInitSRDO failed: dev=%s err=%d info=0x%08lx", app->canName, err,
                     (unsigned long)err_info);
        return -RT_ERROR;
    }
#endif /* (((CO_CONFIG_GFC) & CO_CONFIG_GFC_ENABLE) != 0) || (((CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE) != 0) */

    err_info = 0U;
    err = CO_CANopenInitPDO(co, co->em, OD, app->activeNodeID, &err_info);
    if ((err != CO_ERROR_NO) && (err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS)) {
        CO_RTT_LOG_E("CO_CANopenInitPDO failed: dev=%s err=%d info=0x%08lx", app->canName, err,
                     (unsigned long)err_info);
        return -RT_ERROR;
    }

    CO_CANsetNormalMode(co->CANmodule);
    if (!co->CANmodule->CANnormal) {
        CO_RTT_LOG_E("CAN normal mode failed: dev=%s", app->canName);
        return -RT_ERROR;
    }

    app->timeOldMs = rt_tick_get_millisecond();
    app->lastRtTickMs = app->timeOldMs;
    CO_RTT_LOG_I("CANopen running: dev=%s node=%u bitrate=%u", app->canName, app->activeNodeID, app->baudrate);

    return RT_EOK;
}

/**
 * @brief Delete and recreate the CANopenNode object for communication reset.
 *
 * The caller must hold lifecycleMutex. RT_IPC_CMD_RESET only clears realtime
 * wakeups that have not been taken yet; it cannot wait for a realtime thread
 * already using app->canOpenStack. The lifecycle mutex provides that object
 * lifetime boundary, while CANnormal remains the CANopen processing-state gate.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
static rt_err_t co_app_rtt_recreate_stack(CANopenNodeRTT *app)
{
    rt_err_t ret;

    if (app->canOpenStack != NULL) {
        CO_CANsetConfigurationMode((void *)app->canName);
        CO_CANmodule_disable(app->canOpenStack->CANmodule);
        CO_delete(app->canOpenStack);
        app->canOpenStack = NULL;
    }

    ret = co_app_rtt_new_stack(app);
    if (ret != RT_EOK) {
        return ret;
    }

    ret = co_app_rtt_reset_communication(app);
    if (ret != RT_EOK) {
        CO_CANmodule_disable(app->canOpenStack->CANmodule);
        CO_delete(app->canOpenStack);
        app->canOpenStack = NULL;
    }

    return ret;
}

/**
 * @brief Reset queued realtime wakeups after the realtime timer is stopped.
 *
 * RT_IPC_CMD_RESET only removes semaphore tokens that have not been taken yet.
 * It is paired with lifecycleMutex in the reset path, because a realtime thread
 * may already have taken a token and be using app->canOpenStack.
 *
 * @param app CANopenNode RT-Thread application instance.
 */
static void co_app_rtt_reset_realtime_sem(CANopenNodeRTT *app)
{
    (void)rt_sem_control(&app->rtSem, RT_IPC_CMD_RESET, RT_NULL);
}

/**
 * @brief Mainline CANopen worker thread entry.
 *
 * @param parameter CANopenNode RT-Thread application instance.
 */
static void co_app_rtt_main_thread_entry(void *parameter)
{
    CANopenNodeRTT *app = (CANopenNodeRTT *)parameter;
    CO_t *co = app->canOpenStack;
    uint32_t time_current_ms;

    if (co == NULL) {
        CO_RTT_LOG_E("mainline thread started without CANopen stack: dev=%s", app->canName);
        return;
    }

    while (1) {
        CO_NMT_reset_cmd_t reset_status;
        uint32_t time_difference_us;
        rt_err_t ret;

        rt_thread_mdelay(1);

        time_current_ms = rt_tick_get_millisecond();
        time_difference_us = (time_current_ms - app->timeOldMs) * 1000U;
        if (time_difference_us == 0U) {
            continue;
        }
        app->timeOldMs = time_current_ms;

        reset_status = CO_process(co, false, time_difference_us, NULL);
#if ((CO_CONFIG_TRACE) & CO_CONFIG_TRACE_ENABLE) != 0
        /*
        * TODO: CO_trace has not been ported to the current CANopenNode SDO server and
        * OD APIs. Keep CO_CONFIG_TRACE_ENABLE disabled until CO_trace.c/.h and this
        * initialization path are migrated together.
        */
        CO_LOCK_OD(co->CANmodule);
        for (uint16_t i = 0U; i < co->traceCount; i++) {
            CO_trace_process(&co->trace[i], time_current_ms);
        }
        CO_UNLOCK_OD(co->CANmodule);
#endif /* ((CO_CONFIG_TRACE) & CO_CONFIG_TRACE_ENABLE) != 0 */
#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0
        (void)co_storage_rtt_auto_process(&app->storage, co,
                                          (reset_status == CO_RESET_COMM) || (reset_status == CO_RESET_APP));
#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */

#if defined(CO_CONFIG_LEDS_ENABLE) && (((CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE) != 0)
        app->outStatusLEDRed = CO_LED_RED(co->LEDs, CO_LED_CANopen);
        app->outStatusLEDGreen = CO_LED_GREEN(co->LEDs, CO_LED_CANopen);
#else
        app->outStatusLEDRed = 0U;
        app->outStatusLEDGreen = 0U;
#endif /* defined(CO_CONFIG_LEDS_ENABLE) && (((CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE) != 0) */
        co_app_rtt_led_pin_update(app);

        if (reset_status == CO_RESET_COMM) {
            CO_RTT_LOG_W("communication reset requested: dev=%s", app->canName);

            if (app->rtTimer != RT_NULL) {
                (void)rt_timer_stop(app->rtTimer);
                co_app_rtt_reset_realtime_sem(app);
            }

            /*
             * Stop the timer and reset queued wakeups first, then take the
             * lifecycle mutex before deleting the old CO_t. CANnormal alone is
             * only a state gate; it does not protect a pointer already loaded by
             * the realtime thread.
             */
            ret = rt_mutex_take(&app->lifecycleMutex, RT_WAITING_FOREVER);
            if (ret != RT_EOK) {
                CO_RTT_LOG_E("take lifecycle mutex failed: dev=%s ret=%d", app->canName, ret);
                return;
            }

            ret = co_app_rtt_recreate_stack(app);
            (void)rt_mutex_release(&app->lifecycleMutex);
            if (ret != RT_EOK) {
                CO_RTT_LOG_E("communication reset failed: dev=%s ret=%d", app->canName, ret);
                return;
            }

            co = app->canOpenStack;
            app->timeOldMs = rt_tick_get_millisecond();
            app->lastRtTickMs = app->timeOldMs;

            if (app->rtTimer != RT_NULL) {
                ret = rt_timer_start(app->rtTimer);
                if (ret != RT_EOK) {
                    if ((app->canOpenStack != NULL) && (app->canOpenStack->CANmodule != NULL)) {
                        app->canOpenStack->CANmodule->CANnormal = false;
                    }
                    CO_RTT_LOG_E("restart realtime timer failed: dev=%s ret=%d", app->canName, ret);
                    return;
                }
            }
        } else if (reset_status == CO_RESET_APP) {
            CO_RTT_LOG_W("application reset requested: dev=%s", app->canName);
            rt_hw_cpu_reset();
        }
    }
}

/**
 * @brief Realtime CANopen worker thread entry.
 *
 * @param parameter CANopenNode RT-Thread application instance.
 */
static void co_app_rtt_realtime_thread_entry(void *parameter)
{
    CANopenNodeRTT *app = (CANopenNodeRTT *)parameter;
#if (((CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE) != 0)
    CO_SRDO_state_t lastSrdoState = CO_SRDO_state_unknown;
#endif /* (((CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE) != 0) */

    while (1) {
        CO_t *co;
        bool_t sync_was = false;
        uint32_t now_ms;
        uint32_t time_difference_us;

        if (rt_sem_take(&app->rtSem, RT_WAITING_FOREVER) != RT_EOK) {
            continue;
        }

        /*
         * CO_process_SYNC/SRDO/RPDO/TPDO receive the actual elapsed time between
         * realtime wakeups. PKG_CANOPENNODE_TIMER_PERIOD_US is only the requested
         * period before RT-Thread tick rounding.
         */
        now_ms = rt_tick_get_millisecond();
        time_difference_us = (now_ms - app->lastRtTickMs) * 1000U;
        if (time_difference_us == 0U) {
            time_difference_us = app->actualPeriodUs;
        }
        app->lastRtTickMs = now_ms;

        /*
         * The lifecycle mutex protects only the CO_t pointer lifetime across
         * communication reset. CO_LOCK_OD() below serializes PDO-mappable OD
         * variable access with SDO, storage and application-side OD access.
         */
        if (rt_mutex_take(&app->lifecycleMutex, RT_WAITING_FOREVER) != RT_EOK) {
            continue;
        }

        co = app->canOpenStack;
        if ((co == NULL) || (co->CANmodule == NULL)) {
            (void)rt_mutex_release(&app->lifecycleMutex);
            continue;
        }

        if (!co->nodeIdUnconfigured && co->CANmodule->CANnormal) {
            CO_LOCK_OD(co->CANmodule);
#if (((CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE) != 0)
            sync_was = CO_process_SYNC(co, time_difference_us, NULL);
#endif /* (((CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE) != 0) */

#if (((CO_CONFIG_PDO) & CO_CONFIG_RPDO_ENABLE) != 0)
            CO_process_RPDO(co, sync_was, time_difference_us, NULL);
#endif /* (((CO_CONFIG_PDO) & CO_CONFIG_RPDO_ENABLE) != 0) */

#if (((CO_CONFIG_PDO) & CO_CONFIG_TPDO_ENABLE) != 0)
            CO_process_TPDO(co, sync_was, time_difference_us, NULL);
#endif /* (((CO_CONFIG_PDO) & CO_CONFIG_TPDO_ENABLE) != 0) */

#if (((CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE) != 0)
            CO_SRDO_state_t srdoState = CO_process_SRDO(co, time_difference_us, NULL);
            if (srdoState != lastSrdoState) {
                if (srdoState < CO_SRDO_state_unknown) {
                    CO_RTT_LOG_E("SRDO process error: dev=%s state=%d", app->canName, (int)srdoState);
                } else {
                    CO_RTT_LOG_D("SRDO process state changed: dev=%s state=%d", app->canName, (int)srdoState);
                }
                lastSrdoState = srdoState;
            }
#endif /* (((CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE) != 0) */
            CO_UNLOCK_OD(co->CANmodule);
        }
        (void)rt_mutex_release(&app->lifecycleMutex);
    }
}

/**
 * @brief Periodic timer callback used to wake realtime CANopen processing.
 *
 * @param parameter CANopenNode RT-Thread application instance.
 */
static void co_app_rtt_timer_cb(void *parameter)
{
    CANopenNodeRTT *app = (CANopenNodeRTT *)parameter;

    (void)rt_sem_release(&app->rtSem);
}

/**
 * @brief Initialize and start a CANopenNode RT-Thread application instance.
 *
 * This function stores the mandatory CAN interface parameters into @p app, creates
 * the CANopenNode object, initializes CANopen communication, and starts the
 * internal mainline and realtime worker threads. The instance must be
 * zero-initialized before first use.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @param canName RT-Thread CAN device name. The pointer is stored, not copied, and must remain valid for the
 * lifetime of @p app.
 * @param nodeID CANopen node ID in range 1..127, or 0xFF when LSS slave is enabled to start
 * unconfigured.
 * @param bitrate CAN bitrate in kbit/s.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
rt_err_t canopen_app_rtt_init(CANopenNodeRTT *app, const char *canName, uint8_t nodeID, uint16_t bitrate)
{
    rt_bool_t sem_inited = RT_FALSE;
    rt_bool_t mutex_inited = RT_FALSE;
    rt_bool_t lifecycle_locked = RT_FALSE;
    rt_tick_t rt_period_ticks;
    rt_err_t ret = RT_EOK;

    if (app == NULL) {
        return -RT_EINVAL;
    }
    if ((app->mainThread != RT_NULL) || (app->rtThread != RT_NULL) || (app->rtTimer != RT_NULL)
        || (app->canOpenStack != NULL)) {
        return -RT_EBUSY;
    }
    if (canName == NULL) {
        CO_RTT_LOG_E("invalid CAN device name");
        return -RT_EINVAL;
    }
#if (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0)
    if ((nodeID == 0U) || ((nodeID > 127U) && (nodeID != CO_LSS_NODE_ID_ASSIGNMENT))) {
        CO_RTT_LOG_E("invalid CANopen node ID: %u", nodeID);
        return -RT_EINVAL;
    }
#else
    if ((nodeID == 0U) || (nodeID > 127U)) {
        CO_RTT_LOG_E("invalid CANopen node ID: %u", nodeID);
        return -RT_EINVAL;
    }
#endif /* (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0) */
    if (bitrate == 0U) {
        CO_RTT_LOG_E("invalid CANopen bitrate: %u", bitrate);
        return -RT_EINVAL;
    }

    app->canName = canName;
    app->desiredNodeID = nodeID;
    app->activeNodeID = 0U;
    app->baudrate = bitrate;
#if (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0)
    app->lssPendingNodeID = nodeID;
    app->lssPendingBitrate = bitrate;
#endif /* (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0) */
    app->outStatusLEDGreen = 0U;
    app->outStatusLEDRed = 0U;
    co_app_rtt_led_pin_init();
    app->timeOldMs = 0U;
    app->lastRtTickMs = 0U;
    app->actualPeriodUs = 0U;
    ret = rt_sem_init(&app->rtSem, "co_sem", 0U, RT_IPC_FLAG_FIFO);
    if (ret != RT_EOK) {
        goto cleanup;
    }
    sem_inited = RT_TRUE;

    /*
     * This mutex is intentionally narrow: it does not serialize mainline
     * CO_process() with realtime SRDO/PDO/SYNC work. It only protects the lifetime
     * of app->canOpenStack while reset deletes and recreates the stack object.
     */
    ret = rt_mutex_init(&app->lifecycleMutex, "co_life", RT_IPC_FLAG_PRIO);
    if (ret != RT_EOK) {
        goto cleanup;
    }
    mutex_inited = RT_TRUE;

    ret = co_app_rtt_new_stack(app);
    if (ret != RT_EOK) {
        goto cleanup;
    }
    ret = co_app_rtt_reset_communication(app);
    if (ret != RT_EOK) {
        goto cleanup;
    }

    app->mainThread = rt_thread_create("co_main", co_app_rtt_main_thread_entry, app,
                                       PKG_CANOPENNODE_MAIN_THREAD_STACK_SIZE,
                                       PKG_CANOPENNODE_MAIN_THREAD_PRIORITY,
                                       PKG_CANOPENNODE_MAIN_THREAD_TICK);
    if (app->mainThread == RT_NULL) {
        ret = -RT_ENOMEM;
        goto cleanup;
    }

    app->rtThread = rt_thread_create("co_rt", co_app_rtt_realtime_thread_entry, app,
                                     PKG_CANOPENNODE_RT_THREAD_STACK_SIZE,
                                     PKG_CANOPENNODE_RT_THREAD_PRIORITY,
                                     PKG_CANOPENNODE_RT_THREAD_TICK);
    if (app->rtThread == RT_NULL) {
        ret = -RT_ENOMEM;
        goto cleanup;
    }

    rt_period_ticks = co_app_rtt_timer_period_ticks(&app->actualPeriodUs);
    app->lastRtTickMs = rt_tick_get_millisecond();
    app->rtTimer = rt_timer_create("co_tmr", co_app_rtt_timer_cb, app, rt_period_ticks, RT_TIMER_FLAG_PERIODIC);
    if (app->rtTimer == RT_NULL) {
        ret = -RT_ENOMEM;
        goto cleanup;
    }

    ret = rt_thread_startup(app->rtThread);
    if (ret != RT_EOK) {
        goto cleanup;
    }
    ret = rt_timer_start(app->rtTimer);
    if (ret != RT_EOK) {
        goto cleanup;
    }
    /*
     * Start the mainline thread last. It can process CO_RESET_COMM and delete
     * the current CO_t, so all realtime synchronization objects must already be
     * fully constructed before it runs.
     */
    ret = rt_thread_startup(app->mainThread);
    if (ret != RT_EOK) {
        goto cleanup;
    }

    CO_RTT_LOG_I("CANopen RTT app initialized: dev=%s node=%u mainPrio=%u rtPrio=%u", app->canName, app->activeNodeID,
                 PKG_CANOPENNODE_MAIN_THREAD_PRIORITY, PKG_CANOPENNODE_RT_THREAD_PRIORITY);

    return RT_EOK;

cleanup:
    if (app->rtTimer != RT_NULL) {
        (void)rt_timer_stop(app->rtTimer);
    }
    if (sem_inited == RT_TRUE) {
        co_app_rtt_reset_realtime_sem(app);
    }
    if (mutex_inited == RT_TRUE) {
        /*
         * A late initialization failure can occur after the realtime thread and
         * timer have started. Take the same lifecycle mutex before deleting the
         * stack so cleanup follows the same lifetime rule as CO_RESET_COMM.
         */
        if (rt_mutex_take(&app->lifecycleMutex, RT_WAITING_FOREVER) == RT_EOK) {
            lifecycle_locked = RT_TRUE;
        }
    }
    if (app->rtTimer != RT_NULL) {
        (void)rt_timer_delete(app->rtTimer);
        app->rtTimer = RT_NULL;
    }
    if (app->rtThread != RT_NULL) {
        (void)rt_thread_delete(app->rtThread);
        app->rtThread = RT_NULL;
    }
    if (app->mainThread != RT_NULL) {
        (void)rt_thread_delete(app->mainThread);
        app->mainThread = RT_NULL;
    }
    if (app->canOpenStack != NULL) {
        CO_CANmodule_disable(app->canOpenStack->CANmodule);
        CO_delete(app->canOpenStack);
        app->canOpenStack = NULL;
    }
    if (lifecycle_locked == RT_TRUE) {
        (void)rt_mutex_release(&app->lifecycleMutex);
    }
    if (mutex_inited == RT_TRUE) {
        (void)rt_mutex_detach(&app->lifecycleMutex);
    }
    if (sem_inited == RT_TRUE) {
        (void)rt_sem_detach(&app->rtSem);
    }

    return ret;
}

#if defined(PKG_CANOPENNODE_APP_AUTO_INIT)
static CANopenNodeRTT co_app_rtt_default;

/**
 * @brief Automatically initialize the default CANopenNode RT-Thread application instance.
 *
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
static int co_app_rtt_auto_init(void)
{
    return (int)canopen_app_rtt_init(&co_app_rtt_default, PKG_CANOPENNODE_AUTO_INIT_CAN_DEV_NAME,
                                     PKG_CANOPENNODE_AUTO_INIT_NODE_ID, PKG_CANOPENNODE_AUTO_INIT_BITRATE);
}
INIT_APP_EXPORT(co_app_rtt_auto_init);
#endif /* defined(PKG_CANOPENNODE_APP_AUTO_INIT) */
