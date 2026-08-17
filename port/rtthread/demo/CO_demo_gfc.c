/**
 * @file CO_demo_gfc.c
 * @brief GFC consumer diagnostic and producer trigger demo implementation.
 */

#include "CO_demo_gfc.h"

#include "OD.h"

/**
 * @brief Record one accepted Global Fail-safe Command.
 *
 * CANopenNode invokes this callback from the CAN receive path. Keep the body
 * O(1) and non-blocking; real actuator/safety-state control is deliberately not
 * part of this protocol test diagnostic.
 *
 * @param object GFC demo state.
 */
static void CO_demo_gfc_safe_callback(void *object)
{
    CO_demo_gfc_t *demo = (CO_demo_gfc_t *)object;

    if (demo == NULL) {
        return;
    }

    (void)rt_atomic_add(&demo->rxCount, 1);
    rt_atomic_store(&demo->safeRequested, 1);
}

/** Publish current long-lived state into the test-only OD record. */
static void CO_demo_gfc_publish(CO_demo_gfc_t *demo)
{
    OD_RAM.x2302_gfc_diagnostic.rx_count =
        (uint32_t)rt_atomic_load(&demo->rxCount);
    OD_RAM.x2302_gfc_diagnostic.safe_requested =
        (uint8_t)rt_atomic_load(&demo->safeRequested);
    OD_RAM.x2302_gfc_diagnostic.producer_complete_seq = demo->producerCompleteSeq;
    OD_RAM.x2302_gfc_diagnostic.producer_result = demo->producerResult;
}

void CO_demo_gfc_init(CO_demo_gfc_t *demo)
{
    if (demo == NULL) {
        return;
    }

    rt_atomic_store(&demo->rxCount, 0);
    rt_atomic_store(&demo->safeRequested, 0);
    demo->producerCompleteSeq = OD_RAM.x2302_gfc_diagnostic.producer_request_seq;
    demo->producerResult = (int32_t)CO_ERROR_NO;
    CO_demo_gfc_publish(demo);
}

bool_t CO_demo_gfc_bind(CO_demo_gfc_t *demo, CO_t *co)
{
    if ((demo == NULL) || (co == NULL) || (co->GFC == NULL)) {
        return false;
    }

    /* Suppress stale producer requests across communication reset before the
     * newly created GFC object can enter normal CAN mode. */
    demo->producerCompleteSeq = OD_RAM.x2302_gfc_diagnostic.producer_request_seq;
    CO_GFC_initCallbackEnterSafeState(co->GFC, demo, CO_demo_gfc_safe_callback);
    CO_demo_gfc_publish(demo);
    return true;
}

void CO_demo_gfc_process(CO_demo_gfc_t *demo, CO_t *co)
{
    uint32_t requestSeq;

    if ((demo == NULL) || (co == NULL) || (co->GFC == NULL)) {
        return;
    }

    requestSeq = OD_RAM.x2302_gfc_diagnostic.producer_request_seq;
    if (requestSeq != demo->producerCompleteSeq) {
        demo->producerResult = (int32_t)CO_GFCsend(co->GFC);
        demo->producerCompleteSeq = requestSeq;
    }

    CO_demo_gfc_publish(demo);
}

void CO_demo_gfc_reset(CO_demo_gfc_t *demo)
{
    if (demo == NULL) {
        return;
    }

    /* Keep consumer evidence, but consume any request that belonged to the old
     * communication stack so it cannot be transmitted after rebind. */
    demo->producerCompleteSeq = OD_RAM.x2302_gfc_diagnostic.producer_request_seq;
    CO_demo_gfc_publish(demo);
}
