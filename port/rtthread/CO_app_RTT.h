/**
 * @file CO_app_RTT.h
 * @brief CANopenNode RT-Thread application runtime wrapper.
 * @details This header declares the CANopenNode RT-Thread application instance and initialization API.
 * @author wdfk-prog ()
 * @version 1.0.0
 * @date 2026.07.04
 *
 * @copyright Copyright (c) 2026
 *
 * @note :
 * @par 修改日志:
 * Date       Version Author      Description
 * 2026.07.04 1.0.0   wdfk-prog   first version
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef CO_APP_RTT_H_
#define CO_APP_RTT_H_

/* Includes ------------------------------------------------------------------*/

#include <rtthread.h>

#ifndef CO_DRIVER_CUSTOM
#define CO_DRIVER_CUSTOM
#endif /* CO_DRIVER_CUSTOM */

#include "CANopen.h"
#include "CO_demo.h"

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0
#include "CO_storage_RTT.h"
#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0
/** Storage sub-index used for OD_PERSIST_COMM in OD 0x1010 and 0x1011. */
#define CO_APP_RTT_STORAGE_SUB_INDEX_COMM 2U

/** Storage sub-index used for OD_PERSIST_APP in OD 0x1010 and 0x1011. */
#define CO_APP_RTT_STORAGE_SUB_INDEX_APP  3U

/** Storage sub-index used for OD_PERSIST_MANU in OD 0x1010 and 0x1011. */
#define CO_APP_RTT_STORAGE_SUB_INDEX_MANU 4U

#if defined(PKG_CANOPENNODE_STORAGE_PERSIST_COMM)
/** Number of default storage entries contributed by OD_PERSIST_COMM. */
#define CO_APP_RTT_STORAGE_ENTRY_COUNT_COMM 1U
#else
#define CO_APP_RTT_STORAGE_ENTRY_COUNT_COMM 0U
#endif /* defined(PKG_CANOPENNODE_STORAGE_PERSIST_COMM) */

#if defined(PKG_CANOPENNODE_STORAGE_PERSIST_APP)
/** Number of default storage entries contributed by OD_PERSIST_APP. */
#define CO_APP_RTT_STORAGE_ENTRY_COUNT_APP 1U
#else
#define CO_APP_RTT_STORAGE_ENTRY_COUNT_APP 0U
#endif /* defined(PKG_CANOPENNODE_STORAGE_PERSIST_APP) */

#if defined(PKG_CANOPENNODE_STORAGE_PERSIST_MANU)
/** Number of default storage entries contributed by OD_PERSIST_MANU. */
#define CO_APP_RTT_STORAGE_ENTRY_COUNT_MANU 1U
#else
#define CO_APP_RTT_STORAGE_ENTRY_COUNT_MANU 0U
#endif /* defined(PKG_CANOPENNODE_STORAGE_PERSIST_MANU) */

/** Number of normal OD-backed storage entries owned by each CANopenNodeRTT instance. */
#define CO_APP_RTT_STORAGE_ENTRY_COUNT      (CO_APP_RTT_STORAGE_ENTRY_COUNT_COMM \
                                           + CO_APP_RTT_STORAGE_ENTRY_COUNT_APP \
                                           + CO_APP_RTT_STORAGE_ENTRY_COUNT_MANU)

#if (CO_APP_RTT_STORAGE_ENTRY_COUNT == 0U) && !defined(PKG_CANOPENNODE_LSS_PERSIST)
#error "Enable at least one PKG_CANOPENNODE_STORAGE_PERSIST_* option or PKG_CANOPENNODE_LSS_PERSIST."
#endif /* (CO_APP_RTT_STORAGE_ENTRY_COUNT == 0U) && !defined(PKG_CANOPENNODE_LSS_PERSIST) */
#if CO_APP_RTT_STORAGE_ENTRY_COUNT > CO_CONFIG_STORAGE_MAX_ENTRIES_COUNT
#error "Increase PKG_CANOPENNODE_STORAGE_MAX_ENTRIES_COUNT for the enabled OD_PERSIST_* storage groups."
#endif /* CO_APP_RTT_STORAGE_ENTRY_COUNT > CO_CONFIG_STORAGE_MAX_ENTRIES_COUNT */

/** Keep the C array valid when Storage is used only for backend auxiliary persistence. */
#define CO_APP_RTT_STORAGE_ENTRY_CAPACITY   ((CO_APP_RTT_STORAGE_ENTRY_COUNT > 0U) \
                                           ? CO_APP_RTT_STORAGE_ENTRY_COUNT : 1U)
#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */

/* Exported types ------------------------------------------------------------*/

#if (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0) && defined(PKG_CANOPENNODE_LSS_PERSIST)
/**
 * @brief Non-blocking runtime state for CiA 305 Activate Bit Timing.
 *
 * The LSS callback enters PRE_DELAY immediately after disabling new CANopen
 * transmissions. The mainline worker performs the bitrate switch when the first
 * delay expires and re-enables transmission only after POST_DELAY.
 */
typedef enum {
    CO_APP_RTT_LSS_BITRATE_IDLE = 0,
    CO_APP_RTT_LSS_BITRATE_PRE_DELAY,
    CO_APP_RTT_LSS_BITRATE_POST_DELAY,
    CO_APP_RTT_LSS_BITRATE_FAILED
} CO_app_rtt_lss_bitrate_state_t;
#endif /* LSS runtime bitrate */

/**
 * @brief CANopenNode RT-Thread application instance.
 */
typedef struct {
    const char *canName;             /**< RT-Thread CAN device name pointer stored by reference for the instance lifetime. */
    uint8_t desiredNodeID;           /**< Requested CANopen node ID. */
    uint8_t activeNodeID;            /**< Active CANopen node ID after communication initialization. */
    uint16_t baudrate;               /**< CAN bitrate in kbit/s. */

#if (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0)
    uint8_t lssPendingNodeID;        /**< Persistent pending Node-ID storage passed to CO_LSSinit(). */
    uint16_t lssPendingBitrate;      /**< Persistent pending bitrate storage passed to CO_LSSinit(). */
#if defined(PKG_CANOPENNODE_LSS_PERSIST)
    CO_app_rtt_lss_bitrate_state_t lssBitrateState; /**< Runtime Activate Bit Timing state. */
    uint16_t lssPreviousBitrate;     /**< Active bitrate before the current activation request. */
    uint16_t lssTargetBitrate;       /**< Pending bitrate selected by Configure Bit Timing. */
    uint16_t lssBitrateDelayMs;      /**< CiA 305 delay applied before and after switching. */
    uint32_t lssBitrateDeadlineMs;   /**< Current PRE/POST delay deadline in milliseconds. */
#endif /* defined(PKG_CANOPENNODE_LSS_PERSIST) */
#endif /* (((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE) != 0) */

    uint8_t outStatusLEDGreen;       /**< CANopen green LED status output. */
    uint8_t outStatusLEDRed;         /**< CANopen red LED status output. */

    CO_t *canOpenStack;              /**< CANopenNode object owned by this instance. */
#if CO_DEMO_ENABLED
    CO_demo_t demo;                  /**< Enabled demo/test module state. */
#endif /* CO_DEMO_ENABLED */

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0
    CO_storage_t storage;            /**< CANopenNode storage object owned by this instance. */
    CO_storage_entry_t storageEntries[CO_APP_RTT_STORAGE_ENTRY_CAPACITY]; /**< Storage entries owned by this instance. */
#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */

    rt_thread_t mainThread;          /**< Mainline CANopen worker thread. */
    rt_thread_t rtThread;            /**< Realtime CANopen worker thread. */
    rt_timer_t rtTimer;              /**< Periodic timer for realtime processing. */
    struct rt_semaphore rtSem;       /**< Realtime thread wake-up semaphore. */
    struct rt_mutex lifecycleMutex;  /**< Protects canOpenStack deletion/recreation against realtime use. */

    uint32_t timeOldMs;              /**< Previous mainline process timestamp in milliseconds. */
    uint32_t lastRtTickMs;           /**< Previous realtime process timestamp in milliseconds. */
    uint32_t actualPeriodUs;         /**< Actual realtime timer period after RT-Thread tick rounding, in microseconds. */

#ifdef CO_MULTIPLE_OD
    CO_config_t coConfig;            /**< Persistent CANopenNode configuration for CO_MULTIPLE_OD builds. */
#endif /* CO_MULTIPLE_OD */
} CANopenNodeRTT;

/* Exported variables ---------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/

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
rt_err_t canopen_app_rtt_init(CANopenNodeRTT *app, const char *canName, uint8_t nodeID, uint16_t bitrate);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_APP_RTT_H_ */