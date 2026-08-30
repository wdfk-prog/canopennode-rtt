/**
 * @file CO_time_RTT.h
 * @brief Wrapping microsecond time interface for the CANopenNode RT-Thread wrapper.
 */

#ifndef CO_TIME_RTT_H_
#define CO_TIME_RTT_H_

#include <rtthread.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @brief Initialize the package-wide CANopenNode time source.
 *
 * With PKG_CANOPENNODE_USING_HIGH_RES_TIME disabled, this function owns no
 * exclusive resource and multiple CANopenNodeRTT instances may use the tick
 * backend. When high-resolution time is enabled, the configured RT-Thread clock
 * timer must be dedicated, run at 1 MHz, count upward, and physically provide a
 * 32-bit counter. High-resolution mode supports one CANopenNodeRTT instance only.
 *
 * RT-Thread's generic clock-timer metadata does not reliably expose physical
 * counter width on all BSPs, so this module does not validate that the selected
 * timer is actually 32-bit. Selecting a 16-bit timer may initialize successfully
 * but produces incorrect elapsed-time behavior; timer selection is a BSP/user
 * responsibility.
 *
 * @note CO_RTT_timeInit() and CO_RTT_timeDeinit() are startup/shutdown lifecycle
 *       APIs and must be called serially. Concurrent init/deinit calls are not
 *       supported; the high-resolution initialized state is an ownership guard,
 *       not a synchronization primitive.
 *
 * @return RT_EOK on success, -RT_EBUSY when high-resolution mode is already
 *         owned by another instance, otherwise the RT-Thread error returned while
 *         validating, opening, or starting the configured timer device.
 */
rt_err_t CO_RTT_timeInit(void);

/**
 * @brief Stop and release the package-wide CANopenNode time source.
 *
 * With the tick backend this function is a no-op. With the high-resolution
 * backend it is called after the single owning CANopenNodeRTT instance has
 * stopped using CO_RTT_timeNowUs(); the configured timer is then stopped and
 * closed. This lifecycle operation must be serialized with CO_RTT_timeInit()
 * and with any other teardown path that could release the same time source.
 */
void CO_RTT_timeDeinit(void);

/**
 * @brief Read the current 32-bit wrapping timestamp in microseconds.
 *
 * The high-resolution backend reads its 1 MHz hardware counter directly. RT-Thread
 * starts that timer for UINT32_MAX counts, so its hardware period is UINT32_MAX us,
 * one microsecond shorter than a modulo-2^32 timestamp period. Callers must use
 * CO_RTT_timeElapsedUs() to calculate elapsed time across a High-Res wrap.
 *
 * @return Current boot-local wrapping timestamp in microseconds.
 */
uint32_t CO_RTT_timeNowUs(void);

/**
 * @brief Calculate elapsed microseconds between two wrapping timestamps.
 *
 * The tick backend uses normal uint32_t unsigned subtraction. When High-Res is
 * active, the RT-Thread clock timer wraps after UINT32_MAX counts; this helper
 * compensates the one-count-short hardware period when @p nowUs is below
 * @p previousUs. The real interval must be shorter than one backend wrap period.
 *
 * @param nowUs Current wrapping microsecond timestamp.
 * @param previousUs Previous wrapping microsecond timestamp.
 * @return Elapsed microseconds between the two timestamps.
 */
uint32_t CO_RTT_timeElapsedUs(uint32_t nowUs, uint32_t previousUs);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_TIME_RTT_H_ */
