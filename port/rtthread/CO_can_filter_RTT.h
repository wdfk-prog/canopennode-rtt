/**
 * @file CO_can_filter_RTT.h
 * @brief Internal coarse CAN RX ingress filter interface for the RT-Thread port.
 */

#ifndef CO_CAN_FILTER_RTT_H_
#define CO_CAN_FILTER_RTT_H_

#include "CO_driver.h"

#if defined(RT_CAN_USING_HDR) && defined(PKG_CANOPENNODE_USING_RTT_CAN_FILTER)

/**
 * @brief Rebuild coarse RT-Thread HDR ingress filters from CANopenNode RX rules.
 *
 * @param CANmodule CANopenNode CAN module.
 * @param force Rebuild even when no software RX rule is marked dirty.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
rt_err_t co_rtt_can_filter_refresh(CO_CANmodule_t *CANmodule, bool_t force);

#endif /* defined(RT_CAN_USING_HDR) && defined(PKG_CANOPENNODE_USING_RTT_CAN_FILTER) */

#endif /* CO_CAN_FILTER_RTT_H_ */
