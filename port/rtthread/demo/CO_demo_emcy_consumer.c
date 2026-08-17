/**
 * @file CO_demo_emcy_consumer.c
 * @brief EMCY consumer diagnostic demo implementation.
 */

#include "CO_demo_emcy_consumer.h"

#include "OD.h"
#include "co_rtt_log.h"

#define CO_DEMO_EMCY_SNAPSHOT_RETRY_MAX 3U

/** Active demo state used by CANopenNode's object-less EMCY receive callback. */
static CO_demo_emcy_consumer_t *co_demo_emcy_consumer_active;

/** Coherent copy published from the atomic receive-side snapshot. */
typedef struct {
    uint32_t remoteRxCount;
    uint8_t lastSourceNodeId;
    uint16_t lastCobId;
    uint16_t lastErrorCode;
    uint8_t lastErrorRegister;
    uint8_t lastErrorBit;
    uint32_t lastInfoCode;
} CO_demo_emcy_consumer_snapshot_t;

/**
 * @brief Capture the most recent remote EMCY message with atomic stores.
 *
 * ident == 0 denotes an EMCY produced by this local node and is deliberately
 * excluded from the remote-consumer diagnostic. The odd/even sequence protects
 * the multi-field snapshot from being published across two different frames.
 */
static void CO_demo_emcy_consumer_rx_callback(const uint16_t ident, const uint16_t errorCode,
                                              const uint8_t errorRegister, const uint8_t errorBit,
                                              const uint32_t infoCode)
{
    CO_demo_emcy_consumer_t *demo = co_demo_emcy_consumer_active;

    if ((demo == NULL) || (ident == 0U)) {
        return;
    }

    (void)rt_atomic_add(&demo->sequence, 1);
    rt_atomic_store(&demo->lastSourceNodeId, (rt_atomic_t)(ident - CO_CAN_ID_EMERGENCY));
    rt_atomic_store(&demo->lastCobId, (rt_atomic_t)ident);
    rt_atomic_store(&demo->lastErrorCode, (rt_atomic_t)errorCode);
    rt_atomic_store(&demo->lastErrorRegister, (rt_atomic_t)errorRegister);
    rt_atomic_store(&demo->lastErrorBit, (rt_atomic_t)errorBit);
    rt_atomic_store(&demo->lastInfoCode, (rt_atomic_t)infoCode);
    (void)rt_atomic_add(&demo->remoteRxCount, 1);
    (void)rt_atomic_add(&demo->sequence, 1);
}

/**
 * @brief Read a stable atomic snapshot without blocking the receive path.
 *
 * @return true when a coherent snapshot was copied, otherwise false.
 */
static bool_t CO_demo_emcy_consumer_snapshot(CO_demo_emcy_consumer_t *demo,
                                             CO_demo_emcy_consumer_snapshot_t *snapshot)
{
    uint8_t attempt;

    for (attempt = 0U; attempt < CO_DEMO_EMCY_SNAPSHOT_RETRY_MAX; attempt++) {
        rt_atomic_t sequenceBefore = rt_atomic_load(&demo->sequence);
        rt_atomic_t sequenceAfter;

        if ((sequenceBefore & 1) != 0) {
            continue;
        }

        snapshot->remoteRxCount = (uint32_t)rt_atomic_load(&demo->remoteRxCount);
        snapshot->lastSourceNodeId = (uint8_t)rt_atomic_load(&demo->lastSourceNodeId);
        snapshot->lastCobId = (uint16_t)rt_atomic_load(&demo->lastCobId);
        snapshot->lastErrorCode = (uint16_t)rt_atomic_load(&demo->lastErrorCode);
        snapshot->lastErrorRegister = (uint8_t)rt_atomic_load(&demo->lastErrorRegister);
        snapshot->lastErrorBit = (uint8_t)rt_atomic_load(&demo->lastErrorBit);
        snapshot->lastInfoCode = (uint32_t)rt_atomic_load(&demo->lastInfoCode);

        sequenceAfter = rt_atomic_load(&demo->sequence);
        if ((sequenceBefore == sequenceAfter) && ((sequenceAfter & 1) == 0)) {
            return true;
        }
    }

    return false;
}

void CO_demo_emcy_consumer_init(CO_demo_emcy_consumer_t *demo)
{
    if (demo == NULL) {
        return;
    }

    rt_atomic_store(&demo->sequence, 0);
    rt_atomic_store(&demo->remoteRxCount, 0);
    rt_atomic_store(&demo->lastSourceNodeId, 0);
    rt_atomic_store(&demo->lastCobId, 0);
    rt_atomic_store(&demo->lastErrorCode, 0);
    rt_atomic_store(&demo->lastErrorRegister, 0);
    rt_atomic_store(&demo->lastErrorBit, 0);
    rt_atomic_store(&demo->lastInfoCode, 0);
}

bool_t CO_demo_emcy_consumer_bind(CO_demo_emcy_consumer_t *demo, CO_t *co)
{
    if ((demo == NULL) || (co == NULL) || (co->em == NULL)) {
        return false;
    }

    /* Bind runs before CAN normal mode starts the RX helper thread. */
    if (co_demo_emcy_consumer_active == NULL) {
        co_demo_emcy_consumer_active = demo;
    } else if (co_demo_emcy_consumer_active != demo) {
        CO_RTT_LOG_E("EMCY consumer diagnostic already bound to another demo instance");
        return false;
    }

    CO_EM_initCallbackRx(co->em, CO_demo_emcy_consumer_rx_callback);
    CO_demo_emcy_consumer_process(demo);
    return true;
}

void CO_demo_emcy_consumer_process(CO_demo_emcy_consumer_t *demo)
{
    CO_demo_emcy_consumer_snapshot_t snapshot;

    if ((demo == NULL) || !CO_demo_emcy_consumer_snapshot(demo, &snapshot)) {
        return;
    }

    OD_RAM.x2301_emcy_consumer_diagnostic.remote_rx_count = snapshot.remoteRxCount;
    OD_RAM.x2301_emcy_consumer_diagnostic.last_source_node_id = snapshot.lastSourceNodeId;
    OD_RAM.x2301_emcy_consumer_diagnostic.last_cob_id = snapshot.lastCobId;
    OD_RAM.x2301_emcy_consumer_diagnostic.last_error_code = snapshot.lastErrorCode;
    OD_RAM.x2301_emcy_consumer_diagnostic.last_error_register = snapshot.lastErrorRegister;
    OD_RAM.x2301_emcy_consumer_diagnostic.last_error_bit = snapshot.lastErrorBit;
    OD_RAM.x2301_emcy_consumer_diagnostic.last_info_code = snapshot.lastInfoCode;
}

void CO_demo_emcy_consumer_reset(CO_demo_emcy_consumer_t *demo)
{
    /* Preserve accumulated diagnostics across communication reset. */
    (void)demo;
}

void CO_demo_emcy_consumer_deinit(CO_demo_emcy_consumer_t *demo)
{
    /* Application cleanup stops CAN RX before releasing this callback owner. */
    if (co_demo_emcy_consumer_active == demo) {
        co_demo_emcy_consumer_active = NULL;
    }
}
