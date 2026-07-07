/**
 * @file CO_driver_target.h
 * @brief CANopenNode target definitions for RT-Thread CAN devices.
 * @details This header defines the target CAN message types, CAN module state, locking
 *          primitives, and helper macros required by CANopenNode on RT-Thread.
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
#ifndef CO_DRIVER_TARGET_H_
#define CO_DRIVER_TARGET_H_

/* Includes ------------------------------------------------------------------*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <rtthread.h>
#include <rthw.h>
#include <drivers/dev_can.h>

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

#define CO_LITTLE_ENDIAN
#define CO_SWAP_16(x)                  (x)
#define CO_SWAP_32(x)                  (x)
#define CO_SWAP_64(x)                  (x)

#define CO_RTT_CAN_STD_MASK            0x7FFU
#define CO_RTT_CAN_RTR_FLAG            0x0800U
#define CO_RTT_CAN_RX_BATCH_SIZE       ((uint16_t)PKG_CANOPENNODE_RX_BATCH_SIZE)
#define CO_RTT_CAN_BINDING_COUNT       PKG_CANOPENNODE_CAN_BINDING_COUNT

#include "CO_config_rtt.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Exported types ------------------------------------------------------------*/

typedef uint_fast8_t bool_t;
typedef float float32_t;  /**< CANopen REAL32 storage type. */
typedef double float64_t; /**< CANopen REAL64 storage type. */

/**
 * @brief RT-Thread CAN receive frame type used by CANopenNode callbacks.
 */
typedef struct rt_can_msg CO_CANrxMsg_t;

/**
 * @brief Read the 11-bit CANopen identifier from an RT-Thread CAN frame.
 *
 * @param rxMsg Pointer to an RT-Thread CAN message.
 * @return Standard CAN identifier, without RTR information.
 */
static inline uint16_t CO_CANrxMsg_readIdent(void *rxMsg)
{
    const struct rt_can_msg *msg = (const struct rt_can_msg *)rxMsg;

    return (uint16_t)(msg->id & CO_RTT_CAN_STD_MASK);
}

/**
 * @brief Read the CAN data length from an RT-Thread CAN frame.
 *
 * @param rxMsg Pointer to an RT-Thread CAN message.
 * @return CAN data length in bytes.
 */
static inline uint8_t CO_CANrxMsg_readDLC(void *rxMsg)
{
    const struct rt_can_msg *msg = (const struct rt_can_msg *)rxMsg;

    return (uint8_t)msg->len;
}

/**
 * @brief Read the CAN payload pointer from an RT-Thread CAN frame.
 *
 * @param rxMsg Pointer to an RT-Thread CAN message.
 * @return Pointer to the CAN payload bytes.
 */
static inline const uint8_t *CO_CANrxMsg_readData(void *rxMsg)
{
    const struct rt_can_msg *msg = (const struct rt_can_msg *)rxMsg;

    return msg->data;
}

/**
 * @brief CANopenNode receive buffer descriptor.
 */
typedef struct {
    uint16_t ident;       /**< Standard CAN identifier in bits 0..10 and RTR in bit 11. */
    uint16_t mask;        /**< Identifier and RTR mask with the same alignment as ident. */
    void *object;         /**< CANopenNode object passed to the receive callback. */
    void (*pCANrx_callback)(void *object, void *message); /**< Fast receive callback. */
#ifdef RT_CAN_USING_HDR
    rt_int32_t hdr_bank;  /**< Optional RT-Thread hardware filter bank, or -1 for software dispatch. */
#endif /* RT_CAN_USING_HDR */
} CO_CANrx_t;

/**
 * @brief CANopenNode transmit buffer descriptor backed by RT-Thread CAN frame storage.
 */
typedef struct {
    uint32_t ident;               /**< Standard CAN identifier in bits 0..10 and RTR in bit 11. */
    uint8_t DLC;                  /**< CAN data length in bytes. */
    uint8_t data[8];              /**< CANopenNode-visible CAN payload storage. */
    volatile bool_t bufferFull;   /**< True while a CANopenNode software-pending frame occupies this buffer. */
    volatile bool_t syncFlag;     /**< True for synchronous PDO frames. */
} CO_CANtx_t;

/**
 * @brief RT-Thread CAN module state used by CANopenNode.
 */
typedef struct {
    rt_device_t dev;                 /**< RT-Thread CAN device handle. */
    struct rt_semaphore rx_sem;      /**< RX wake semaphore released by RX indication or the stop path. */
    struct rt_semaphore rxExitSem;   /**< Released after the RX helper no longer accesses CANmodule-owned state. */
    rt_thread_t rx_thread;           /**< RX helper thread handle. */
    volatile rt_bool_t rxStop;       /**< True requests cooperative RX helper thread exit. */
    volatile rt_bool_t rxRunning;    /**< True while the RX helper may access RX buffers or callback objects. */

    struct rt_mutex canSendMutex;    /**< Protects CANopenNode transmit buffer state. */
    struct rt_mutex emcyMutex;       /**< Protects CANopenNode Emergency state. */
    struct rt_mutex odMutex;         /**< Protects PDO-mappable Object Dictionary access. */

    CO_CANrx_t *rxArray;             /**< CANopenNode receive buffer array. */
    uint16_t rxSize;                 /**< Number of receive buffers. */
    CO_CANtx_t *txArray;             /**< CANopenNode transmit buffer array. */
    uint16_t txSize;                 /**< Number of transmit buffers. */

    uint16_t CANerrorStatus;         /**< CANopenNode CAN error status bitfield. */
    uint16_t CANtxEventStatus;       /**< One-shot TX event bits consumed by CO_CANmodule_process(). */
    volatile bool_t CANnormal;       /**< True after the RT-Thread CAN device enters normal operation. */
    volatile bool_t useCANrxFilters; /**< True if RT-Thread HDR filters are active for all RX buffers. */
    volatile bool_t bufferInhibitFlag; /**< Reserved for CANopenNode-compatible synchronous TPDO inhibit state. */
    volatile bool_t firstCANtxMessage; /**< True while the first CANopen transmit message is pending. */
    volatile uint16_t CANtxCount;    /**< Number of CANopenNode software-pending transmit buffers. */
    uint32_t rxDropOld;              /**< Previous RT-Thread dropped-RX-frame counter. */
    uint32_t txDropOld;              /**< Previous RT-Thread dropped-TX-frame counter. */
    uint32_t errOld;                 /**< Previous raw RT-Thread CAN error/drop snapshot. */
} CO_CANmodule_t;

/**
 * @brief Data storage object descriptor used by CANopenNode storage code.
 */
typedef struct {
    void *addr;             /**< Address of data to store. */
    size_t len;             /**< Length of data to store. */
    uint8_t subIndexOD;     /**< Sub-index in OD objects 1010h/1011h. */
    uint8_t attr;           /**< Storage attributes. */
    void *storageModule;    /**< Storage backend private data pointer. */
#if defined(PKG_CANOPENNODE_USING_STORAGE_EEPROM)
    size_t eepromAddrSignature; /**< EEPROM address of the entry signature used by CO_storageEeprom.c. */
    size_t eepromAddr;          /**< EEPROM address of the entry payload used by CO_storageEeprom.c. */
    size_t offset;              /**< Automatic storage byte offset used by CO_storageEeprom_auto_process(). */
    uint16_t crc;               /**< CRC value used by CO_storageEeprom.c. */
    bool_t eepromPayloadChanged; /**< True when automatic storage changed payload before signature refresh. */
#endif /* defined(PKG_CANOPENNODE_USING_STORAGE_EEPROM) */
} CO_storage_entry_t;

/*
 * This RT-Thread port runs CANopenNode from RT-Thread threads: co_rx dispatches
 * CAN receive callbacks, co_rt runs SYNC/SRDO/RPDO/TPDO processing when enabled,
 * and co_main runs asynchronous CANopen processing. CANopenNode APIs that may
 * take these locks
 * must not be called directly from ISR context; defer ISR work to a thread.
 */

/**
 * @brief Lock CANopenNode transmit buffer state.
 */
#define CO_LOCK_CAN_SEND(CAN_MODULE)    { CO_CANmodule_t *coRttCanSendModule = (CAN_MODULE); \
                                          (void)rt_mutex_take(&coRttCanSendModule->canSendMutex, RT_WAITING_FOREVER)

/**
 * @brief Unlock CANopenNode transmit buffer state.
 */
#define CO_UNLOCK_CAN_SEND(CAN_MODULE)  (void)(CAN_MODULE); \
                                          (void)rt_mutex_release(&coRttCanSendModule->canSendMutex); }

/**
 * @brief Lock CANopenNode Emergency state.
 */
#define CO_LOCK_EMCY(CAN_MODULE)        { CO_CANmodule_t *coRttEmcyModule = (CAN_MODULE); \
                                          (void)rt_mutex_take(&coRttEmcyModule->emcyMutex, RT_WAITING_FOREVER)

/**
 * @brief Unlock CANopenNode Emergency state.
 */
#define CO_UNLOCK_EMCY(CAN_MODULE)      (void)(CAN_MODULE); \
                                          (void)rt_mutex_release(&coRttEmcyModule->emcyMutex); }

/**
 * @brief Lock PDO-mappable Object Dictionary access.
 */
#define CO_LOCK_OD(CAN_MODULE)          { CO_CANmodule_t *coRttOdModule = (CAN_MODULE); \
                                          (void)rt_mutex_take(&coRttOdModule->odMutex, RT_WAITING_FOREVER)

/**
 * @brief Unlock PDO-mappable Object Dictionary access.
 */
#define CO_UNLOCK_OD(CAN_MODULE)        (void)(CAN_MODULE); \
                                          (void)rt_mutex_release(&coRttOdModule->odMutex); }

/**
 * @brief Read a CANopenNode receive-new flag under a short interrupt lock.
 *
 * @param rxNew Address of the CANopenNode flag pointer. NULL means no pending data; non-NULL means pending data.
 * @return true when @p rxNew currently stores a non-NULL flag value, otherwise false.
 */
static inline bool_t co_rtt_flag_read(void *volatile *rxNew)
{
    bool_t flag;
    rt_base_t level = rt_hw_interrupt_disable();

    flag = (*rxNew != NULL);
    rt_hw_interrupt_enable(level);

    return flag;
}

/**
 * @brief Set a CANopenNode receive-new flag under a short interrupt lock.
 *
 * The interrupt-disabled section protects only the pointer-sized flag update so
 * RX callback and processing thread cannot observe a torn flag transition.
 *
 * @param rxNew Address of the CANopenNode flag pointer to set to a non-NULL value.
 */
static inline void co_rtt_flag_set(void *volatile *rxNew)
{
    rt_base_t level = rt_hw_interrupt_disable();

    *rxNew = (void *)1L;
    rt_hw_interrupt_enable(level);
}

/**
 * @brief Clear a CANopenNode receive-new flag under a short interrupt lock.
 *
 * The interrupt-disabled section protects only the pointer-sized flag update so
 * RX callback and processing thread cannot observe a torn flag transition.
 *
 * @param rxNew Address of the CANopenNode flag pointer to clear to NULL.
 */
static inline void co_rtt_flag_clear(void *volatile *rxNew)
{
    rt_base_t level = rt_hw_interrupt_disable();

    *rxNew = NULL;
    rt_hw_interrupt_enable(level);
}

#define CO_FLAG_READ(rxNew)             co_rtt_flag_read((void *volatile *)&(rxNew))
#define CO_FLAG_SET(rxNew)              do { co_rtt_flag_set((void *volatile *)&(rxNew)); } while (0)
#define CO_FLAG_CLEAR(rxNew)            do { co_rtt_flag_clear((void *volatile *)&(rxNew)); } while (0)

/* Exported variables ---------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_DRIVER_TARGET_H_ */
