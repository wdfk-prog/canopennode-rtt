/**
 * @file CO_time_RTT.c
 * @brief Wrapping microsecond time source for the CANopenNode RT-Thread wrapper.
 *
 * The module exposes a wrapping 32-bit microsecond timestamp interface. The
 * default backend uses the RT-Thread system tick and owns no exclusive resource.
 * The optional high-resolution backend owns one package-wide dedicated 1 MHz,
 * 32-bit, up-counting RT-Thread clock timer. Its elapsed helper accounts for the
 * RT-Thread UINT32_MAX-count hardware period without extending the timebase.
 */

#define LOG_TAG                         "canopen.time"
#define LOG_LVL                         LOG_LVL_DBG

#include "CO_time_RTT.h"
#include "co_rtt_log.h"

#include <rtdevice.h>
#include <string.h>

#if defined(PKG_CANOPENNODE_USING_HIGH_RES_TIME)
#include <drivers/clock_time.h>
#endif /* defined(PKG_CANOPENNODE_USING_HIGH_RES_TIME) */

#define CO_RTT_TIME_US_PER_SECOND       1000000U

#if defined(PKG_CANOPENNODE_USING_HIGH_RES_TIME)
/**
 * @brief Package-wide state owned by the exclusive high-resolution time backend.
 *
 * High-resolution mode intentionally supports one CANopenNodeRTT instance only.
 * The selected timer must be dedicated to this backend for the whole instance
 * lifetime. RT-Thread's generic clock-timer metadata does not reliably expose the
 * physical counter width on all BSPs, so the application/BSP must select a real
 * 32-bit timer; this module cannot reject a 16-bit timer by width at runtime.
 */
typedef struct {
    rt_bool_t initialized;   /**< Exclusive high-resolution backend is active. */
    rt_device_t timerDevice; /**< Configured dedicated RT-Thread clock timer device. */
    rt_clock_timer_t *timer; /**< Required 1 MHz, 32-bit, up-counting clock timer. */
} CO_RTT_time_context_t;

static CO_RTT_time_context_t co_rtt_time;
#endif /* defined(PKG_CANOPENNODE_USING_HIGH_RES_TIME) */

/** Return the RT-Thread tick backend as a naturally wrapping 32-bit microsecond timestamp. */
static uint32_t co_rtt_time_tick_now_us(void)
{
    return ((uint32_t)rt_tick_get_millisecond()) * 1000U;
}

#if defined(PKG_CANOPENNODE_USING_HIGH_RES_TIME)
/** Stop and close the configured high-resolution timer after all time readers have stopped. */
static void co_rtt_time_high_res_deinit(void)
{
    if (co_rtt_time.timer != RT_NULL) {
        if ((co_rtt_time.timer->ops != RT_NULL) && (co_rtt_time.timer->ops->stop != RT_NULL)) {
            co_rtt_time.timer->ops->stop(co_rtt_time.timer);
        }
    }
    if (co_rtt_time.timerDevice != RT_NULL) {
        (void)rt_device_close(co_rtt_time.timerDevice);
    }
}

/**
 * @brief Initialize the dedicated high-resolution clock timer.
 *
 * Frequency and count direction are validated through the generic RT-Thread API.
 * Counter width is a board-integration contract: current RT-Thread clock-timer
 * metadata cannot reliably distinguish 16-bit and 32-bit STM32 F4 timers.
 */
static rt_err_t co_rtt_time_high_res_init(void)
{
    rt_clock_timer_t *timer;
    rt_device_t device;
    rt_err_t ret;

    device = rt_device_find(PKG_CANOPENNODE_HIGH_RES_TIMER_NAME);
    if (device == RT_NULL) {
        CO_RTT_LOG_E("high resolution timer not found: dev=%s", PKG_CANOPENNODE_HIGH_RES_TIMER_NAME);
        return -RT_ERROR;
    }
    if (device->type != RT_Device_Class_Timer) {
        CO_RTT_LOG_E("high resolution source is not a timer device: dev=%s type=%d",
                     PKG_CANOPENNODE_HIGH_RES_TIMER_NAME, (int)device->type);
        return -RT_EINVAL;
    }

    ret = rt_device_open(device, RT_DEVICE_OFLAG_RDWR);
    if (ret != RT_EOK) {
        CO_RTT_LOG_E("open high resolution timer failed: dev=%s ret=%d",
                     PKG_CANOPENNODE_HIGH_RES_TIMER_NAME, ret);
        return ret;
    }

    timer = (rt_clock_timer_t *)device;
    if ((timer->ops == RT_NULL) || (timer->info == RT_NULL) || (timer->ops->start == RT_NULL)
        || (timer->ops->stop == RT_NULL) || (timer->ops->count_get == RT_NULL)) {
        CO_RTT_LOG_E("high resolution timer lacks required counter operations: dev=%s",
                     PKG_CANOPENNODE_HIGH_RES_TIMER_NAME);
        (void)rt_device_close(device);
        return -RT_ENOSYS;
    }
    if (timer->freq != (rt_int32_t)CO_RTT_TIME_US_PER_SECOND) {
        CO_RTT_LOG_E("high resolution timer must run at 1 MHz: dev=%s freq=%d",
                     PKG_CANOPENNODE_HIGH_RES_TIMER_NAME, timer->freq);
        (void)rt_device_close(device);
        return -RT_EINVAL;
    }
    if (timer->info->cntmode != CLOCK_TIMER_CNTMODE_UP) {
        CO_RTT_LOG_E("high resolution timer must count upward: dev=%s mode=%u",
                     PKG_CANOPENNODE_HIGH_RES_TIMER_NAME, timer->info->cntmode);
        (void)rt_device_close(device);
        return -RT_EINVAL;
    }

    /*
     * Do not validate timer->info->maxcnt as the physical counter width. Some
     * RT-Thread BSPs, including the current generic STM32 F4 timer metadata,
     * advertise one shared 0xFFFF maxcnt for both 16-bit and 32-bit TIM devices.
     * The configured device must therefore be verified as 32-bit by the user/BSP.
     */

    timer->ops->stop(timer);
    timer->overflow = 0;
    timer->cycles = 1;
    timer->reload = 1;
    timer->mode = CLOCK_TIMER_MODE_PERIOD;

    /*
     * RT-Thread clock-timer start() takes a count duration. UINT32_MAX is the
     * longest representable duration, so a 1 MHz timer wraps after UINT32_MAX us,
     * one count before a modulo-2^32 timestamp would wrap. CO_RTT_timeElapsedUs()
     * compensates this single-count difference across one High-Res wrap.
     */
    ret = timer->ops->start(timer, UINT32_MAX, CLOCK_TIMER_MODE_PERIOD);
    if (ret != RT_EOK) {
        CO_RTT_LOG_E("start high resolution timer failed: dev=%s ret=%d",
                     PKG_CANOPENNODE_HIGH_RES_TIMER_NAME, ret);
        (void)rt_device_close(device);
        return ret;
    }

    co_rtt_time.timerDevice = device;
    co_rtt_time.timer = timer;

    CO_RTT_LOG_I("high resolution timer started: dev=%s freq=%d Hz; 32-bit width is BSP/user responsibility",
                 PKG_CANOPENNODE_HIGH_RES_TIMER_NAME, timer->freq);

    return RT_EOK;
}

/**
 * @brief Read the 1 MHz 32-bit hardware counter directly as microseconds.
 *
 * No software overflow extension is needed. The counter is returned without
 * conversion; callers must use CO_RTT_timeElapsedUs() so the RT-Thread
 * UINT32_MAX-count timer period is handled correctly across a High-Res wrap.
 */
static uint32_t co_rtt_time_high_res_now_us(void)
{
    return co_rtt_time.timer->ops->count_get(co_rtt_time.timer);
}
#endif /* defined(PKG_CANOPENNODE_USING_HIGH_RES_TIME) */

rt_err_t CO_RTT_timeInit(void)
{
#if defined(PKG_CANOPENNODE_USING_HIGH_RES_TIME)
    rt_err_t ret;

    /* High-resolution mode owns one package-wide timer and rejects a second instance. */
    if (co_rtt_time.initialized == RT_TRUE) {
        return -RT_EBUSY;
    }

    memset(&co_rtt_time, 0, sizeof(co_rtt_time));

    ret = co_rtt_time_high_res_init();
    if (ret != RT_EOK) {
        return ret;
    }

    co_rtt_time.initialized = RT_TRUE;
#endif /* defined(PKG_CANOPENNODE_USING_HIGH_RES_TIME) */

    return RT_EOK;
}

void CO_RTT_timeDeinit(void)
{
#if defined(PKG_CANOPENNODE_USING_HIGH_RES_TIME)
    if (co_rtt_time.initialized == RT_FALSE) {
        return;
    }

    co_rtt_time_high_res_deinit();
    memset(&co_rtt_time, 0, sizeof(co_rtt_time));
#endif /* defined(PKG_CANOPENNODE_USING_HIGH_RES_TIME) */
}

uint32_t CO_RTT_timeNowUs(void)
{
#if defined(PKG_CANOPENNODE_USING_HIGH_RES_TIME)
    if (co_rtt_time.timer != RT_NULL) {
        return co_rtt_time_high_res_now_us();
    }
#endif /* defined(PKG_CANOPENNODE_USING_HIGH_RES_TIME) */

    return co_rtt_time_tick_now_us();
}

uint32_t CO_RTT_timeElapsedUs(uint32_t nowUs, uint32_t previousUs)
{
#if defined(PKG_CANOPENNODE_USING_HIGH_RES_TIME)
    if ((co_rtt_time.timer != RT_NULL) && (nowUs < previousUs)) {
        return (UINT32_MAX - previousUs) + nowUs;
    }
#endif /* defined(PKG_CANOPENNODE_USING_HIGH_RES_TIME) */

    return nowUs - previousUs;
}
