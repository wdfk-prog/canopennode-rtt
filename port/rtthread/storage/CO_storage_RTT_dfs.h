/**
 * @file CO_storage_RTT_dfs.h
 * @brief RT-Thread DFS storage backend metadata.
 * @details This header declares private metadata used by the RT-Thread DFS storage backend.
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
#ifndef CO_STORAGE_RTT_DFS_H_
#define CO_STORAGE_RTT_DFS_H_

/* Includes ------------------------------------------------------------------*/

#include <stdint.h>

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/

/**
 * @brief Per-entry DFS storage metadata.
 */
typedef struct {
    const char *directory;            /**< Directory without a trailing slash, or NULL to use the Kconfig default. */
    const char *filePrefix;           /**< Optional filename prefix used to separate CANopenNodeRTT instances. */
    uint16_t autoCrc;                 /**< Last CRC stored by automatic DFS processing. */
    uint8_t autoCrcValid;             /**< Non-zero if autoCrc describes the last persisted automatic payload. */
    uint8_t dataCorrupt;              /**< Non-zero if startup read rejected the persisted file for this entry. */
} CO_storage_rtt_dfs_entry_t;

/* Exported variables ---------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/

#endif /* CO_STORAGE_RTT_DFS_H_ */
