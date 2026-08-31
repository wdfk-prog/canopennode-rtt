/**
 * @file CO_gateway_RTT.c
 * @brief RT-Thread MSH frontend for the CANopenNode CiA 309-3 ASCII Gateway.
 */

#include "CO_gateway_RTT.h"

#if defined(PKG_CANOPENNODE_GATEWAY_RTT_CONSOLE)

#include <finsh.h>
#include <stdint.h>
#include <string.h>

#if !defined(RT_CONSOLEBUF_SIZE) || (RT_CONSOLEBUF_SIZE < 3)
#error "RT_CONSOLEBUF_SIZE must be at least 3 for CANopen Gateway console output"
#endif /* !defined(RT_CONSOLEBUF_SIZE) || (RT_CONSOLEBUF_SIZE < 3) */

/** MSH frontend state for the single default auto-initialized CANopenNode instance. */
typedef struct {
    CANopenNodeRTT *app; /**< Application whose current Gateway object receives MSH commands. */
    uint32_t sequence;   /**< Sequence prepended to the next command; starts at 1 after bind. */
    char commandBuf[CO_CONFIG_GTWA_COMM_BUF_SIZE + 1U]; /**< Complete command staged before FIFO write. */
} CO_gateway_RTT_t;

/** Singleton MSH Gateway frontend state bound to the default auto-initialized application. */
static CO_gateway_RTT_t co_gateway_rtt;

/**
 * @brief Print one Gateway response segment to the active RT-Thread console.
 *
 * @param object Unused callback object.
 * @param buf Gateway response bytes.
 * @param count Number of response bytes available in @p buf.
 * @param connectionOK Optional connection status output.
 * @return @p count because the console frontend consumes the whole segment.
 */
static size_t co_gateway_rtt_read_callback(void *object, const char *buf, size_t count, uint8_t *connectionOK)
{
    const char *current = buf;
    size_t remaining = count;

    (void)object;

    if (connectionOK != NULL) {
        *connectionOK = 1U;
    }

    while ((current != NULL) && (remaining > 0U)) {
        size_t chunk = remaining;

        /* rt_kprintf passes RT_CONSOLEBUF_SIZE - 1 to rt_vsnprintf, leaving one more byte for its NUL. */
        if (chunk > (RT_CONSOLEBUF_SIZE - 2U)) {
            chunk = RT_CONSOLEBUF_SIZE - 2U;
        }

        rt_kprintf("%.*s", (int)chunk, current);
        current += chunk;
        remaining -= chunk;
    }

    return count;
}

/**
 * @brief Register the response callback on the application's current Gateway object.
 *
 * @param app CANopenNode RT-Thread application instance with stable stack lifetime.
 */
static void co_gateway_rtt_init_read(CANopenNodeRTT *app)
{
    CO_t *co;

    if (app == NULL) {
        return;
    }

    co = app->canOpenStack;
    if ((co != NULL) && (co->gtwa != NULL)) {
        CO_GTWA_initRead(co->gtwa, co_gateway_rtt_read_callback, NULL);
    }
}

/**
 * @brief Build one complete CiA 309-3 command line from MSH argv tokens.
 *
 * @param sequence Sequence number to prepend.
 * @param argc MSH argument count.
 * @param argv MSH argument vector.
 * @param buf Destination buffer including room for the terminating NUL.
 * @param bufSize Destination buffer size.
 * @return Command byte count excluding NUL, or 0 if the formatted command does not fit.
 */
static size_t co_gateway_rtt_build_command(uint32_t sequence, int argc, char **argv, char *buf, size_t bufSize)
{
    size_t offset;
    int prefixLen;
    int i;

    if ((argc < 2) || (argv == NULL) || (buf == NULL) || (bufSize < 4U)) {
        return 0U;
    }

    prefixLen = rt_snprintf(buf, bufSize, "[%lu]", (unsigned long)sequence);
    if ((prefixLen <= 0) || ((size_t)prefixLen >= bufSize)) {
        return 0U;
    }
    offset = (size_t)prefixLen;

    for (i = 1; i < argc; i++) {
        size_t argLen;

        if (argv[i] == NULL) {
            return 0U;
        }
        argLen = strlen(argv[i]);
        if ((offset + 1U + argLen + 2U) >= bufSize) {
            return 0U;
        }

        buf[offset++] = ' ';
        if (argLen > 0U) {
            memcpy(&buf[offset], argv[i], argLen);
            offset += argLen;
        }
    }

    buf[offset++] = '\r';
    buf[offset++] = '\n';
    buf[offset] = '\0';

    return offset;
}

/**
 * @brief Advance the frontend sequence after one complete command is queued.
 */
static void co_gateway_rtt_advance_sequence(void)
{
    if (co_gateway_rtt.sequence == UINT32_MAX) {
        co_gateway_rtt.sequence = 1U;
    } else {
        co_gateway_rtt.sequence++;
    }
}

/**
 * @brief Submit one MSH command to the CANopenNode ASCII Gateway.
 *
 * @param argc MSH argument count.
 * @param argv MSH argument vector; argv[0] is "canopen_gw".
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
static int canopen_gw(int argc, char **argv)
{
    CANopenNodeRTT *app = co_gateway_rtt.app;
    char *command = co_gateway_rtt.commandBuf;
    size_t commandLen;
    size_t written;
    CO_t *co;
    rt_err_t ret;
#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
    rt_bool_t wakeMainline = RT_FALSE;
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */

    if (argc < 2) {
        rt_kprintf("usage: canopen_gw <CiA 309-3 command>\n");
        return -RT_EINVAL;
    }
    if (app == NULL) {
        rt_kprintf("canopen_gw: Gateway frontend is not initialized\n");
        return -RT_ERROR;
    }

    ret = rt_mutex_take(&app->lifecycleMutex, RT_WAITING_FOREVER);
    if (ret != RT_EOK) {
        rt_kprintf("canopen_gw: failed to acquire CANopen lifecycle lock (%d)\n", ret);
        return ret;
    }

    co = app->canOpenStack;
    if ((co == NULL) || (co->gtwa == NULL) || (co->CANmodule == NULL)) {
        ret = -RT_ERROR;
        rt_kprintf("canopen_gw: Gateway is unavailable\n");
        goto out;
    }

    commandLen = co_gateway_rtt_build_command(co_gateway_rtt.sequence, argc, argv, command, sizeof(co_gateway_rtt.commandBuf));
    if (commandLen == 0U) {
        ret = -RT_EFULL;
        rt_kprintf("canopen_gw: formatted command is too long\n");
        goto out;
    }

    if (CO_GTWA_write_getSpace(co->gtwa) < commandLen) {
        ret = -RT_EFULL;
        rt_kprintf("canopen_gw: Gateway input buffer is full\n");
        goto out;
    }

    written = CO_GTWA_write(co->gtwa, command, commandLen);
    if (written != commandLen) {
        ret = -RT_ERROR;
        rt_kprintf("canopen_gw: failed to queue complete command\n");
        goto out;
    }

    co_gateway_rtt_advance_sequence();
#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
    wakeMainline = RT_TRUE;
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */
    ret = RT_EOK;

out:
    (void)rt_mutex_release(&app->lifecycleMutex);
#if defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
    /* The wake is only a scheduling hint; publish it after releasing the stack lifetime lock. */
    if (wakeMainline == RT_TRUE) {
        CO_RTT_mainlineWakeup(app);
    }
#endif /* defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */
    return ret;
}
MSH_CMD_EXPORT(canopen_gw, send a CiA 309-3 command through CANopenNode Gateway ASCII);

rt_err_t CO_gateway_RTT_init(CANopenNodeRTT *app)
{
    rt_err_t ret;

    if (app == NULL) {
        return -RT_EINVAL;
    }

    ret = rt_mutex_take(&app->lifecycleMutex, RT_WAITING_FOREVER);
    if (ret != RT_EOK) {
        return ret;
    }

    co_gateway_rtt.sequence = 1U;
    co_gateway_rtt_init_read(app);
    co_gateway_rtt.app = app;

    (void)rt_mutex_release(&app->lifecycleMutex);
    return RT_EOK;
}

void CO_gateway_RTT_rebind(CANopenNodeRTT *app)
{
    if ((app != NULL) && (app == co_gateway_rtt.app)) {
        co_gateway_rtt_init_read(app);
    }
}

#endif /* defined(PKG_CANOPENNODE_GATEWAY_RTT_CONSOLE) */
