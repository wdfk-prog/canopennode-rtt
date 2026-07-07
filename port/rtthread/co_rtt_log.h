/**
 * @file co_rtt_log.h
 * @brief RT-Thread ulog helpers for the CANopenNode RT-Thread port.
 * @details This header maps CANopenNode RT-Thread port log helpers to RT-Thread ulog when debug logging is enabled.
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
#ifndef CO_RTT_LOG_H_
#define CO_RTT_LOG_H_

/* Includes ------------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

#if defined(PKG_CANOPENNODE_USING_DEBUG) && defined(RT_USING_ULOG)

#ifndef LOG_TAG
#define LOG_TAG                         "canopen.rtt"
#endif /* LOG_TAG */

#ifndef LOG_LVL
#define LOG_LVL                         LOG_LVL_DBG
#endif /* LOG_LVL */

#include <ulog.h>

#define CO_RTT_LOG_E(...)               LOG_E(__VA_ARGS__)
#define CO_RTT_LOG_W(...)               LOG_W(__VA_ARGS__)
#define CO_RTT_LOG_I(...)               LOG_I(__VA_ARGS__)
#define CO_RTT_LOG_D(...)               LOG_D(__VA_ARGS__)
#define CO_RTT_LOG_HEX(name, width, buf, size) LOG_HEX(name, width, buf, size)

#else

#define CO_RTT_LOG_E(...)               do { } while (0)
#define CO_RTT_LOG_W(...)               do { } while (0)
#define CO_RTT_LOG_I(...)               do { } while (0)
#define CO_RTT_LOG_D(...)               do { } while (0)
#define CO_RTT_LOG_HEX(name, width, buf, size) do { (void)(name); (void)(width); (void)(buf); (void)(size); } while (0)

#endif /* defined(PKG_CANOPENNODE_USING_DEBUG) && defined(RT_USING_ULOG) */

/* Exported types ------------------------------------------------------------*/

/* Exported variables ---------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/

#endif /* CO_RTT_LOG_H_ */
