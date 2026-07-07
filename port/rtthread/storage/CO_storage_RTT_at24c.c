/**
 * @file CO_storage_RTT_at24c.c
 * @brief AT24CXX device adapter for CANopenNode EEPROM storage on RT-Thread.
 * @details This file implements the RT-Thread AT24CXX adapter required by the CANopenNode EEPROM storage backend.
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

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/
#include "CO_storage_RTT_at24c.h"

#if ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0
#if defined(PKG_CANOPENNODE_USING_STORAGE_EEPROM) && defined(PKG_CANOPENNODE_USING_STORAGE_AT24C)

#include "301/crc16-ccitt.h"
#include "co_rtt_log.h"
#include "storage/CO_eeprom.h"

#include <limits.h>
#include <string.h>

/* Private define ------------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/**
 * @brief AT24CXX device state for one CANopenNode instance.
 */
typedef struct {
    bool_t used;             /**< True when this static slot is assigned. */
    CO_storage_t *storage;   /**< Storage object owning this slot. */
    const char *i2cBusName;  /**< RT-Thread I2C bus name passed to at24cxx_init(). */
    uint8_t addrInput;       /**< AT24CXX package address-input value. */
    size_t size;             /**< EEPROM capacity in bytes from at24cxx.h. */
    size_t nextAddress;      /**< Next allocated EEPROM address. */
    void *device;            /**< AT24CXX package device handle. */
    uint8_t initialized;     /**< Non-zero after the AT24CXX package device has been opened. */
} CO_storage_rtt_at24c_instance_t;

/** Static AT24CXX device slot. The built-in EEPROM backend supports only one CANopenNode instance. */
static CO_storage_rtt_at24c_instance_t co_storage_rtt_at24c_instance;

/**
 * @brief Allocate or reuse the single AT24CXX device slot.
 *
 * @param storage Storage object that owns the device slot.
 * @return AT24CXX device slot, or NULL if another storage object already owns it.
 */
static CO_storage_rtt_at24c_instance_t *co_storage_rtt_at24c_instance_get(CO_storage_t *storage)
{
    if ((co_storage_rtt_at24c_instance.used) && (co_storage_rtt_at24c_instance.storage == storage)) {
        return &co_storage_rtt_at24c_instance;
    }

    if (co_storage_rtt_at24c_instance.used) {
        CO_RTT_LOG_E("AT24CXX storage backend supports only one CANopenNode instance");
        return NULL;
    }

    memset(&co_storage_rtt_at24c_instance, 0, sizeof(co_storage_rtt_at24c_instance));
    co_storage_rtt_at24c_instance.used = true;
    co_storage_rtt_at24c_instance.storage = storage;

    return &co_storage_rtt_at24c_instance;
}

/**
 * @brief Configure an AT24CXX device slot from Kconfig and at24cxx.h.
 *
 * The AT24CXX device handle is preserved across communication resets so repeated
 * CANopen stack recreation does not leak the mutex and device object allocated by
 * at24cxx_init().
 *
 * @param instance AT24CXX device slot to configure.
 */
static void co_storage_rtt_at24c_config_init(CO_storage_rtt_at24c_instance_t *instance)
{
    void *device;
    uint8_t initialized;

    if (instance == NULL) {
        return;
    }

    device = instance->device;
    initialized = instance->initialized;
    instance->i2cBusName = PKG_CANOPENNODE_STORAGE_AT24C_I2C_BUS_NAME;
    instance->addrInput = (uint8_t)PKG_CANOPENNODE_STORAGE_AT24C_ADDR_INPUT;
    instance->size = (size_t)AT24CXX_MAX_MEM_ADDRESS;
    instance->nextAddress = (size_t)PKG_CANOPENNODE_STORAGE_AT24C_OFFSET;
    instance->device = device;
    instance->initialized = initialized;
}

/**
 * @brief Get the AT24CXX EEPROM module for one storage object.
 *
 * @param storage Storage object requesting the EEPROM module.
 * @param instanceName Optional CANopenNodeRTT instance name. It is unused because this adapter is single-instance.
 * @return AT24CXX backend instance pointer, or NULL if the single module slot is already owned.
 */
void *co_storage_rtt_eeprom_module_get(CO_storage_t *storage, const char *instanceName)
{
    CO_storage_rtt_at24c_instance_t *instance;

    (void)instanceName;

    instance = co_storage_rtt_at24c_instance_get(storage);
    if (instance == NULL) {
        return NULL;
    }

    co_storage_rtt_at24c_config_init(instance);
    return instance;
}

/**
 * @brief Validate a requested EEPROM range.
 *
 * @param instance AT24CXX device slot.
 * @param eepromAddr Start address.
 * @param len Number of bytes.
 * @return true if the range is inside EEPROM capacity, otherwise false.
 */
static bool_t co_storage_rtt_at24c_range_valid(const CO_storage_rtt_at24c_instance_t *instance,
                                               size_t eepromAddr,
                                               size_t len)
{
    if ((instance == NULL) || (instance->initialized == 0U) || (instance->device == RT_NULL)) {
        return false;
    }
    if ((len == 0U) || (eepromAddr >= instance->size)) {
        return false;
    }

    return (len <= (instance->size - eepromAddr));
}

/**
 * @brief Read a block through the AT24CXX package.
 *
 * @param instance AT24CXX device slot.
 * @param data Destination buffer.
 * @param eepromAddr EEPROM start address.
 * @param len Number of bytes to read.
 * @return true on success, otherwise false.
 */
static bool_t co_storage_rtt_at24c_read_block(CO_storage_rtt_at24c_instance_t *instance,
                                              uint8_t *data,
                                              size_t eepromAddr,
                                              size_t len)
{
    size_t offset = 0U;

    if ((data == NULL) || !co_storage_rtt_at24c_range_valid(instance, eepromAddr, len)) {
        return false;
    }

    while (offset < len) {
        size_t remaining = len - offset;
        uint16_t chunk = (remaining > (size_t)UINT16_MAX) ? (uint16_t)UINT16_MAX : (uint16_t)remaining;

        if (at24cxx_read((at24cxx_device_t)instance->device, (uint32_t)(eepromAddr + offset), &data[offset],
                         chunk) != RT_EOK) {
            CO_RTT_LOG_W("AT24CXX read failed: bus=%s addrInput=%u offset=%lu len=%u", instance->i2cBusName,
                         instance->addrInput, (unsigned long)(eepromAddr + offset), chunk);
            return false;
        }
        offset += chunk;
    }

    return true;
}

/**
 * @brief Write a block through the AT24CXX package.
 *
 * @param instance AT24CXX device slot.
 * @param data Source buffer.
 * @param eepromAddr EEPROM start address.
 * @param len Number of bytes to write.
 * @return true on success, otherwise false.
 */
static bool_t co_storage_rtt_at24c_write_block(CO_storage_rtt_at24c_instance_t *instance,
                                               uint8_t *data,
                                               size_t eepromAddr,
                                               size_t len)
{
    size_t offset = 0U;

    if ((data == NULL) || !co_storage_rtt_at24c_range_valid(instance, eepromAddr, len)) {
        return false;
    }

    while (offset < len) {
        size_t remaining = len - offset;
        uint16_t chunk = (remaining > (size_t)UINT16_MAX) ? (uint16_t)UINT16_MAX : (uint16_t)remaining;

        if (at24cxx_write((at24cxx_device_t)instance->device, (uint32_t)(eepromAddr + offset), &data[offset],
                          chunk) != RT_EOK) {
            CO_RTT_LOG_W("AT24CXX write failed: bus=%s addrInput=%u offset=%lu len=%u", instance->i2cBusName,
                         instance->addrInput, (unsigned long)(eepromAddr + offset), chunk);
            return false;
        }
        offset += chunk;
    }

    return true;
}

/**
 * @brief Initialize the AT24CXX EEPROM module.
 *
 * @param storageModule AT24CXX backend instance returned by co_storage_rtt_eeprom_module_get().
 * @return true on successful initialization, otherwise false.
 */
bool_t CO_eeprom_init(void *storageModule)
{
    CO_storage_rtt_at24c_instance_t *instance = (CO_storage_rtt_at24c_instance_t *)storageModule;

    if ((instance == NULL) || (instance->i2cBusName == NULL) || (instance->i2cBusName[0] == '\0')
        || (instance->size == 0U) || (instance->nextAddress >= instance->size)) {
        CO_RTT_LOG_E("AT24CXX init failed: invalid configuration");
        return false;
    }

    if (instance->device == RT_NULL) {
        instance->device = at24cxx_init(instance->i2cBusName, instance->addrInput);
        if (instance->device == RT_NULL) {
            CO_RTT_LOG_E("AT24CXX init failed: bus=%s addrInput=%u", instance->i2cBusName, instance->addrInput);
            return false;
        }
    }

    instance->initialized = 1U;
    CO_RTT_LOG_I("AT24CXX storage initialized: bus=%s addrInput=%u size=%lu offset=%lu page=%u",
                 instance->i2cBusName, instance->addrInput, (unsigned long)instance->size,
                 (unsigned long)instance->nextAddress, AT24CXX_PAGE_BYTE);

    return true;
}

/**
 * @brief Allocate an EEPROM address range for one storage entry.
 *
 * @param storageModule AT24CXX backend instance.
 * @param isAuto True when the caller allocates automatic storage. It is unused by this adapter.
 * @param len Number of bytes to allocate.
 * @param overflow Output flag set to true when the requested range does not fit.
 * @return Allocated EEPROM start address, or the failed start address when overflow is true.
 */
size_t CO_eeprom_getAddr(void *storageModule, bool_t isAuto, size_t len, bool_t *overflow)
{
    CO_storage_rtt_at24c_instance_t *instance = (CO_storage_rtt_at24c_instance_t *)storageModule;
    size_t addr = 0U;

    (void)isAuto;

    if (overflow != NULL) {
        *overflow = true;
    }
    if ((instance == NULL) || (instance->initialized == 0U) || (len == 0U)) {
        return 0U;
    }

    addr = instance->nextAddress;
    if ((addr >= instance->size) || (len > (instance->size - addr))) {
        return addr;
    }

    instance->nextAddress += len;
    if (overflow != NULL) {
        *overflow = false;
    }

    return addr;
}

/**
 * @brief Read one EEPROM block into RAM.
 *
 * @param storageModule AT24CXX backend instance.
 * @param data Destination buffer.
 * @param eepromAddr EEPROM start address.
 * @param len Number of bytes to read.
 */
void CO_eeprom_readBlock(void *storageModule, uint8_t *data, size_t eepromAddr, size_t len)
{
    CO_storage_rtt_at24c_instance_t *instance = (CO_storage_rtt_at24c_instance_t *)storageModule;

    if ((data == NULL) || !co_storage_rtt_at24c_read_block(instance, data, eepromAddr, len)) {
        if (data != NULL) {
            memset(data, 0xFF, len);
        }
    }
}

/**
 * @brief Write one EEPROM block from RAM.
 *
 * @param storageModule AT24CXX backend instance.
 * @param data Source buffer.
 * @param eepromAddr EEPROM start address.
 * @param len Number of bytes to write.
 * @return true on success, otherwise false.
 */
bool_t CO_eeprom_writeBlock(void *storageModule, uint8_t *data, size_t eepromAddr, size_t len)
{
    return co_storage_rtt_at24c_write_block((CO_storage_rtt_at24c_instance_t *)storageModule, data, eepromAddr, len);
}

/**
 * @brief Calculate CRC16 over one EEPROM block.
 *
 * @param storageModule AT24CXX backend instance.
 * @param eepromAddr EEPROM start address.
 * @param len Number of bytes included in the CRC calculation.
 * @return CRC16-CCITT value, or 0 if the EEPROM block cannot be read.
 */
uint16_t CO_eeprom_getCrcBlock(void *storageModule, size_t eepromAddr, size_t len)
{
    CO_storage_rtt_at24c_instance_t *instance = (CO_storage_rtt_at24c_instance_t *)storageModule;
    uint8_t buf[PKG_CANOPENNODE_STORAGE_AT24C_CRC_BUF_SIZE];
    uint16_t crc = 0U;
    size_t offset = 0U;

    if (!co_storage_rtt_at24c_range_valid(instance, eepromAddr, len)) {
        return 0U;
    }

    while (offset < len) {
        size_t remaining = len - offset;
        size_t chunk = (remaining < sizeof(buf)) ? remaining : sizeof(buf);

        if (!co_storage_rtt_at24c_read_block(instance, buf, eepromAddr + offset, chunk)) {
            return 0U;
        }
        crc = crc16_ccitt(buf, chunk, crc);
        offset += chunk;
    }

    return crc;
}

/**
 * @brief Update one EEPROM byte only when the stored value differs.
 *
 * @param storageModule AT24CXX backend instance.
 * @param data Byte value to write.
 * @param eepromAddr EEPROM byte address.
 * @return true on success or when the byte already has the requested value, otherwise false.
 */
bool_t CO_eeprom_updateByte(void *storageModule, uint8_t data, size_t eepromAddr)
{
    CO_storage_rtt_at24c_instance_t *instance = (CO_storage_rtt_at24c_instance_t *)storageModule;
    uint8_t storedData;

    if (!co_storage_rtt_at24c_read_block(instance, &storedData, eepromAddr, sizeof(storedData))) {
        return false;
    }
    if (storedData == data) {
        return true;
    }

    return co_storage_rtt_at24c_write_block(instance, &data, eepromAddr, sizeof(data));
}

#endif /* defined(PKG_CANOPENNODE_USING_STORAGE_EEPROM) && defined(PKG_CANOPENNODE_USING_STORAGE_AT24C) */
#endif /* ((CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE) != 0 */
