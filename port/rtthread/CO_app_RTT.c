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
#include "CO_time_RTT.h"
#include "co_rtt_log.h"
#include "OD.h"

#if defined(PKG_CANOPENNODE_GATEWAY_RTT_CONSOLE)
#include "CO_gateway_RTT.h"
#endif /* defined(PKG_CANOPENNODE_GATEWAY_RTT_CONSOLE) */

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0
#include "CO_storage_RTT.h"
#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */

#if defined(PKG_CANOPENNODE_LSS_PERSIST)
#include "CO_lss_persist_RTT.h"
#endif /* defined(PKG_CANOPENNODE_LSS_PERSIST) */

#include <rthw.h>
#if defined(PKG_CANOPENNODE_LEDS_USING_RTT_PIN)
#include <drivers/dev_pin.h>
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
 * CANopenNode objects that do not own dedicated OD entries, such as LEDs, LSS and
 * Gateway ASCII. This function keeps those counts aligned with the final
 * CO_CONFIG_* bitmasks.
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

#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII) != 0
    /* The generated demo OD keeps CNT_GTWA=0; derive this non-OD object count from the build configuration. */
    config->CNT_GTWA = 1U;
#endif /* ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII) != 0 */
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
#if defined(PKG_CANOPENNODE_LSS_PERSIST)
/** Check a millisecond deadline with uint32_t wraparound. */
static bool_t co_app_rtt_lss_time_reached(uint32_t nowMs, uint32_t deadlineMs)
{
    return (((int32_t)(nowMs - deadlineMs)) >= 0) ? true : false;
}

/** Reset only the application-side Activate Bit Timing state. */
static void co_app_rtt_lss_bitrate_reset_state(CANopenNodeRTT *app)
{
    app->lssBitrateState = CO_APP_RTT_LSS_BITRATE_IDLE;
    app->lssPreviousBitrate = 0U;
    app->lssTargetBitrate = 0U;
    app->lssBitrateDelayMs = 0U;
    app->lssBitrateDeadlineMs = 0U;
}

/** Finish a successful or recovered activation and allow new CANopen TX again. */
static void co_app_rtt_lss_bitrate_finish(CANopenNodeRTT *app)
{
    if ((app->canOpenStack != NULL) && (app->canOpenStack->CANmodule != NULL)) {
        CO_RTT_CANsetTxEnabled(app->canOpenStack->CANmodule, true);
    }
    co_app_rtt_lss_bitrate_reset_state(app);
}

/** Keep CANopen TX disabled after an unrecoverable runtime switch failure. */
static void co_app_rtt_lss_bitrate_fail(CANopenNodeRTT *app)
{
    app->lssBitrateState = CO_APP_RTT_LSS_BITRATE_FAILED;
    if ((app->canOpenStack != NULL) && (app->canOpenStack->CANmodule != NULL)) {
        CO_RTT_CANsetTxEnabled(app->canOpenStack->CANmodule, false);
        app->canOpenStack->CANmodule->CANnormal = false;
    }
    CO_RTT_LOG_E("LSS bitrate activation failed; reset/power-cycle required: dev=%s previous=%u target=%u",
                 app->canName, app->lssPreviousBitrate, app->lssTargetBitrate);
}

/** CANopenNode LSS Configure Bit Timing capability callback. */
static bool_t co_app_rtt_lss_check_bitrate(void *object, uint16_t bitrate)
{
    CANopenNodeRTT *app = (CANopenNodeRTT *)object;
    bool_t supported;

    if (app == NULL) {
        return false;
    }

    supported = CO_RTT_CANisBitrateSupported(bitrate);
    CO_RTT_LOG_D("LSS bitrate check: dev=%s bitrate=%u supported=%u",
                 app->canName, bitrate, supported ? 1U : 0U);
    return supported;
}

/**
 * @brief CANopenNode LSS Activate Bit Timing callback.
 *
 * The callback never sleeps or reconfigures hardware. It only closes the
 * CANopen-layer TX gate and starts PRE_DELAY. A frame that already entered the
 * RT-Thread/HAL TX path before this atomic gate is closed is intentionally not
 * aborted by the current implementation.
 */
static void co_app_rtt_lss_activate_bitrate(void *object, uint16_t delay)
{
    CANopenNodeRTT *app = (CANopenNodeRTT *)object;
    CO_CANmodule_t *CANmodule;
    uint32_t nowMs;

    if ((app == NULL) || (app->canOpenStack == NULL) || (app->canOpenStack->CANmodule == NULL)) {
        return;
    }
    if (app->lssBitrateState != CO_APP_RTT_LSS_BITRATE_IDLE) {
        CO_RTT_LOG_W("LSS bitrate activate ignored while busy: dev=%s state=%d",
                     app->canName, (int)app->lssBitrateState);
        return;
    }
    if (!CO_RTT_CANisBitrateSupported(app->lssPendingBitrate)) {
        CO_RTT_LOG_E("LSS bitrate activate rejected invalid pending bitrate: dev=%s bitrate=%u",
                     app->canName, app->lssPendingBitrate);
        return;
    }

    CANmodule = app->canOpenStack->CANmodule;
    app->lssPreviousBitrate = app->baudrate;
    app->lssTargetBitrate = app->lssPendingBitrate;
    app->lssBitrateDelayMs = delay;

    nowMs = rt_tick_get_millisecond();
    CO_RTT_CANsetTxEnabled(CANmodule, false);
    app->lssBitrateDeadlineMs = nowMs + delay;
    app->lssBitrateState = CO_APP_RTT_LSS_BITRATE_PRE_DELAY;

    CO_RTT_LOG_I("LSS bitrate PRE_DELAY started: dev=%s previous=%u target=%u delay=%u ms",
                 app->canName, app->lssPreviousBitrate, app->lssTargetBitrate, delay);
}

/** Advance PRE_DELAY -> bitrate switch -> POST_DELAY without blocking co_main. */
static void co_app_rtt_lss_bitrate_process(CANopenNodeRTT *app, uint32_t nowMs)
{
    CO_CANmodule_t *CANmodule;

    if ((app == NULL) || (app->lssBitrateState == CO_APP_RTT_LSS_BITRATE_IDLE)
        || (app->lssBitrateState == CO_APP_RTT_LSS_BITRATE_FAILED)) {
        return;
    }
    if ((app->canOpenStack == NULL) || (app->canOpenStack->CANmodule == NULL)) {
        co_app_rtt_lss_bitrate_fail(app);
        return;
    }

    CANmodule = app->canOpenStack->CANmodule;

    if ((app->lssBitrateState == CO_APP_RTT_LSS_BITRATE_PRE_DELAY)
        && co_app_rtt_lss_time_reached(nowMs, app->lssBitrateDeadlineMs)) {
        rt_err_t lockRet;
        rt_err_t switchRet;
        rt_err_t rollbackRet = -RT_ERROR;

        lockRet = rt_mutex_take(&app->lifecycleMutex, RT_WAITING_FOREVER);
        if (lockRet != RT_EOK) {
            CO_RTT_LOG_E("take lifecycle mutex for LSS bitrate switch failed: dev=%s ret=%d", app->canName, lockRet);
            co_app_rtt_lss_bitrate_fail(app);
            return;
        }

        switchRet = CO_RTT_CANsetBitrate(CANmodule, app->lssTargetBitrate);
        if (switchRet != RT_EOK) {
            CO_RTT_LOG_E("LSS bitrate switch failed: dev=%s target=%u ret=%ld; rolling back to %u",
                         app->canName, app->lssTargetBitrate, (long)switchRet, app->lssPreviousBitrate);
            rollbackRet = CO_RTT_CANsetBitrate(CANmodule, app->lssPreviousBitrate);
        }
        (void)rt_mutex_release(&app->lifecycleMutex);

        if (switchRet == RT_EOK) {
            app->baudrate = app->lssTargetBitrate;
            app->lssBitrateDeadlineMs = nowMs + app->lssBitrateDelayMs;
            app->lssBitrateState = CO_APP_RTT_LSS_BITRATE_POST_DELAY;
            CO_RTT_LOG_I("LSS bitrate switched: dev=%s bitrate=%u; POST_DELAY=%u ms",
                         app->canName, app->baudrate, app->lssBitrateDelayMs);
        } else if (rollbackRet == RT_EOK) {
            app->baudrate = app->lssPreviousBitrate;
            app->lssPendingBitrate = app->lssPreviousBitrate;
            CO_RTT_LOG_W("LSS bitrate rollback succeeded: dev=%s bitrate=%u", app->canName, app->baudrate);
            co_app_rtt_lss_bitrate_finish(app);
            return;
        } else {
            CO_RTT_LOG_E("LSS bitrate rollback failed: dev=%s bitrate=%u ret=%ld",
                         app->canName, app->lssPreviousBitrate, (long)rollbackRet);
            co_app_rtt_lss_bitrate_fail(app);
            return;
        }
    }

    if ((app->lssBitrateState == CO_APP_RTT_LSS_BITRATE_POST_DELAY)
        && co_app_rtt_lss_time_reached(nowMs, app->lssBitrateDeadlineMs)) {
        CO_RTT_LOG_I("LSS bitrate activation complete: dev=%s bitrate=%u", app->canName, app->baudrate);
        co_app_rtt_lss_bitrate_finish(app);
    }
}

/**
 * @brief Persist the pending LSS configuration requested by the CANopenNode LSS Store service.
 *
 * CANopenNode invokes this callback synchronously while processing LSS command 0x17.
 * Returning true is therefore reserved for a fully committed and verified single-slot record.
 *
 * @param object CANopenNodeRTT application instance.
 * @param nodeId Pending LSS Node-ID to store.
 * @param bitrate Pending LSS bitrate in kbit/s to store.
 * @return true when the committed record was read back and validated, otherwise false.
 */
static bool_t co_app_rtt_lss_store_config(void *object, uint8_t nodeId, uint16_t bitrate)
{
    CANopenNodeRTT *app = (CANopenNodeRTT *)object;
    bool_t stored;

    if (app == NULL) {
        return false;
    }

    stored = co_lss_persist_rtt_store(&app->storage, nodeId, bitrate);
    if (stored) {
        CO_RTT_LOG_I("LSS configuration stored: dev=%s node=%u bitrate=%u", app->canName, nodeId, bitrate);
    } else {
        CO_RTT_LOG_E("LSS configuration store failed: dev=%s node=%u bitrate=%u", app->canName, nodeId, bitrate);
    }

    return stored;
}
#endif /* defined(PKG_CANOPENNODE_LSS_PERSIST) */

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
#if defined(PKG_CANOPENNODE_LSS_PERSIST)
        co_app_rtt_lss_bitrate_reset_state(app);
        CO_RTT_CANsetTxEnabled(app->canOpenStack->CANmodule, true);
        CO_LSSslave_initCfgStoreCall(app->canOpenStack->LSSslave, app, co_app_rtt_lss_store_config);
        CO_LSSslave_initCkBitRateCall(app->canOpenStack->LSSslave, app, co_app_rtt_lss_check_bitrate);
        CO_LSSslave_initActBitRateCall(app->canOpenStack->LSSslave, app, co_app_rtt_lss_activate_bitrate);
#endif /* defined(PKG_CANOPENNODE_LSS_PERSIST) */
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
 * @param storageInitError Optional backend detail or CO_storageEeprom corruption bitmask.
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
#if CO_DEMO_ENABLED
    CO_demo_on_storage_init(&app->demo, &app->storage, app->storageEntries, entriesCount,
                            err, *storageInitError);
#endif /* CO_DEMO_ENABLED */
    if (err != CO_ERROR_NO) {
        CO_RTT_LOG_E("CO storage init failed: dev=%s err=%d detail=0x%08lx", app->canName, err,
                     (unsigned long)*storageInitError);
    }

    return err;
}

/**
 * @brief Prepare Storage and normalize recoverable startup corruption.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @param auxiliaryOnly True to prepare only backend auxiliary persistence without invoking normal OD Storage init.
 * @param dataCorrupt Set when persisted Storage data or the backend is unavailable.
 * @param storageInitError Receives the backend detail or corruption bitmask.
 * @param storageAvailable Set when the selected backend is initialized and can be accessed.
 * @return RT_EOK when Storage is usable or recoverably corrupt, otherwise a negative RT-Thread error code.
 */
static rt_err_t co_app_rtt_storage_prepare(CANopenNodeRTT *app, bool_t auxiliaryOnly, bool_t *dataCorrupt,
                                            uint32_t *storageInitError, bool_t *storageAvailable)
{
    CO_ReturnError_t err;

    if ((dataCorrupt == NULL) || (storageInitError == NULL) || (storageAvailable == NULL)) {
        return -RT_EINVAL;
    }

    *dataCorrupt = false;
    *storageInitError = 0U;
    *storageAvailable = false;

#if defined(PKG_CANOPENNODE_LSS_PERSIST)
    if (auxiliaryOnly) {
        err = co_storage_rtt_aux_init(&app->storage, app->canName, storageInitError);
    } else
#endif /* defined(PKG_CANOPENNODE_LSS_PERSIST) */
    {
        (void)auxiliaryOnly;
        err = co_app_rtt_storage_init(app, storageInitError);
    }
    if (err == CO_ERROR_NO) {
        *storageAvailable = true;
        return RT_EOK;
    }
    if (err != CO_ERROR_DATA_CORRUPT) {
        return -RT_ERROR;
    }

    *dataCorrupt = true;
    if (*storageInitError == UINT32_MAX) {
        CO_RTT_LOG_W("CO storage backend unavailable: dev=%s detail=0x%08lx", app->canName,
                     (unsigned long)*storageInitError);
    } else {
        *storageAvailable = true;
        CO_RTT_LOG_W("CO storage persistent data invalid: dev=%s subIndexMask=0x%08lx", app->canName,
                     (unsigned long)*storageInitError);
    }

    return RT_EOK;
}
#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */

/**
 * @brief Reset CANopen communication for an application instance.
 *
 * @param app CANopenNode RT-Thread application instance.
 * @param loadPersistentLss True only for initial application startup, when saved LSS values may replace startup values.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
static rt_err_t co_app_rtt_reset_communication(CANopenNodeRTT *app, bool_t loadPersistentLss)
{
    CO_t *co = app->canOpenStack;
    CO_ReturnError_t err;
#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
    rt_err_t ret;
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */
    uint32_t err_info = 0U;
#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0
    bool_t storageDataCorrupt = false;
    bool_t storageAvailable = false;
    uint32_t storageInitError = 0U;
#if defined(PKG_CANOPENNODE_LSS_PERSIST)
    bool_t persistentLssLoaded = false;
    const uint8_t startupNodeId = app->lssPendingNodeID;
    const uint16_t startupBitrate = app->lssPendingBitrate;
#endif /* defined(PKG_CANOPENNODE_LSS_PERSIST) */
#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */
#if (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0)
    uint8_t can_node_id;
    uint16_t can_bitrate;
#else
    const uint8_t can_node_id = app->desiredNodeID;
    const uint16_t can_bitrate = app->baudrate;
#endif /* (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0) */

#if !defined(PKG_CANOPENNODE_LSS_PERSIST)
    (void)loadPersistentLss;
#endif /* !defined(PKG_CANOPENNODE_LSS_PERSIST) */

    co->CANmodule->CANnormal = false;
    CO_CANsetConfigurationMode((void *)app->canName);
    CO_CANmodule_disable(co->CANmodule);

#if defined(PKG_CANOPENNODE_LSS_PERSIST)
    if (loadPersistentLss) {
        /* The persisted bitrate must be available before the first CO_CANinit(). */
        if (co_app_rtt_storage_prepare(app, true, &storageDataCorrupt, &storageInitError, &storageAvailable) != RT_EOK) {
            return -RT_ERROR;
        }
        if (storageAvailable) {
            CO_lss_persist_load_result_t persistResult = co_lss_persist_rtt_load(
                &app->storage, &app->lssPendingNodeID, &app->lssPendingBitrate);

            if (persistResult == CO_LSS_PERSIST_LOAD_OK) {
                persistentLssLoaded = true;
                CO_RTT_LOG_I("LSS persistent configuration loaded: dev=%s node=%u bitrate=%u", app->canName,
                             app->lssPendingNodeID, app->lssPendingBitrate);
            } else if (persistResult == CO_LSS_PERSIST_LOAD_EMPTY) {
                CO_RTT_LOG_I("LSS persistent configuration is empty: dev=%s, using startup values", app->canName);
            } else if (persistResult == CO_LSS_PERSIST_LOAD_CONFIG_ERROR) {
                CO_RTT_LOG_E("LSS persistent storage unavailable: dev=%s, using startup values", app->canName);
            } else {
                CO_RTT_LOG_W("LSS persistent configuration ignored: dev=%s result=%d, using startup values",
                             app->canName, (int)persistResult);
            }
        }
    }
#endif /* defined(PKG_CANOPENNODE_LSS_PERSIST) */

#if (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0)
    can_node_id = app->lssPendingNodeID;
    can_bitrate = app->lssPendingBitrate;
#endif /* (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0) */

    CO_RTT_LOG_W("CANopen reset communication: dev=%s node=%u bitrate=%u", app->canName, can_node_id, can_bitrate);

    err = CO_CANinit(co, (void *)app->canName, can_bitrate);
#if defined(PKG_CANOPENNODE_LSS_PERSIST)
    if ((err != CO_ERROR_NO) && persistentLssLoaded && (can_bitrate != startupBitrate)) {
        CO_RTT_LOG_W("CAN init rejected persistent LSS configuration: dev=%s node=%u bitrate=%u err=%d; "
                     "retrying startup values", app->canName, can_node_id, can_bitrate, err);
        app->lssPendingNodeID = startupNodeId;
        app->lssPendingBitrate = startupBitrate;
        can_node_id = startupNodeId;
        can_bitrate = startupBitrate;
        err = CO_CANinit(co, (void *)app->canName, can_bitrate);
    }
#endif /* defined(PKG_CANOPENNODE_LSS_PERSIST) */
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
    /* Normal OD-backed Storage initialization is intentionally separate from pre-CAN auxiliary preparation. */
    if (co_app_rtt_storage_prepare(app, false, &storageDataCorrupt, &storageInitError, &storageAvailable) != RT_EOK) {
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
#if defined(PKG_CANOPENNODE_GATEWAY_RTT_CONSOLE)
    CO_gateway_RTT_rebind(app);
#endif /* defined(PKG_CANOPENNODE_GATEWAY_RTT_CONSOLE) */

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

    /* Bind optional demo/test callbacks after all communication objects are
     * initialized, but before normal mode allows receive callbacks to run. */
#if CO_DEMO_ENABLED
    if (!CO_demo_bind(&app->demo, co)) {
        CO_RTT_LOG_E("bind demo callbacks failed: dev=%s", app->canName);
        return -RT_EBUSY;
    }
#endif /* CO_DEMO_ENABLED */

#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
    ret = CO_RTT_mainlineBindCallbacks(app, co);
    if (ret != RT_EOK) {
        CO_RTT_LOG_E("bind mainline wake callbacks failed: dev=%s ret=%d", app->canName, ret);
        return ret;
    }
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */

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
#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
        CO_RTT_mainlineResetWakeups(app);
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */
        CO_delete(app->canOpenStack);
        app->canOpenStack = NULL;
    }

    ret = co_app_rtt_new_stack(app);
    if (ret != RT_EOK) {
        return ret;
    }

    ret = co_app_rtt_reset_communication(app, false);
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
    uint32_t time_current_us;
    uint32_t time_old_us = CO_RTT_timeNowUs();
#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
    uint32_t deadline_base_us = time_old_us;
    uint32_t timer_next_us = 0U;
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */

    if (co == NULL) {
        CO_RTT_LOG_E("mainline thread started without CANopen stack: dev=%s", app->canName);
        return;
    }

    while (1) {
        CO_NMT_reset_cmd_t reset_status;
        uint32_t time_difference_us;
        rt_err_t ret;

#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
        CO_RTT_mainlineWait(app, deadline_base_us, timer_next_us);
#else
        rt_thread_mdelay(1);
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */

        time_current_ms = rt_tick_get_millisecond();
        time_current_us = CO_RTT_timeNowUs();
        time_difference_us = CO_RTT_timeElapsedUs(time_current_us, time_old_us);
        time_old_us = time_current_us;
#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
        deadline_base_us = time_current_us;
#else
        if (time_difference_us == 0U) {
            continue;
        }
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */
        app->timeOldMs = time_current_ms;

#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
        timer_next_us = UINT32_MAX;
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */
        reset_status = CO_process(co, ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII) != 0,
                                  time_difference_us,
#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
                                  &timer_next_us
#else
                                  NULL
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */
        );
#if (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0) && defined(PKG_CANOPENNODE_LSS_PERSIST)
        if (reset_status == CO_RESET_NOT) {
            co_app_rtt_lss_bitrate_process(app, time_current_ms);
        }
#endif /* LSS runtime bitrate */
#if CO_DEMO_ENABLED
        CO_demo_process(&app->demo, co, app->activeNodeID, time_current_ms,
                        time_difference_us, reset_status,
#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
                        &timer_next_us
#else
                        NULL
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */
        );
#endif /* CO_DEMO_ENABLED */
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

#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
        CO_RTT_mainlineUpdateDeadline(app, co, time_current_ms, reset_status, &timer_next_us);
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */

#if defined(CO_CONFIG_LEDS_ENABLE) && (((CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE) != 0)
        app->outStatusLEDRed = CO_LED_RED(co->LEDs, CO_LED_CANopen);
        app->outStatusLEDGreen = CO_LED_GREEN(co->LEDs, CO_LED_CANopen);
#else
        app->outStatusLEDRed = 0U;
        app->outStatusLEDGreen = 0U;
#endif /* defined(CO_CONFIG_LEDS_ENABLE) && (((CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE) != 0) */
        co_app_rtt_led_pin_update(app);

        if (reset_status == CO_RESET_COMM) {
#if CO_DEMO_ENABLED
            CO_demo_reset(&app->demo);
#endif /* CO_DEMO_ENABLED */
            CO_RTT_LOG_W("communication reset requested: dev=%s", app->canName);

            if (app->rtTimer != RT_NULL) {
                (void)rt_timer_stop(app->rtTimer);
                co_app_rtt_reset_realtime_sem(app);
            }

            /*
             * Stop the realtime timer first, then take the lifecycle mutex before
             * deleting the old CO_t. The recreate path disables old CAN RX before
             * deletion; event-driven builds also clear stale wake state before
             * binding callbacks on the new stack.
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
            time_old_us = CO_RTT_timeNowUs();
#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
            deadline_base_us = time_old_us;
            timer_next_us = 0U;
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */

            if (app->rtTimer != RT_NULL) {
                /*
                 * The timer is stopped and queued wakeups were drained before stack recreation.
                 * Refresh the baseline immediately before restart so the first token charges only
                 * time elapsed after realtime scheduling resumes.
                 */
                app->lastRtUs = CO_RTT_timeNowUs();
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
#if CO_DEMO_ENABLED
            CO_demo_reset(&app->demo);
#endif /* CO_DEMO_ENABLED */
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
        uint32_t now_us;
        uint32_t time_difference_us;

        if (rt_sem_take(&app->rtSem, RT_WAITING_FOREVER) != RT_EOK) {
            continue;
        }

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

        /*
         * rtSem is counting, so queued wakeups can be drained back-to-back with
         * zero measured elapsed time. Keep that zero value instead of substituting
         * actualPeriodUs; otherwise protocol timers would advance without real time.
         */
        now_us = CO_RTT_timeNowUs();
        time_difference_us = CO_RTT_timeElapsedUs(now_us, app->lastRtUs);
        app->lastRtUs = now_us;
        app->lastRtTickMs = rt_tick_get_millisecond();

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
 * This function stores the mandatory CAN interface parameters into @p app, acquires
 * the wrapper microsecond time source, creates the CANopenNode object, initializes CANopen
 * communication, and starts the internal mainline and realtime worker threads.
 * The optional timerNext scheduler uses a coalescing per-instance event for
 * asynchronous mainline wakeups; the existing periodic rtTimer/rtSem realtime
 * worker remains unchanged. High-resolution time uses one package-wide dedicated
 * timer and therefore
 * supports only one CANopenNodeRTT instance; the BSP/user is responsible for
 * selecting a physically 32-bit timer because generic RT-Thread timer metadata
 * cannot reliably report counter width on all supported BSPs. High-resolution
 * instance initialization and teardown are lifecycle operations and must be
 * serialized by the caller; concurrent init/deinit is not supported. The
 * instance must be zero-initialized before first use.
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
#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
    rt_bool_t mainline_inited = RT_FALSE;
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */
    rt_bool_t mutex_inited = RT_FALSE;
    rt_bool_t lifecycle_locked = RT_FALSE;
    rt_bool_t time_inited = RT_FALSE;
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

    ret = CO_RTT_timeInit();
    if (ret != RT_EOK) {
        CO_RTT_LOG_E("initialize CANopen time source failed: ret=%d", ret);
        return ret;
    }
    time_inited = RT_TRUE;

    app->canName = canName;
    app->desiredNodeID = nodeID;
    app->activeNodeID = 0U;
    app->baudrate = bitrate;
#if (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0)
    app->lssPendingNodeID = nodeID;
    app->lssPendingBitrate = bitrate;
#if defined(PKG_CANOPENNODE_LSS_PERSIST)
    co_app_rtt_lss_bitrate_reset_state(app);
#endif /* defined(PKG_CANOPENNODE_LSS_PERSIST) */
#endif /* (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0) */
    app->outStatusLEDGreen = 0U;
    app->outStatusLEDRed = 0U;
#if CO_DEMO_ENABLED
    CO_demo_init(&app->demo);
#endif /* CO_DEMO_ENABLED */
    co_app_rtt_led_pin_init();
    app->timeOldMs = 0U;
    app->lastRtTickMs = 0U;
    app->actualPeriodUs = 0U;
    app->lastRtUs = 0U;
    ret = rt_sem_init(&app->rtSem, "co_sem", 0U, RT_IPC_FLAG_FIFO);
    if (ret != RT_EOK) {
        goto cleanup;
    }
    sem_inited = RT_TRUE;

#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
    ret = CO_RTT_mainlineInit(app);
    if (ret != RT_EOK) {
        goto cleanup;
    }
    mainline_inited = RT_TRUE;
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */

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
    ret = co_app_rtt_reset_communication(app, true);
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
    /*
     * The realtime thread is waiting on an empty semaphore and the timer is not running yet.
     * Establish the baseline immediately before the first wakeup can be queued so initialization
     * latency is not charged to protocol processing.
     */
    app->lastRtUs = CO_RTT_timeNowUs();
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
#if CO_DEMO_ENABLED
    /* CAN RX is quiesced before releasing demo callback ownership. */
    CO_demo_deinit(&app->demo);
#endif /* CO_DEMO_ENABLED */
#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
    if (mainline_inited == RT_TRUE) {
        CO_RTT_mainlineDeinit(app);
    }
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */
    if (lifecycle_locked == RT_TRUE) {
        (void)rt_mutex_release(&app->lifecycleMutex);
    }
    if (mutex_inited == RT_TRUE) {
        (void)rt_mutex_detach(&app->lifecycleMutex);
    }
    if (sem_inited == RT_TRUE) {
        (void)rt_sem_detach(&app->rtSem);
    }
    if (time_inited == RT_TRUE) {
        CO_RTT_timeDeinit();
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
    rt_err_t ret = canopen_app_rtt_init(&co_app_rtt_default, PKG_CANOPENNODE_CAN_DEV_NAME,
                                        PKG_CANOPENNODE_AUTO_INIT_NODE_ID, PKG_CANOPENNODE_AUTO_INIT_BITRATE);

#if defined(PKG_CANOPENNODE_GATEWAY_RTT_CONSOLE)
    if (ret == RT_EOK) {
        ret = CO_gateway_RTT_init(&co_app_rtt_default);
    }
#endif /* defined(PKG_CANOPENNODE_GATEWAY_RTT_CONSOLE) */

    return (int)ret;
}
INIT_APP_EXPORT(co_app_rtt_auto_init);
#endif /* defined(PKG_CANOPENNODE_APP_AUTO_INIT) */