/**
 * @file CO_demo_time.c
 * @brief TIME consumer diagnostic demo implementation.
 */

#include "CO_demo_time.h"

#include "OD.h"

/**
 * @brief Count one syntactically valid TIME reception.
 *
 * CANopenNode invokes callback-pre only after CO_TIME_receive() accepts an exact
 * DLC=6 TIME frame. Keep this callback O(1) because it executes in the CAN
 * receive dispatch path.
 *
 * @param object TIME diagnostic runtime state.
 */
static void CO_demo_time_rx_callback(void *object)
{
    CO_demo_time_t *demo = (CO_demo_time_t *)object;

    if (demo != NULL) {
        (void)rt_atomic_add(&demo->rxCount, 1);
    }
}

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

    CO_TIME_initCallbackPre(co->TIME, demo, CO_demo_time_rx_callback);
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
