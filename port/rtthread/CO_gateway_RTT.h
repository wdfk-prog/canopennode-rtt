/**
 * @file CO_gateway_RTT.h
 * @brief RT-Thread MSH frontend for the CANopenNode CiA 309-3 ASCII Gateway.
 */

#ifndef CO_GATEWAY_RTT_H_
#define CO_GATEWAY_RTT_H_

#include "CO_app_RTT.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if defined(PKG_CANOPENNODE_GATEWAY_RTT_CONSOLE)
/**
 * @brief Bind the MSH Gateway frontend to an initialized CANopenNode RT-Thread application.
 *
 * The function acquires @p app lifecycle protection, initializes the frontend
 * sequence at 1, and registers the Gateway read callback on the current stack.
 *
 * @param app Initialized CANopenNode RT-Thread application instance.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
rt_err_t CO_gateway_RTT_init(CANopenNodeRTT *app);

/**
 * @brief Re-register the Gateway read callback after Communication Reset.
 *
 * This function is called by the application reset path while the current
 * CANopenNode object has stable lifetime. It has no effect for an application
 * that is not bound to the MSH frontend.
 *
 * @param app CANopenNode RT-Thread application instance owning the new stack.
 */
void CO_gateway_RTT_rebind(CANopenNodeRTT *app);
#endif /* defined(PKG_CANOPENNODE_GATEWAY_RTT_CONSOLE) */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_GATEWAY_RTT_H_ */
