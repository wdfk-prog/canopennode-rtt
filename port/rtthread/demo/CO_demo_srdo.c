/**
 * @file CO_demo_srdo.c
 * @brief Test-only SRDO diagnostic bridge implementation.
 */

#include "CO_demo_srdo.h"

#include "OD.h"

#include <stdint.h>

#if (((CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE) == 0)
#error "CO_demo_srdo.c requires CO_CONFIG_SRDO_ENABLE"
#endif

#if (OD_CNT_SRDO < 2)
#error "CO_demo_srdo.c requires two SRDO objects"
#endif

#if (PKG_CANOPENNODE_CAN_BINDING_COUNT != 1)
#error "SRDO diagnostic requires PKG_CANOPENNODE_CAN_BINDING_COUNT=1"
#endif

/* CANopenNode's pinned SRDO implementation stores information direction as
 * invalid=0, TX=1 and RX=2; this revision does not export symbolic TX/RX names. */
#define CO_DEMO_SRDO_DIRECTION_TX        1U
#define CO_DEMO_SRDO_DIRECTION_RX        2U
#define CO_DEMO_SRDO_TX_NORMAL_DEFAULT    0x12345678UL
#define CO_DEMO_SRDO_TX_INVERTED_DEFAULT  0xEDCBA987UL
#define CO_DEMO_SRDO_REQUEST_RESULT_IDLE   INT32_MIN

#define CO_DEMO_SRDO_DIAG OD_RAM.x2306_srdo_diagnostic

static int8_t CO_demo_srdo_to_i8(CO_SRDO_state_t state)
{
    return (int8_t)state;
}

static int8_t CO_demo_srdo_aggregate(int8_t rxState, int8_t txState)
{
    return (rxState < txState) ? rxState : txState;
}

static void CO_demo_srdo_restore_tx_baseline(void)
{
    /* The manufacturer object is a test fixture, not product application data.
     * Resetting its deliberately corrupt F11 values prevents one fault case from
     * contaminating the next freshly re-created SRDO instance. */
    CO_DEMO_SRDO_DIAG.tx_normal = CO_DEMO_SRDO_TX_NORMAL_DEFAULT;
    CO_DEMO_SRDO_DIAG.tx_inverted = CO_DEMO_SRDO_TX_INVERTED_DEFAULT;
}

void CO_demo_srdo_init(void)
{
    CO_demo_srdo_restore_tx_baseline();
    CO_DEMO_SRDO_DIAG.aggregate_state = CO_demo_srdo_to_i8(CO_SRDO_state_unknown);
    CO_DEMO_SRDO_DIAG.rx_state = CO_demo_srdo_to_i8(CO_SRDO_state_unknown);
    CO_DEMO_SRDO_DIAG.tx_state = CO_demo_srdo_to_i8(CO_SRDO_state_unknown);
    CO_DEMO_SRDO_DIAG.state_seq = 0U;
    CO_DEMO_SRDO_DIAG.tx_request_seq = 0U;
    CO_DEMO_SRDO_DIAG.tx_complete_seq = 0U;
    /* INT32_MIN means no diagnostic TX request has been processed by this
     * freshly initialized/bound fixture generation. Natural cyclic TX never
     * changes this witness. */
    CO_DEMO_SRDO_DIAG.tx_request_result = CO_DEMO_SRDO_REQUEST_RESULT_IDLE;
}

bool_t CO_demo_srdo_bind(CO_t *co)
{
    if (co == NULL) {
        return false;
    }

    /* Initial bind runs before the realtime timer starts. Communication-reset
     * bind runs after that timer is stopped and while lifecycleMutex protects
     * the new CO_t. Restore mapped TX values and publish a new-generation
     * request witness only on this quiesced path. */
    CO_demo_srdo_restore_tx_baseline();
    CO_DEMO_SRDO_DIAG.tx_complete_seq = CO_DEMO_SRDO_DIAG.tx_request_seq;
    CO_DEMO_SRDO_DIAG.tx_request_result = CO_DEMO_SRDO_REQUEST_RESULT_IDLE;

    /* CO_CANopenInitSRDO() returns NODE_ID_UNCONFIGURED_LSS before SRDO
     * directions are initialized. Keep LSS commissioning available. */
    if (co->nodeIdUnconfigured) {
        return true;
    }
    if (co->SRDO == NULL) {
        return false;
    }
    if ((co->SRDO[0].informationDirection != CO_DEMO_SRDO_DIRECTION_RX)
        || (co->SRDO[1].informationDirection != CO_DEMO_SRDO_DIRECTION_TX)) {
        return false;
    }

    return true;
}

void CO_demo_srdo_process(CO_t *co)
{
    int8_t rxState;
    int8_t txState;
    int8_t aggregateState;

    if ((co == NULL) || co->nodeIdUnconfigured || (co->CANmodule == NULL) || (co->SRDO == NULL)) {
        return;
    }

    /* Realtime CO_process_SRDO() runs under the same OD lock in CO_app_RTT.c.
     * Keep this mainline bridge inside one short critical section so the three
     * published states and a possible event-send request refer to one coherent
     * SRDO instance/state. No SDO, logging, allocation or wait occurs here. */
    CO_LOCK_OD(co->CANmodule);

    rxState = CO_demo_srdo_to_i8(co->SRDO[0].internalState);
    txState = CO_demo_srdo_to_i8(co->SRDO[1].internalState);
    aggregateState = CO_demo_srdo_aggregate(rxState, txState);

    if ((CO_DEMO_SRDO_DIAG.rx_state != rxState) || (CO_DEMO_SRDO_DIAG.tx_state != txState)
        || (CO_DEMO_SRDO_DIAG.aggregate_state != aggregateState)) {
        CO_DEMO_SRDO_DIAG.rx_state = rxState;
        CO_DEMO_SRDO_DIAG.tx_state = txState;
        CO_DEMO_SRDO_DIAG.aggregate_state = aggregateState;
        CO_DEMO_SRDO_DIAG.state_seq++;
    }

    if (CO_DEMO_SRDO_DIAG.tx_request_seq != CO_DEMO_SRDO_DIAG.tx_complete_seq) {
        const uint32_t requestSeq = CO_DEMO_SRDO_DIAG.tx_request_seq;
        CO_ReturnError_t result = CO_SRDO_requestSend(&co->SRDO[1]);

        CO_DEMO_SRDO_DIAG.tx_request_result = (int32_t)result;
        CO_DEMO_SRDO_DIAG.tx_complete_seq = requestSeq;
    }

    CO_UNLOCK_OD(co->CANmodule);
}

void CO_demo_srdo_reset(void)
{
    CO_DEMO_SRDO_DIAG.aggregate_state = CO_demo_srdo_to_i8(CO_SRDO_state_unknown);
    CO_DEMO_SRDO_DIAG.rx_state = CO_demo_srdo_to_i8(CO_SRDO_state_unknown);
    CO_DEMO_SRDO_DIAG.tx_state = CO_demo_srdo_to_i8(CO_SRDO_state_unknown);
    CO_DEMO_SRDO_DIAG.state_seq++;
    CO_DEMO_SRDO_DIAG.tx_complete_seq = CO_DEMO_SRDO_DIAG.tx_request_seq;
    /* Re-bind after the quiesced communication reset publishes the same value;
     * keeping the reset path at the sentinel makes stale-request replay visible
     * throughout the reset generation transition. */
    CO_DEMO_SRDO_DIAG.tx_request_result = CO_DEMO_SRDO_REQUEST_RESULT_IDLE;
}
