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
#error "PKG_CANOPENNODE_STORAGE_AT24C_OFFSET must be configured explicitly"
#endif /* PKG_CANOPENNODE_STORAGE_AT24C_OFFSET */

#ifndef PKG_CANOPENNODE_STORAGE_AT24C_REGION_SIZE
#error "PKG_CANOPENNODE_STORAGE_AT24C_REGION_SIZE must be configured explicitly"
#endif /* PKG_CANOPENNODE_STORAGE_AT24C_REGION_SIZE */

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

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0
#if defined(PKG_CANOPENNODE_USING_STORAGE_EEPROM) && defined(PKG_CANOPENNODE_USING_STORAGE_AT24C)
/**
 * @brief Read AT24CXX geometry exposed for raw Storage access.
 *
 * @param storageModule AT24CXX backend instance owned by CANopenNode storage.
 * @param storageOffset Configured first EEPROM address reserved for CANopenNode storage.
 * @param storageRegionSize Effective number of EEPROM bytes reserved for CANopenNode storage.
 * @param eepromSize EEPROM capacity in bytes.
 * @param pageSize EEPROM page size in bytes.
 * @param addrInput AT24CXX AddrInput value passed to at24cxx_init().
 * @return true when the backend is initialized and all outputs are valid, otherwise false.
 */
bool_t co_storage_rtt_at24c_raw_get_info(void *storageModule, size_t *storageOffset,
                                          size_t *storageRegionSize, size_t *eepromSize,
                                          size_t *pageSize, uint8_t *addrInput);

/**
 * @brief Read raw EEPROM bytes through the AT24CXX backend access API.
 *
 * @param storageModule AT24CXX backend instance owned by CANopenNode storage.
 * @param eepromAddr EEPROM start address.
 * @param data Destination buffer.
 * @param len Number of bytes to read.
 * @return true when all requested bytes are read, otherwise false.
 */
bool_t co_storage_rtt_at24c_raw_read(void *storageModule, size_t eepromAddr, uint8_t *data, size_t len);

/**
 * @brief Write raw EEPROM bytes through the AT24CXX backend access API.
 *
 * @param storageModule AT24CXX backend instance owned by CANopenNode storage.
 * @param eepromAddr EEPROM start address.
 * @param data Source buffer.
 * @param len Number of bytes to write.
 * @return true when all requested bytes are written, otherwise false.
 */
bool_t co_storage_rtt_at24c_raw_write(void *storageModule, size_t eepromAddr, uint8_t *data, size_t len);

#if defined(PKG_CANOPENNODE_LSS_PERSIST)
/**
 * @brief Read bytes from the AT24CXX auxiliary persistence area.
 *
 * @param storage Storage object that owns the AT24CXX backend instance.
 * @param offset Start offset relative to the reserved auxiliary area.
 * @param data Destination buffer.
 * @param len Number of bytes to read.
 * @return true when all requested bytes are read, otherwise false.
 */
bool_t co_storage_rtt_at24c_aux_read(CO_storage_t *storage, size_t offset, uint8_t *data, size_t len);

/**
 * @brief Write bytes to the AT24CXX auxiliary persistence area.
 *
 * @param storage Storage object that owns the AT24CXX backend instance.
 * @param offset Start offset relative to the reserved auxiliary area.
 * @param data Source buffer.
 * @param len Number of bytes to write.
 * @return true when all requested bytes are written, otherwise false.
 */
bool_t co_storage_rtt_at24c_aux_write(CO_storage_t *storage, size_t offset, const uint8_t *data, size_t len);
#endif /* defined(PKG_CANOPENNODE_LSS_PERSIST) */
#endif /* defined(PKG_CANOPENNODE_USING_STORAGE_EEPROM) && defined(PKG_CANOPENNODE_USING_STORAGE_AT24C) */
#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_STORAGE_RTT_AT24C_H_ */
