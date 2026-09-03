/**
 * @file CO_402_log_RTT.h
 * @brief Optional RT-Thread ulog adapter for the Pure-C CiA 402 log hooks.
 */

#ifndef CO_402_LOG_RTT_H
#define CO_402_LOG_RTT_H

#include "co_rtt_log.h"

#ifndef CO_402_LOG_E
#define CO_402_LOG_E(...) CO_RTT_LOG_E(__VA_ARGS__)
#endif

#ifndef CO_402_LOG_W
#define CO_402_LOG_W(...) CO_RTT_LOG_W(__VA_ARGS__)
#endif

#ifndef CO_402_LOG_I
#define CO_402_LOG_I(...) CO_RTT_LOG_I(__VA_ARGS__)
#endif

#ifndef CO_402_LOG_D
#define CO_402_LOG_D(...) CO_RTT_LOG_D(__VA_ARGS__)
#endif

#endif /* CO_402_LOG_RTT_H */
