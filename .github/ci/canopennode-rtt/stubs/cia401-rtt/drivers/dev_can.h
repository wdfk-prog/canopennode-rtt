#ifndef CIA401_RTT_HOST_STUB_DEV_CAN_H
#define CIA401_RTT_HOST_STUB_DEV_CAN_H

#include <rtthread.h>

#define RT_CAN_STDID 0U
#define RT_CAN_DTR 0U
#define RT_CAN_RTR 1U
#define RT_CAN_MODE_NORMAL 0U
#define RT_CAN_CMD_SET_BAUD 1
#define RT_CAN_CMD_SET_MODE 2
#define RT_CAN_CMD_START 3
#define RT_CAN_CMD_GET_STATUS 4

struct rt_can_msg {
    rt_uint32_t id;
    rt_uint32_t ide;
    rt_uint32_t rtr;
    rt_uint32_t len;
    rt_uint32_t priv;
    uint8_t data[8];
};

struct rt_can_status {
    rt_uint32_t rcverrcnt;
    rt_uint32_t snderrcnt;
    rt_uint32_t dropedrcvpkg;
    rt_uint32_t dropedsndpkg;
};

#endif /* CIA401_RTT_HOST_STUB_DEV_CAN_H */
