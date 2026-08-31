/**
 * @file CO_demo_time.c
 * @brief TIME consumer diagnostic demo implementation.
 */

#include "CO_demo_time.h"

#include "OD.h"

void CO_demo_time_on_receive(CO_demo_time_t *demo)
{
    if (demo != NULL) {
        (void)rt_atomic_add(&demo->rxCount, 1);
    }
}

#if !defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
/** Forward a valid TIME reception to the diagnostic state in legacy polling mode. */
static void CO_demo_time_rx_callback(void *object)
{
    CO_demo_time_on_receive((CO_demo_time_t *)object);
}
#endif /* !defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */

void CO_demo_time_init(CO_demo_time_t *demo)
{
    if (demo == NULL) {
        return;
    }

    rt_atomic_store(&demo->rxCount, 0);
}

void CO_demo_time_bind(CO_demo_time_t *demo, CO_t *co)
{
    if ((demo == NULL) || (co == NULL) || (co->TIME == NULL)) {
        return;
    }

#if !defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT)
    CO_TIME_initCallbackPre(co->TIME, demo, CO_demo_time_rx_callback);
#endif /* !defined(PKG_CANOPENNODE_GLOBAL_TIMERNEXT) */
    CO_demo_time_process(demo, co);
}

void CO_demo_time_process(CO_demo_time_t *demo, const CO_t *co)
{
    if ((demo == NULL) || (co == NULL) || (co->TIME == NULL)) {
        return;
    }

    OD_RAM.x2300_time_consumer_diagnostic.valid_time_rx_count =
        (uint32_t)rt_atomic_load(&demo->rxCount);
    OD_RAM.x2300_time_consumer_diagnostic.applied_time_ms = co->TIME->ms;
    OD_RAM.x2300_time_consumer_diagnostic.applied_time_days = co->TIME->days;
}

void CO_demo_time_reset(CO_demo_time_t *demo)
{
    /* Preserve the accumulated receive count across communication reset. */
    (void)demo;
}
