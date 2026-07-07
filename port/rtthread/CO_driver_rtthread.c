/**
 * @file CO_driver_rtthread.c
 * @brief CANopenNode CAN driver binding for RT-Thread CAN devices.
 * @details This file implements the glue logic between the CANopenNode CAN driver
 *          abstraction and RT-Thread CAN device APIs, including RX dispatch, TX
 *          submission, filtering, and CAN status processing.
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
#define LOG_TAG                         "canopen.rtt"
#define LOG_LVL                         LOG_LVL_DBG

/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/
#include "CO_driver.h"
#include "co_rtt_log.h"

#include <string.h>

/* Private function prototypes -----------------------------------------------*/

/**
 * @brief Convert a CANopenNode bitrate in kbit/s to RT-Thread CAN baud rate.
 *
 * @param bitrate CANopenNode bitrate in kbit/s.
 * @return RT-Thread baud rate in bit/s, or 0 for unsupported values.
 */
static rt_uint32_t co_rtt_bitrate_to_baud(uint16_t bitrate)
{
    switch (bitrate) {
    case 10U:
    case 20U:
    case 50U:
    case 125U:
    case 250U:
    case 500U:
    case 800U:
    case 1000U:
        return ((rt_uint32_t)bitrate * 1000U);
    default:
        return 0U;
    }
}

/**
 * @brief Convert an RT-Thread CAN write result to a CANopenNode return code.
 *
 * @param written Value returned from rt_device_write().
 * @param expected Expected write size in bytes.
 * @return CO_ERROR_NO on complete write, otherwise a mapped CANopenNode error.
 */
static CO_ReturnError_t co_rtt_write_result_to_error(rt_ssize_t written, rt_size_t expected)
{
    if (written == (rt_ssize_t)expected) {
        return CO_ERROR_NO;
    }

    if (written == 0) {
        return CO_ERROR_TX_BUSY;
    }

    if (written > 0) {
        return CO_ERROR_SYSCALL;
    }

#if defined(RT_EINVAL)
    if (written == -RT_EINVAL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }
#endif /* defined(RT_EINVAL) */

#if defined(RT_ETIMEOUT)
    if (written == -RT_ETIMEOUT) {
        return CO_ERROR_TIMEOUT;
    }
#endif /* defined(RT_ETIMEOUT) */

    return CO_ERROR_SYSCALL;
}

#ifdef PKG_CANOPENNODE_USING_FRAME_TRACE
/**
 * @brief Trace one CAN frame through RT-Thread ulog.
 *
 * @param direction Direction label such as RX or TX.
 * @param msg RT-Thread CAN frame to trace.
 */
static void co_rtt_trace_frame(const char *direction, const struct rt_can_msg *msg)
{
    rt_size_t data_len = msg->len;

    if (data_len > sizeof(msg->data)) {
        data_len = sizeof(msg->data);
    }

    CO_RTT_LOG_D("%s ID:%03lx IDE:%u RTR:%u DLC:%u", direction, (unsigned long)(msg->id & CO_RTT_CAN_STD_MASK), msg->ide,
                 msg->rtr, msg->len);
    if (data_len > 0U) {
        CO_RTT_LOG_HEX("data", 8, msg->data, data_len);
    }
}
#else
#define co_rtt_trace_frame(direction, msg) do { (void)(direction); (void)(msg); } while (0)
#endif /* PKG_CANOPENNODE_USING_FRAME_TRACE */

/**
 * @brief Binding item used to map one RT-Thread CAN device to a CANopenNode CAN module.
 */
typedef struct {
    rt_device_t dev;              /**< RT-Thread CAN device handle. */
    CO_CANmodule_t *CANmodule;    /**< CANopenNode CAN module bound to the device. */
} co_rtt_can_binding_t;

#if CO_RTT_CAN_BINDING_COUNT == 1
static co_rtt_can_binding_t co_rtt_can_binding;

/**
 * @brief Find the CANopenNode module bound to an RT-Thread CAN device.
 *
 * @param dev RT-Thread CAN device handle.
 * @return Bound CAN module, or NULL if the device is not bound.
 */
static CO_CANmodule_t *co_rtt_can_binding_find(rt_device_t dev)
{
    return (co_rtt_can_binding.dev == dev) ? co_rtt_can_binding.CANmodule : NULL;
}

/**
 * @brief Bind an RT-Thread CAN device to a CANopenNode CAN module.
 *
 * @param dev RT-Thread CAN device handle.
 * @param CANmodule CANopenNode CAN module.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
static int co_rtt_can_binding_add(rt_device_t dev, CO_CANmodule_t *CANmodule)
{
    if (co_rtt_can_binding.dev == dev) {
        return (co_rtt_can_binding.CANmodule == CANmodule) ? RT_EOK : -RT_EBUSY;
    }

    if (co_rtt_can_binding.dev != RT_NULL) {
        return -RT_EBUSY;
    }

    co_rtt_can_binding.dev = dev;
    co_rtt_can_binding.CANmodule = CANmodule;

    return RT_EOK;
}

/**
 * @brief Remove a CANopenNode CAN module from the RT-Thread CAN binding slot.
 *
 * @param CANmodule CANopenNode CAN module.
 */
static void co_rtt_can_binding_remove(CO_CANmodule_t *CANmodule)
{
    if (co_rtt_can_binding.CANmodule == CANmodule) {
        co_rtt_can_binding.dev = RT_NULL;
        co_rtt_can_binding.CANmodule = NULL;
    }
}

#else
static co_rtt_can_binding_t co_rtt_can_bindings[CO_RTT_CAN_BINDING_COUNT];

/**
 * @brief Find the CANopenNode module bound to an RT-Thread CAN device.
 *
 * @param dev RT-Thread CAN device handle.
 * @return Bound CAN module, or NULL if the device is not bound.
 */
static CO_CANmodule_t *co_rtt_can_binding_find(rt_device_t dev)
{
    uint8_t i;

    for (i = 0U; i < CO_RTT_CAN_BINDING_COUNT; i++) {
        if (co_rtt_can_bindings[i].dev == dev) {
            return co_rtt_can_bindings[i].CANmodule;
        }
    }

    return NULL;
}

/**
 * @brief Bind an RT-Thread CAN device to a CANopenNode CAN module.
 *
 * @param dev RT-Thread CAN device handle.
 * @param CANmodule CANopenNode CAN module.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
static int co_rtt_can_binding_add(rt_device_t dev, CO_CANmodule_t *CANmodule)
{
    uint8_t i;

    for (i = 0U; i < CO_RTT_CAN_BINDING_COUNT; i++) {
        if (co_rtt_can_bindings[i].dev == dev) {
            return (co_rtt_can_bindings[i].CANmodule == CANmodule) ? RT_EOK : -RT_EBUSY;
        }
    }

    for (i = 0U; i < CO_RTT_CAN_BINDING_COUNT; i++) {
        if (co_rtt_can_bindings[i].dev == RT_NULL) {
            co_rtt_can_bindings[i].dev = dev;
            co_rtt_can_bindings[i].CANmodule = CANmodule;
            return RT_EOK;
        }
    }

    return -RT_EBUSY;
}

/**
 * @brief Remove a CANopenNode CAN module from the RT-Thread CAN binding table.
 *
 * @param CANmodule CANopenNode CAN module.
 */
static void co_rtt_can_binding_remove(CO_CANmodule_t *CANmodule)
{
    uint8_t i;

    for (i = 0U; i < CO_RTT_CAN_BINDING_COUNT; i++) {
        if (co_rtt_can_bindings[i].CANmodule == CANmodule) {
            co_rtt_can_bindings[i].dev = RT_NULL;
            co_rtt_can_bindings[i].CANmodule = NULL;
            break;
        }
    }
}

#endif /* CO_RTT_CAN_BINDING_COUNT == 1 */

/**
 * @brief Submit one CAN frame through the RT-Thread CAN blocking TX path.
 *
 * @param CANmodule CANopenNode CAN module.
 * @param buffer CANopenNode transmit buffer.
 * @return CO_ERROR_NO on successful completion, otherwise a mapped CANopenNode error.
 */
static CO_ReturnError_t co_rtt_submit_msg(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer)
{
    CO_ReturnError_t ret;
    struct rt_can_msg msg;
    rt_ssize_t written;
    rt_size_t data_len = buffer->DLC;

    memset(&msg, 0, sizeof(msg));
    msg.id = (rt_uint32_t)(buffer->ident & CO_RTT_CAN_STD_MASK);
    msg.ide = RT_CAN_STDID;
    msg.rtr = ((buffer->ident & CO_RTT_CAN_RTR_FLAG) != 0U) ? RT_CAN_RTR : RT_CAN_DTR;
    msg.len = buffer->DLC;
    msg.priv = 0U;
#ifdef RT_CAN_USING_HDR
    msg.hdr_index = -1;
#endif /* RT_CAN_USING_HDR */

    if (data_len > sizeof(buffer->data)) {
        data_len = sizeof(buffer->data);
    }
    if (data_len > 0U) {
        memcpy(msg.data, buffer->data, data_len);
    }

    co_rtt_trace_frame("TX", &msg);
    written = rt_device_write(CANmodule->dev, 0, &msg, sizeof(msg));
    ret = co_rtt_write_result_to_error(written, sizeof(msg));
    if (ret != CO_ERROR_NO) {
        CO_RTT_LOG_W("tx failed: id=0x%03lx written=%ld ret=%d", (unsigned long)msg.id, (long)written, ret);
    }

    return ret;
}

#ifdef RT_CAN_USING_HDR
#ifdef PKG_CANOPENNODE_USING_RTT_CAN_FILTER
/**
 * @brief Clear RT-Thread HDR bank indexes stored in CANopenNode RX buffers.
 *
 * @param CANmodule CANopenNode CAN module.
 */
static void co_rtt_clear_rx_hdr_banks(CO_CANmodule_t *CANmodule)
{
    uint16_t i;

    for (i = 0U; i < CANmodule->rxSize; i++) {
        CANmodule->rxArray[i].hdr_bank = -1;
    }
}

/**
 * @brief Configure an accept-all standard-frame HDR fallback for software RX dispatch.
 *
 * Two HDR banks are required to accept both standard data and standard RTR frames. If
 * this fallback cannot be configured, software dispatch cannot be guaranteed to see
 * every standard frame, so CAN normal mode must not be entered.
 *
 * @param CANmodule CANopenNode CAN module.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
static rt_err_t co_rtt_apply_accept_all_filter(CO_CANmodule_t *CANmodule)
{
    struct rt_can_filter_item items[2];
    struct rt_can_filter_config config;
    rt_can_t can = (rt_can_t)CANmodule->dev;
    rt_err_t ret;

    if (can->config.maxhdr < 2U) {
        CO_RTT_LOG_E("accept-all CAN filter unavailable: maxhdr=%lu", (unsigned long)can->config.maxhdr);
        return -RT_ERROR;
    }

    memset(items, 0, sizeof(items));
    items[0].id = 0U;
    items[0].ide = RT_CAN_STDID;
    items[0].rtr = RT_CAN_DTR;
    items[0].mode = 0U;
    items[0].mask = 0U;
    items[0].hdr_bank = 0;
    items[0].rxfifo = 0U;
    items[1].id = 0U;
    items[1].ide = RT_CAN_STDID;
    items[1].rtr = RT_CAN_RTR;
    items[1].mode = 0U;
    items[1].mask = 0U;
    items[1].hdr_bank = 1;
    items[1].rxfifo = 0U;

    config.count = 2U;
    config.actived = 1U;
    config.items = items;
    ret = rt_device_control(CANmodule->dev, RT_CAN_CMD_SET_FILTER, &config);
    if (ret != RT_EOK) {
        CO_RTT_LOG_E("accept-all CAN filter setup failed: ret=%ld", (long)ret);
        return ret;
    }

    co_rtt_clear_rx_hdr_banks(CANmodule);
    CANmodule->useCANrxFilters = false;
    CO_RTT_LOG_W("using accept-all standard CAN filter fallback for software RX dispatch");

    return RT_EOK;
}

/**
 * @brief Configure RT-Thread HDR filters from CANopenNode RX buffers.
 *
 * If optimized per-buffer HDR filters cannot be configured, this function falls
 * back to accept-all standard-frame filters so software dispatch still receives
 * every standard frame. Failure of both paths prevents CAN normal mode.
 *
 * @param CANmodule CANopenNode CAN module.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
static rt_err_t co_rtt_apply_rx_filters(CO_CANmodule_t *CANmodule)
{
#ifdef RT_USING_HEAP
    struct rt_can_filter_item *items = RT_NULL;
    struct rt_can_filter_config config;
    rt_err_t ret = RT_EOK;
    uint16_t i;
    rt_can_t can = (rt_can_t)CANmodule->dev;

    if ((can->config.maxhdr == 0U) || (CANmodule->rxSize > can->config.maxhdr)) {
        ret = -RT_ERROR;
        CO_RTT_LOG_W("hardware RX filters unavailable: rxSize=%u maxhdr=%lu", CANmodule->rxSize,
                     (unsigned long)can->config.maxhdr);
        goto fallback_accept_all;
    }

    items = (struct rt_can_filter_item *)rt_calloc(CANmodule->rxSize, sizeof(struct rt_can_filter_item));
    if (items == RT_NULL) {
        ret = -RT_ENOMEM;
        CO_RTT_LOG_W("hardware RX filter allocation failed: count=%u", CANmodule->rxSize);
        goto fallback_accept_all;
    }

    for (i = 0U; i < CANmodule->rxSize; i++) {
        CO_CANrx_t *rx = &CANmodule->rxArray[i];

        items[i].id = (rt_uint32_t)(rx->ident & CO_RTT_CAN_STD_MASK);
        items[i].ide = RT_CAN_STDID;
        items[i].rtr = ((rx->ident & CO_RTT_CAN_RTR_FLAG) != 0U) ? RT_CAN_RTR : RT_CAN_DTR;
        items[i].mode = 0U;
        items[i].mask = (rt_uint32_t)(rx->mask & CO_RTT_CAN_STD_MASK);
        items[i].hdr_bank = (rt_int32_t)i;
        items[i].rxfifo = 0U;
        rx->hdr_bank = (rt_int32_t)i;
    }

    config.count = CANmodule->rxSize;
    config.actived = 1U;
    config.items = items;

    ret = rt_device_control(CANmodule->dev, RT_CAN_CMD_SET_FILTER, &config);
    if (ret != RT_EOK) {
        CO_RTT_LOG_W("hardware RX filter setup failed: ret=%ld", (long)ret);
        goto fallback_accept_all;
    }

    CANmodule->useCANrxFilters = true;
    CO_RTT_LOG_I("enabled %u hardware RX filters", CANmodule->rxSize);
    goto cleanup;

fallback_accept_all:
    co_rtt_clear_rx_hdr_banks(CANmodule);
    CANmodule->useCANrxFilters = false;

    ret = co_rtt_apply_accept_all_filter(CANmodule);
    if (ret != RT_EOK) {
        CO_RTT_LOG_E("accept-all CAN RX filter fallback failed: ret=%ld", (long)ret);
    }

cleanup:
    if (items != RT_NULL) {
        rt_free(items);
    }

    return ret;
#else
    CANmodule->useCANrxFilters = false;
    CO_RTT_LOG_E("hardware RX filters require RT_USING_HEAP for dynamic filter table allocation");
    return -RT_ERROR;
#endif /* RT_USING_HEAP */
}

#endif /* PKG_CANOPENNODE_USING_RTT_CAN_FILTER */
#endif /* RT_CAN_USING_HDR */

/**
 * @brief Dispatch one RT-Thread CAN RX frame to CANopenNode receive callbacks.
 *
 * @param CANmodule CANopenNode CAN module.
 * @param msg RT-Thread CAN frame.
 */
static void co_rtt_can_dispatch(CO_CANmodule_t *CANmodule, const CO_CANrxMsg_t *msg)
{
    uint16_t msgIdent;
    uint16_t i;

    if ((msg->ide != RT_CAN_STDID) || (msg->len > 8U)) {
        CO_RTT_LOG_D("drop unsupported RX frame: id=0x%03lx ide=%u dlc=%u", (unsigned long)msg->id, msg->ide, msg->len);
        return;
    }

#ifdef RT_CAN_USING_HDR
    if (CANmodule->useCANrxFilters && (msg->hdr_index >= 0) && ((uint16_t)msg->hdr_index < CANmodule->rxSize)) {
        CO_CANrx_t *rx = &CANmodule->rxArray[msg->hdr_index];

        if (rx->pCANrx_callback != NULL) {
            rx->pCANrx_callback(rx->object, (void *)msg);
        }
        return;
    }
#endif /* RT_CAN_USING_HDR */

    msgIdent = (uint16_t)(msg->id & CO_RTT_CAN_STD_MASK);
    if (msg->rtr == RT_CAN_RTR) {
        msgIdent |= CO_RTT_CAN_RTR_FLAG;
    }

    for (i = 0U; i < CANmodule->rxSize; i++) {
        CO_CANrx_t *rx = &CANmodule->rxArray[i];

        if ((rx->pCANrx_callback != NULL) && ((((uint16_t)(msgIdent ^ rx->ident)) & rx->mask) == 0U)) {
            rx->pCANrx_callback(rx->object, (void *)msg);
            break;
        }
    }
}

/**
 * @brief RT-Thread CAN RX indication callback used to wake the CANopenNode RX thread.
 *
 * @param dev RT-Thread CAN device.
 * @param size Number of available bytes reported by RT-Thread CAN.
 * @return RT_EOK.
 */
static rt_err_t co_rtt_can_rx_indicate(rt_device_t dev, rt_size_t size)
{
    CO_CANmodule_t *CANmodule;

    (void)size;

    CANmodule = co_rtt_can_binding_find(dev);
    if ((CANmodule == NULL) || (CANmodule->rxStop == RT_TRUE)) {
        CO_RTT_LOG_W("RX indication from unbound or stopping CAN device");
        return -RT_ERROR;
    }

    return rt_sem_release(&CANmodule->rx_sem);
}

/**
 * @brief RT-Thread CAN RX worker thread entry.
 *
 * @param parameter CANopenNode CAN module pointer.
 */
static void co_rtt_rx_thread_entry(void *parameter)
{
    CO_CANmodule_t *CANmodule = (CO_CANmodule_t *)parameter;
    struct rt_can_msg rxbuf[CO_RTT_CAN_RX_BATCH_SIZE];
    rt_device_t can_dev = CANmodule->dev;
    rt_uint32_t i;

    CANmodule->rxRunning = RT_TRUE;

    while (CANmodule->rxStop != RT_TRUE) {
        if (rt_sem_take(&CANmodule->rx_sem, RT_WAITING_FOREVER) != RT_EOK) {
            continue;
        }

        do {
            rt_ssize_t read_size;
            rt_uint32_t count = 0U;

            if (CANmodule->rxStop == RT_TRUE) {
                break;
            }

#ifdef RT_CAN_USING_HDR
            for (i = 0U; i < CO_RTT_CAN_RX_BATCH_SIZE; i++) {
                rxbuf[i].hdr_index = -1;
            }
#endif /* RT_CAN_USING_HDR */

            read_size = rt_device_read(can_dev, 0, rxbuf, sizeof(rxbuf));
            if (read_size < 0) {
                CO_RTT_LOG_W("CAN RX read failed: %ld", (long)read_size);
                break;
            }
            if (read_size == 0) {
                break;
            }

            if ((read_size % (rt_ssize_t)sizeof(rxbuf[0])) != 0) {
                CO_RTT_LOG_W("CAN RX read returned partial frame data: %ld", (long)read_size);
                break;
            }

            count = (rt_uint32_t)(read_size / (rt_ssize_t)sizeof(rxbuf[0]));
            for (i = 0U; i < count; i++) {
                if (CANmodule->rxStop == RT_TRUE) {
                    break;
                }
                co_rtt_trace_frame("RX", &rxbuf[i]);
                co_rtt_can_dispatch(CANmodule, &rxbuf[i]);
            }

            if ((CANmodule->rxStop == RT_TRUE) || (count != CO_RTT_CAN_RX_BATCH_SIZE)) {
                break;
            }
        } while (1);
    }

    /*
     * After this point co_rx no longer accesses CANmodule, rxArray or callback
     * objects. CO_CANmodule_disable() may detach rx_sem and release CANopenNode
     * objects only after this semaphore is observed.
     */
    CANmodule->rxRunning = RT_FALSE;
    CANmodule->rx_thread = RT_NULL;
    (void)rt_sem_release(&CANmodule->rxExitSem);
}

/**
 * @brief Bind RT-Thread RX indication to one CANopenNode CAN module.
 *
 * @param CANmodule CANopenNode CAN module.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
static int co_rtt_rx_bind(CO_CANmodule_t *CANmodule)
{
    rt_err_t err;
    int ret;

    ret = co_rtt_can_binding_add(CANmodule->dev, CANmodule);
    if (ret != RT_EOK) {
        CO_RTT_LOG_E("bind CAN device failed: %d", ret);
        return ret;
    }

    err = rt_device_set_rx_indicate(CANmodule->dev, co_rtt_can_rx_indicate);
    if (err != RT_EOK) {
        co_rtt_can_binding_remove(CANmodule);
        CO_RTT_LOG_E("set CAN RX indication failed: %ld", (long)err);
        return err;
    }

    return RT_EOK;
}

/**
 * @brief Unbind RT-Thread RX indication from one CANopenNode CAN module.
 *
 * @param CANmodule CANopenNode CAN module.
 */
static void co_rtt_rx_unbind(CO_CANmodule_t *CANmodule)
{
    if (CANmodule->dev != RT_NULL) {
        (void)rt_device_set_rx_indicate(CANmodule->dev, RT_NULL);
    }
    co_rtt_can_binding_remove(CANmodule);
}

/**
 * @brief Start the RT-Thread RX helper thread for a CANopenNode CAN module.
 *
 * @param CANmodule CANopenNode CAN module.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
static int co_rtt_rx_start(CO_CANmodule_t *CANmodule)
{
    int ret;

    if ((CANmodule->rx_thread != RT_NULL) || (CANmodule->rxRunning == RT_TRUE)) {
        return RT_EOK;
    }

    CANmodule->rxStop = RT_FALSE;
    CANmodule->rxRunning = RT_FALSE;
    (void)rt_sem_control(&CANmodule->rx_sem, RT_IPC_CMD_RESET, RT_NULL);
    (void)rt_sem_control(&CANmodule->rxExitSem, RT_IPC_CMD_RESET, RT_NULL);

    ret = co_rtt_rx_bind(CANmodule);
    if (ret != RT_EOK) {
        return ret;
    }

    CANmodule->rx_thread = rt_thread_create("co_rx", co_rtt_rx_thread_entry, CANmodule,
                                            PKG_CANOPENNODE_RX_THREAD_STACK_SIZE,
                                            PKG_CANOPENNODE_RX_THREAD_PRIORITY,
                                            PKG_CANOPENNODE_RX_THREAD_TICK);
    if (CANmodule->rx_thread == RT_NULL) {
        co_rtt_rx_unbind(CANmodule);
        CO_RTT_LOG_E("create CAN RX thread failed");
        return -RT_ENOMEM;
    }

    ret = rt_thread_startup(CANmodule->rx_thread);
    if (ret != RT_EOK) {
        rt_thread_t rx_thread = CANmodule->rx_thread;

        CANmodule->rx_thread = RT_NULL;
        (void)rt_thread_delete(rx_thread);
        co_rtt_rx_unbind(CANmodule);
        CO_RTT_LOG_E("start CAN RX thread failed: %d", ret);
        return ret;
    }

    CO_RTT_LOG_I("CAN RX thread started");

    return RT_EOK;
}

/**
 * @brief Stop the RT-Thread RX helper thread if it belongs to the specified CAN module.
 *
 * @param CANmodule CANopenNode CAN module.
 */
static void co_rtt_rx_stop(CO_CANmodule_t *CANmodule)
{
    CANmodule->rxStop = RT_TRUE;
    co_rtt_rx_unbind(CANmodule);

    if ((CANmodule->rx_thread != RT_NULL) || (CANmodule->rxRunning == RT_TRUE)) {
        (void)rt_sem_release(&CANmodule->rx_sem);
        (void)rt_sem_take(&CANmodule->rxExitSem, RT_WAITING_FOREVER);
    }
}

/**
 * @brief Request RT-Thread CAN configuration mode.
 *
 * RT-Thread's public CAN API has no generic CAN controller configuration-mode
 * wait primitive, so this function only keeps the required CANopenNode hook.
 *
 * @param CANptr Optional RT-Thread CAN device name string. If NULL, the Kconfig device name is used.
 */
void CO_CANsetConfigurationMode(void *CANptr)
{
    (void)CANptr;
}

/**
 * @brief Request RT-Thread CAN normal mode and start the RX helper.
 *
 * @param CANmodule CANopenNode CAN module.
 */
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule)
{
    rt_err_t ret;
    int rx_ret;
    rt_uint32_t mode = RT_CAN_MODE_NORMAL;

    CANmodule->CANnormal = false;

    ret = rt_device_control(CANmodule->dev, RT_CAN_CMD_SET_MODE, (void *)(rt_ubase_t)mode);
    if (ret != RT_EOK) {
        CO_RTT_LOG_E("set CAN normal mode failed: %ld", (long)ret);
        return;
    }
#ifdef RT_CAN_USING_HDR
#ifdef PKG_CANOPENNODE_USING_RTT_CAN_FILTER
    ret = co_rtt_apply_rx_filters(CANmodule);
    if (ret != RT_EOK) {
        CO_RTT_LOG_E("configure CAN RX filters failed: ret=%ld", (long)ret);
        return;
    }
#endif /* PKG_CANOPENNODE_USING_RTT_CAN_FILTER */
#endif /* RT_CAN_USING_HDR */

    rx_ret = co_rtt_rx_start(CANmodule);
    if (rx_ret != RT_EOK) {
        CO_RTT_LOG_E("start CAN RX path failed: %d", rx_ret);
        return;
    }

#ifdef RT_CAN_CMD_START
    rt_uint32_t start = RT_TRUE;
    ret = rt_device_control(CANmodule->dev, RT_CAN_CMD_START, &start);
    if (ret != RT_EOK) {
        co_rtt_rx_stop(CANmodule);
        CO_RTT_LOG_E("start CAN device failed: %ld", (long)ret);
        return;
    }
#endif /* RT_CAN_CMD_START */

    CANmodule->CANnormal = true;
    CO_RTT_LOG_I("CAN normal mode requested");
}

/**
 * @brief Initialize CANopenNode CAN module on top of an RT-Thread CAN device.
 *
 * @param CANmodule CANopenNode CAN module.
 * @param CANptr Optional RT-Thread CAN device name string. If NULL, the Kconfig device name is used.
 * @param rxArray CANopenNode RX buffer array.
 * @param rxSize Number of RX buffers.
 * @param txArray CANopenNode TX buffer array.
 * @param txSize Number of TX buffers.
 * @param CANbitRate CAN bitrate in kbit/s.
 * @return CO_ERROR_NO on success, otherwise an error code.
 */
CO_ReturnError_t CO_CANmodule_init(CO_CANmodule_t *CANmodule, void *CANptr, CO_CANrx_t rxArray[], uint16_t rxSize,
                                   CO_CANtx_t txArray[], uint16_t txSize, uint16_t CANbitRate)
{
    const char *dev_name = (CANptr != NULL) ? (const char *)CANptr : PKG_CANOPENNODE_CAN_DEV_NAME;
    rt_device_t dev;
    rt_uint32_t baud;

    if ((CANmodule == NULL) || (rxArray == NULL) || (txArray == NULL)) {
        CO_RTT_LOG_E("CAN module init failed: invalid argument");
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    baud = co_rtt_bitrate_to_baud(CANbitRate);
    if (baud == 0U) {
        CO_RTT_LOG_E("CAN module init failed: unsupported bitrate %u kbit/s", CANbitRate);
        return CO_ERROR_ILLEGAL_BAUDRATE;
    }

    if ((dev_name == NULL) || (dev_name[0] == '\0')) {
        CO_RTT_LOG_E("CAN module init failed: empty CAN device name");
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    dev = rt_device_find(dev_name);
    if (dev == RT_NULL) {
        CO_RTT_LOG_E("CAN module init failed: device %s not found", dev_name);
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    memset(CANmodule, 0, sizeof(*CANmodule));
    CANmodule->dev = dev;
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->CANnormal = false;
    CANmodule->useCANrxFilters = false;
    CANmodule->bufferInhibitFlag = false;
    CANmodule->firstCANtxMessage = true;
    CANmodule->CANtxCount = 0U;
    CANmodule->rxStop = RT_FALSE;
    CANmodule->rxRunning = RT_FALSE;
    CANmodule->rxDropOld = 0U;
    CANmodule->txDropOld = 0U;
    CANmodule->errOld = 0U;

    memset(rxArray, 0, (size_t)rxSize * sizeof(rxArray[0]));
    memset(txArray, 0, (size_t)txSize * sizeof(txArray[0]));
#ifdef RT_CAN_USING_HDR
    {
        uint16_t i;

        for (i = 0U; i < rxSize; i++) {
            rxArray[i].hdr_bank = -1;
        }
    }
#endif /* RT_CAN_USING_HDR */

    if (rt_sem_init(&CANmodule->rx_sem, "co_rx", 0U, RT_IPC_FLAG_FIFO) != RT_EOK) {
        CANmodule->dev = RT_NULL;
        CO_RTT_LOG_E("CAN module init failed: RX semaphore init failed");
        return CO_ERROR_SYSCALL;
    }

    if (rt_sem_init(&CANmodule->rxExitSem, "co_rxe", 0U, RT_IPC_FLAG_FIFO) != RT_EOK) {
        CO_RTT_LOG_E("CAN module init failed: RX exit semaphore init failed");
        goto err_rx_sem;
    }

    if (rt_mutex_init(&CANmodule->canSendMutex, "co_tx", RT_IPC_FLAG_PRIO) != RT_EOK) {
        CO_RTT_LOG_E("CAN module init failed: CAN send mutex init failed");
        goto err_rx_exit_sem;
    }

    if (rt_mutex_init(&CANmodule->emcyMutex, "co_em", RT_IPC_FLAG_PRIO) != RT_EOK) {
        CO_RTT_LOG_E("CAN module init failed: Emergency mutex init failed");
        goto err_can_send_mutex;
    }

    if (rt_mutex_init(&CANmodule->odMutex, "co_od", RT_IPC_FLAG_PRIO) != RT_EOK) {
        CO_RTT_LOG_E("CAN module init failed: OD mutex init failed");
        goto err_emcy_mutex;
    }

    if (rt_device_open(dev, RT_DEVICE_FLAG_INT_RX | RT_DEVICE_FLAG_INT_TX) != RT_EOK) {
        CO_RTT_LOG_E("CAN module init failed: open device %s failed", dev_name);
        goto err_od_mutex;
    }

    if (rt_device_control(dev, RT_CAN_CMD_SET_BAUD, (void *)(rt_ubase_t)baud) != RT_EOK) {
        CO_RTT_LOG_E("CAN module init failed: set baud %lu failed", (unsigned long)baud);
        goto err_device_open;
    }

    CO_RTT_LOG_I("CAN module initialized: dev=%s bitrate=%u kbit/s rx=%u tx=%u", dev_name, CANbitRate, rxSize, txSize);

    return CO_ERROR_NO;

err_device_open:
    (void)rt_device_close(dev);
err_od_mutex:
    (void)rt_mutex_detach(&CANmodule->odMutex);
err_emcy_mutex:
    (void)rt_mutex_detach(&CANmodule->emcyMutex);
err_can_send_mutex:
    (void)rt_mutex_detach(&CANmodule->canSendMutex);
err_rx_exit_sem:
    (void)rt_sem_detach(&CANmodule->rxExitSem);
err_rx_sem:
    (void)rt_sem_detach(&CANmodule->rx_sem);
    CANmodule->dev = RT_NULL;
    return CO_ERROR_SYSCALL;
}

/**
 * @brief Disable the RT-Thread CAN module binding.
 *
 * @param CANmodule CANopenNode CAN module.
 */
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule)
{
    if (CANmodule == NULL) {
        return;
    }

    CANmodule->CANnormal = false;

    if (CANmodule->dev == RT_NULL) {
        return;
    }

    co_rtt_rx_stop(CANmodule);
    (void)rt_device_close(CANmodule->dev);
    (void)rt_mutex_detach(&CANmodule->odMutex);
    (void)rt_mutex_detach(&CANmodule->emcyMutex);
    (void)rt_mutex_detach(&CANmodule->canSendMutex);
    (void)rt_sem_detach(&CANmodule->rxExitSem);
    (void)rt_sem_detach(&CANmodule->rx_sem);
    CANmodule->dev = RT_NULL;
    CO_RTT_LOG_I("CAN module disabled");
}

/**
 * @brief Configure one CANopenNode RX buffer.
 *
 * @param CANmodule CANopenNode CAN module.
 * @param index RX buffer index.
 * @param ident 11-bit CAN identifier.
 * @param mask 11-bit CAN identifier mask.
 * @param rtr True to match RTR frames.
 * @param object Callback object.
 * @param CANrx_callback Callback function.
 * @return CO_ERROR_NO on success, otherwise an error code.
 */
CO_ReturnError_t CO_CANrxBufferInit(CO_CANmodule_t *CANmodule, uint16_t index, uint16_t ident, uint16_t mask, bool_t rtr,
                                    void *object, void (*CANrx_callback)(void *object, void *message))
{
    CO_CANrx_t *buffer;

    if ((CANmodule == NULL) || (CANmodule->rxArray == NULL) || (index >= CANmodule->rxSize)) {
        CO_RTT_LOG_E("RX buffer init failed: index=%u rxSize=%u", index, (CANmodule != NULL) ? CANmodule->rxSize : 0U);
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    buffer = &CANmodule->rxArray[index];
    buffer->ident = (uint16_t)((ident & CO_RTT_CAN_STD_MASK) | (rtr ? CO_RTT_CAN_RTR_FLAG : 0U));
    buffer->mask = (uint16_t)((mask & CO_RTT_CAN_STD_MASK) | CO_RTT_CAN_RTR_FLAG);
    buffer->object = object;
    buffer->pCANrx_callback = CANrx_callback;
#ifdef RT_CAN_USING_HDR
    buffer->hdr_bank = -1;
#endif /* RT_CAN_USING_HDR */

    CO_RTT_LOG_D("RX buffer configured: index=%u ident=0x%03x mask=0x%03x rtr=%u", index, ident & CO_RTT_CAN_STD_MASK,
                 mask & CO_RTT_CAN_STD_MASK, rtr ? 1U : 0U);

#ifdef RT_CAN_USING_HDR
#ifdef PKG_CANOPENNODE_USING_RTT_CAN_FILTER
    rt_err_t ret;
    if (!CANmodule->CANnormal) {
        return CO_ERROR_NO;
    }

    ret = co_rtt_apply_rx_filters(CANmodule);
    if (ret == RT_EOK) {
        return CO_ERROR_NO;
    }

    CO_RTT_LOG_E("refresh CAN RX filters failed: ret=%ld", (long)ret);

    return (ret == -RT_ENOMEM) ? CO_ERROR_OUT_OF_MEMORY : CO_ERROR_SYSCALL;
#endif /* PKG_CANOPENNODE_USING_RTT_CAN_FILTER */
#endif /* RT_CAN_USING_HDR */
    return CO_ERROR_NO;
}

/**
 * @brief Configure one CANopenNode TX buffer.
 *
 * @param CANmodule CANopenNode CAN module.
 * @param index TX buffer index.
 * @param ident 11-bit CAN identifier.
 * @param rtr True to transmit RTR frames.
 * @param noOfBytes Payload length.
 * @param syncFlag True for synchronous PDO frames.
 * @return Configured TX buffer, or NULL on invalid arguments.
 */
CO_CANtx_t *CO_CANtxBufferInit(CO_CANmodule_t *CANmodule, uint16_t index, uint16_t ident, bool_t rtr, uint8_t noOfBytes,
                               bool_t syncFlag)
{
    CO_CANtx_t *buffer;

    if ((CANmodule == NULL) || (CANmodule->txArray == NULL) || (index >= CANmodule->txSize) || (noOfBytes > 8U)) {
        CO_RTT_LOG_E("TX buffer init failed: index=%u txSize=%u dlc=%u", index,
                     (CANmodule != NULL) ? CANmodule->txSize : 0U, noOfBytes);
        return NULL;
    }

    buffer = &CANmodule->txArray[index];
    memset(buffer, 0, sizeof(*buffer));
    buffer->ident = (uint32_t)((ident & CO_RTT_CAN_STD_MASK) | (rtr ? CO_RTT_CAN_RTR_FLAG : 0U));
    buffer->DLC = noOfBytes;
    buffer->bufferFull = false;
    buffer->syncFlag = syncFlag;

    CO_RTT_LOG_D("TX buffer configured: index=%u ident=0x%03x rtr=%u dlc=%u sync=%u", index, ident & CO_RTT_CAN_STD_MASK,
                 rtr ? 1U : 0U, noOfBytes, syncFlag ? 1U : 0U);

    return buffer;
}

/**
 * @brief Send a CANopenNode CAN frame through RT-Thread INT_TX blocking path.
 *
 * RT-Thread CAN core manages TX mailbox availability with its INT_TX
 * semaphore/completion path, so the normal blocking write path either writes the
 * frame or reports a syscall/argument/timeout error. This port does not keep a
 * separate CANopenNode software-pending TX queue in CO_CANtx_t::bufferFull.
 *
 * @param CANmodule CANopenNode CAN module.
 * @param buffer Configured TX buffer.
 * @return CO_ERROR_NO on successful write, otherwise a mapped error code.
 */
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer)
{
    CO_ReturnError_t ret;
    bool_t overflow;

    CO_LOCK_CAN_SEND(CANmodule);
    overflow = buffer->bufferFull;
    if (overflow) {
        CANmodule->CANtxEventStatus |= CO_CAN_ERRTX_OVERFLOW;
    }
    CO_UNLOCK_CAN_SEND(CANmodule);

    if (overflow) {
        CO_RTT_LOG_W("TX overflow: id=0x%03lx", (unsigned long)(buffer->ident & CO_RTT_CAN_STD_MASK));
        return CO_ERROR_TX_OVERFLOW;
    }

    ret = co_rtt_submit_msg(CANmodule, buffer);

    CO_LOCK_CAN_SEND(CANmodule);
    if (ret != CO_ERROR_NO) {
        CANmodule->CANtxEventStatus |= CO_CAN_ERRTX_OVERFLOW;
    }
    CANmodule->firstCANtxMessage = false;
    CANmodule->bufferInhibitFlag = false;
    CANmodule->CANtxCount = 0U;
    CO_UNLOCK_CAN_SEND(CANmodule);

    return ret;
}

/**
 * @brief Clear software-pending synchronous PDO frames.
 *
 * The default RT-Thread port uses blocking rt_device_write() and does not create
 * new software-pending TX buffers. This function still clears any bufferFull
 * synchronous buffer left by external or legacy code so the CANopenNode API
 * remains conservative.
 *
 * @param CANmodule CANopenNode CAN module.
 */
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule)
{
    uint16_t i;
    bool_t cleared = false;

    CO_LOCK_CAN_SEND(CANmodule);
    for (i = 0U; i < CANmodule->txSize; i++) {
        CO_CANtx_t *buffer = &CANmodule->txArray[i];

        if (buffer->syncFlag && buffer->bufferFull) {
            buffer->bufferFull = false;
            if (CANmodule->CANtxCount > 0U) {
                CANmodule->CANtxCount--;
            }
            cleared = true;
        }
    }

    CANmodule->bufferInhibitFlag = false;
    if (cleared) {
        CANmodule->CANtxEventStatus |= CO_CAN_ERRTX_PDO_LATE;
    }
    CO_UNLOCK_CAN_SEND(CANmodule);

    if (cleared) {
        CO_RTT_LOG_W("cleared pending synchronous PDO frames");
    }
}

/**
 * @brief Poll RT-Thread CAN status.
 *
 * @param CANmodule CANopenNode CAN module.
 */
void CO_CANmodule_process(CO_CANmodule_t *CANmodule)
{
    struct rt_can_status status;
    rt_err_t status_ret;
    uint16_t err = 0U;
    uint16_t tx_event_status;
    uint32_t rx_drop = 0U;
    uint32_t tx_drop = 0U;
    uint32_t raw = 0U;
    bool_t status_changed = false;

    CO_LOCK_CAN_SEND(CANmodule);
    tx_event_status = (uint16_t)(CANmodule->CANtxEventStatus & (CO_CAN_ERRTX_PDO_LATE | CO_CAN_ERRTX_OVERFLOW));
    status_ret = rt_device_control(CANmodule->dev, RT_CAN_CMD_GET_STATUS, &status);
    if (status_ret == RT_EOK) {
        CANmodule->CANtxEventStatus &= (uint16_t)~tx_event_status;
        rx_drop = (uint32_t)status.dropedrcvpkg;
        tx_drop = (uint32_t)status.dropedsndpkg;

        err |= tx_event_status;

        if (status.snderrcnt >= 256U) {
            err |= CO_CAN_ERRTX_BUS_OFF;
        } else if (status.snderrcnt >= 128U) {
            err |= CO_CAN_ERRTX_PASSIVE;
        } else if (status.snderrcnt >= 96U) {
            err |= CO_CAN_ERRTX_WARNING;
        }

        if (status.rcverrcnt >= 128U) {
            err |= CO_CAN_ERRRX_PASSIVE;
        } else if (status.rcverrcnt >= 96U) {
            err |= CO_CAN_ERRRX_WARNING;
        }

        if ((rx_drop != CANmodule->rxDropOld) && (((uint32_t)(rx_drop - CANmodule->rxDropOld)) < 0x80000000U)) {
            err |= CO_CAN_ERRRX_OVERFLOW;
        }
        if ((tx_drop != CANmodule->txDropOld) && (((uint32_t)(tx_drop - CANmodule->txDropOld)) < 0x80000000U)) {
            err |= CO_CAN_ERRTX_OVERFLOW;
        }

        raw = ((uint32_t)err << 16) | ((rx_drop & 0xFFU) << 8) | (tx_drop & 0xFFU);

        CANmodule->rxDropOld = rx_drop;
        CANmodule->txDropOld = tx_drop;
        CANmodule->CANerrorStatus = err;
        status_changed = (raw != CANmodule->errOld);
        CANmodule->errOld = raw;
    }
    CO_UNLOCK_CAN_SEND(CANmodule);

    if (status_ret != RT_EOK) {
        CO_RTT_LOG_W("read CAN status failed");
        return;
    }
    if (status_changed) {
        CO_RTT_LOG_W("CAN status changed: rx_err=%lu tx_err=%lu rx_drop=%lu tx_drop=%lu err=0x%04x",
                     (unsigned long)status.rcverrcnt, (unsigned long)status.snderrcnt, (unsigned long)rx_drop,
                     (unsigned long)tx_drop, err);
    }
}
