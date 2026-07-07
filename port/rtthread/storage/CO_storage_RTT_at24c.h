/**
 * @file CO_storage_RTT_at24c.h
 * @brief AT24CXX device adapter for the RT-Thread EEPROM storage backend.
 * @details This header declares compile-time requirements for the AT24CXX-backed EEPROM storage adapter.
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
#ifndef CO_STORAGE_RTT_AT24C_H_
#define CO_STORAGE_RTT_AT24C_H_

/* Includes ------------------------------------------------------------------*/

#include "CO_storage_RTT_backend.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0
#if defined(PKG_CANOPENNODE_USING_STORAGE_EEPROM) && defined(PKG_CANOPENNODE_USING_STORAGE_AT24C)

#include <at24cxx.h>

#ifndef CO_CONFIG_STORAGE_MAX_ENTRIES_COUNT
#define CO_CONFIG_STORAGE_MAX_ENTRIES_COUNT 1U
#endif /* CO_CONFIG_STORAGE_MAX_ENTRIES_COUNT */

#ifndef PKG_CANOPENNODE_STORAGE_AT24C_OFFSET
#define PKG_CANOPENNODE_STORAGE_AT24C_OFFSET 0
#endif /* PKG_CANOPENNODE_STORAGE_AT24C_OFFSET */

#ifndef PKG_CANOPENNODE_STORAGE_AT24C_CRC_BUF_SIZE
#define PKG_CANOPENNODE_STORAGE_AT24C_CRC_BUF_SIZE 32
#endif /* PKG_CANOPENNODE_STORAGE_AT24C_CRC_BUF_SIZE */

#ifndef AT24CXX_MAX_MEM_ADDRESS
#error "AT24CXX_MAX_MEM_ADDRESS must be provided by at24cxx.h"
#endif /* AT24CXX_MAX_MEM_ADDRESS */

#ifndef AT24CXX_PAGE_BYTE
#error "AT24CXX_PAGE_BYTE must be provided by at24cxx.h"
#endif /* AT24CXX_PAGE_BYTE */

#if (PKG_CANOPENNODE_STORAGE_AT24C_CRC_BUF_SIZE <= 0)
#error "PKG_CANOPENNODE_STORAGE_AT24C_CRC_BUF_SIZE must be greater than 0"
#endif /* (PKG_CANOPENNODE_STORAGE_AT24C_CRC_BUF_SIZE <= 0) */

#if (PKG_CANOPENNODE_STORAGE_AT24C_OFFSET < 0)
#error "PKG_CANOPENNODE_STORAGE_AT24C_OFFSET must not be negative"
#endif /* (PKG_CANOPENNODE_STORAGE_AT24C_OFFSET < 0) */

#endif /* defined(PKG_CANOPENNODE_USING_STORAGE_EEPROM) && defined(PKG_CANOPENNODE_USING_STORAGE_AT24C) */
#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */

/* Exported types ------------------------------------------------------------*/

/* Exported variables ---------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_STORAGE_RTT_AT24C_H_ */
