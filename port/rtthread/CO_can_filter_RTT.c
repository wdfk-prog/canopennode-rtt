/**
 * @file CO_can_filter_RTT.c
 * @brief Coarse RT-Thread CAN hardware-filter compiler for CANopenNode RX rules.
 *
 * CANopenNode RX buffers remain the authoritative receive rules. This module
 * compiles them into a bounded set of coarse hardware filters that may accept
 * extra frames, but must never reject a frame required by the software rules.
 * Exact identifier/mask matching therefore remains in the CANopen RX dispatch.
 * DTR and RTR rules are kept separate because the RT-Thread STM32 bxCAN mask
 * path compares the RTR bit in hardware.
 */

#define LOG_TAG                         "canopen.rtt.filter"
#define LOG_LVL                         LOG_LVL_DBG

#include "CO_can_filter_RTT.h"
#include "co_rtt_log.h"

#include <rtatomic.h>
#include <string.h>

#if defined(RT_CAN_USING_HDR) && defined(PKG_CANOPENNODE_USING_RTT_CAN_FILTER)
/** Coarse standard-ID rule programmed into one RT-Thread hardware filter bank. */
typedef struct {
    uint16_t id;       /**< Normalized 11-bit CAN identifier. */
    uint16_t mask;     /**< 11-bit mask; one bits must match and zero bits are don't-care. */
    rt_uint32_t rtr;   /**< RT_CAN_DTR or RT_CAN_RTR; frame types are never merged together. */
} co_rtt_can_ingress_rule_t;

/**
 * @brief Count the identifier bits still constrained by a coarse mask.
 *
 * A higher value means a narrower rule. The merge heuristic uses this value to
 * prefer the result that admits the least additional CAN-ID space.
 *
 * @param mask Standard-ID mask to evaluate.
 * @return Number of set bits inside the 11-bit CAN-ID mask.
 */
static uint8_t co_rtt_filter_specificity(uint16_t mask)
{
    uint8_t bits = 0U;

    mask &= CO_RTT_CAN_STD_MASK;
    while (mask != 0U) {
        bits = (uint8_t)(bits + (uint8_t)(mask & 1U));
        mask >>= 1U;
    }

    return bits;
}

/**
 * @brief Test whether one hardware rule is a superset of another rule.
 *
 * Coverage is only valid for the same frame type. Every bit constrained by the
 * outer rule must also be constrained by the inner rule with the same value.
 *
 * @param outer Candidate superset rule.
 * @param inner Candidate subset rule.
 * @return true if every frame accepted by inner is also accepted by outer.
 */
static bool_t co_rtt_filter_rule_covers(const co_rtt_can_ingress_rule_t *outer,
                                        const co_rtt_can_ingress_rule_t *inner)
{
    uint16_t outer_mask;
    uint16_t inner_mask;

    if (outer->rtr != inner->rtr) {
        return false;
    }

    outer_mask = (uint16_t)(outer->mask & CO_RTT_CAN_STD_MASK);
    inner_mask = (uint16_t)(inner->mask & CO_RTT_CAN_STD_MASK);

    return (((outer_mask & (uint16_t)~inner_mask) == 0U)
            && ((((uint16_t)(outer->id ^ inner->id)) & outer_mask) == 0U));
}

/**
 * @brief Merge two same-type rules into the narrowest mask-rule superset.
 *
 * Identifier bits that differ between the inputs, or that either input already
 * treats as don't-care, become don't-care in the merged rule. Callers ensure
 * both inputs have the same RTR/DTR type before using the result.
 *
 * @param left First rule.
 * @param right Second rule.
 * @return Coarse rule that covers both input rules.
 */
static co_rtt_can_ingress_rule_t co_rtt_filter_merge(const co_rtt_can_ingress_rule_t *left,
                                                       const co_rtt_can_ingress_rule_t *right)
{
    co_rtt_can_ingress_rule_t merged;
    uint16_t differing;

    differing = (uint16_t)((left->id ^ right->id) & CO_RTT_CAN_STD_MASK);
    merged.mask = (uint16_t)(left->mask & right->mask & (uint16_t)~differing & CO_RTT_CAN_STD_MASK);
    merged.id = (uint16_t)(left->id & merged.mask);
    merged.rtr = left->rtr;

    return merged;
}

/**
 * @brief Remove one rule without preserving rule order.
 *
 * The last active rule is moved into the removed slot. Rule ordering has no
 * protocol meaning, so swap-removal avoids shifting the remaining array.
 *
 * @param rules Rule array.
 * @param count In/out number of active entries.
 * @param index Entry to remove.
 */
static void co_rtt_filter_remove_rule(co_rtt_can_ingress_rule_t *rules, uint16_t *count, uint16_t index)
{
    if ((rules == NULL) || (count == NULL) || (index >= *count)) {
        return;
    }

    (*count)--;
    if (index != *count) {
        rules[index] = rules[*count];
    }
}

/**
 * @brief Remove rules that are already covered by another coarse rule.
 *
 * Compaction is applied after replacements and merges so redundant banks can be
 * reused without changing the set of CANopen frames admitted by hardware.
 *
 * @param rules Rule array.
 * @param count In/out number of active entries.
 */
static void co_rtt_filter_compact(co_rtt_can_ingress_rule_t *rules, uint16_t *count)
{
    uint16_t i = 0U;

    while (i < *count) {
        uint16_t j = (uint16_t)(i + 1U);
        bool_t removed_i = false;

        while (j < *count) {
            if (co_rtt_filter_rule_covers(&rules[i], &rules[j])) {
                co_rtt_filter_remove_rule(rules, count, j);
            } else if (co_rtt_filter_rule_covers(&rules[j], &rules[i])) {
                co_rtt_filter_remove_rule(rules, count, i);
                removed_i = true;
                break;
            } else {
                j++;
            }
        }

        if (!removed_i) {
            i++;
        }
    }
}

/**
 * @brief Find the existing rule that produces the narrowest merge with a candidate.
 *
 * Only rules with the same RTR/DTR type are eligible. The most specific merged
 * mask is preferred to minimize unrelated traffic admitted by the hardware.
 *
 * @param rules Existing compiled rules.
 * @param count Number of existing rules.
 * @param candidate New software-derived rule that must be covered.
 * @return Existing rule index, or -1 when no same-type merge target exists.
 */
static int co_rtt_filter_find_best_merge_target(const co_rtt_can_ingress_rule_t *rules, uint16_t count,
                                                 const co_rtt_can_ingress_rule_t *candidate)
{
    int best = -1;
    uint8_t best_specificity = 0U;
    uint16_t i;

    for (i = 0U; i < count; i++) {
        co_rtt_can_ingress_rule_t merged;
        uint8_t specificity;

        if (rules[i].rtr != candidate->rtr) {
            continue;
        }

        merged = co_rtt_filter_merge(&rules[i], candidate);
        specificity = co_rtt_filter_specificity(merged.mask);
        if ((best < 0) || (specificity > best_specificity)) {
            best = (int)i;
            best_specificity = specificity;
        }
    }

    return best;
}

/**
 * @brief Merge the best existing same-type pair to free one hardware-filter slot.
 *
 * This is used when the table is already full and a new candidate cannot merge
 * directly with any existing rule. The pair with the most specific merged mask
 * is selected, then redundant rules are compacted.
 *
 * @param rules Existing compiled rules.
 * @param count In/out number of active entries.
 * @return true if a pair was merged; false when no same-type pair exists.
 */
static bool_t co_rtt_filter_merge_existing_pair(co_rtt_can_ingress_rule_t *rules, uint16_t *count)
{
    int best_left = -1;
    int best_right = -1;
    uint8_t best_specificity = 0U;
    uint16_t i;
    uint16_t j;

    for (i = 0U; i < *count; i++) {
        for (j = (uint16_t)(i + 1U); j < *count; j++) {
            co_rtt_can_ingress_rule_t merged;
            uint8_t specificity;

            if (rules[i].rtr != rules[j].rtr) {
                continue;
            }

            merged = co_rtt_filter_merge(&rules[i], &rules[j]);
            specificity = co_rtt_filter_specificity(merged.mask);
            if ((best_left < 0) || (specificity > best_specificity)) {
                best_left = (int)i;
                best_right = (int)j;
                best_specificity = specificity;
            }
        }
    }

    if (best_left < 0) {
        return false;
    }

    rules[best_left] = co_rtt_filter_merge(&rules[best_left], &rules[best_right]);
    co_rtt_filter_remove_rule(rules, count, (uint16_t)best_right);
    co_rtt_filter_compact(rules, count);
    return true;
}

/**
 * @brief Compile software RX rules into at most @p capacity coarse HDR rules.
 *
 * The caller must hold CANmodule->rxRuleMutex so the RX rule set stays stable
 * during compilation. The resulting hardware rules are supersets of the active
 * CANopenNode rules: extra traffic is allowed, but required traffic is never
 * intentionally filtered out.
 *
 * @param CANmodule CANopenNode CAN module with a stable RX rule array.
 * @param rules Output coarse-rule array.
 * @param capacity Maximum number of coarse rules available.
 * @param rule_count Output number of compiled rules.
 * @return RT_EOK on success, otherwise -RT_ERROR when the bounded table cannot
 *         represent both required frame-type groups without using the fallback.
 */
static rt_err_t co_rtt_compile_ingress_rules_locked(CO_CANmodule_t *CANmodule, co_rtt_can_ingress_rule_t *rules,
                                                      uint16_t capacity, uint16_t *rule_count)
{
    uint16_t i;

    *rule_count = 0U;
    for (i = 0U; i < CANmodule->rxSize; i++) {
        const CO_CANrx_t *rx = &CANmodule->rxArray[i];
        co_rtt_can_ingress_rule_t candidate;
        bool_t covered = false;
        uint16_t j;

        /* RX slots without a callback are not active CANopen receive rules. */
        if (rx->pCANrx_callback == NULL) {
            continue;
        }

        candidate.id = (uint16_t)(rx->ident & CO_RTT_CAN_STD_MASK);
        candidate.mask = (uint16_t)(rx->mask & CO_RTT_CAN_STD_MASK);
        candidate.rtr = ((rx->ident & CO_RTT_CAN_RTR_FLAG) != 0U) ? RT_CAN_RTR : RT_CAN_DTR;

        /* Reuse an existing superset, or replace a narrower rule when the
         * candidate already covers it. This keeps the table compact before any
         * lossy coarse merge is needed. */
        for (j = 0U; j < *rule_count; j++) {
            if (co_rtt_filter_rule_covers(&rules[j], &candidate)) {
                covered = true;
                break;
            }
            if (co_rtt_filter_rule_covers(&candidate, &rules[j])) {
                rules[j] = candidate;
                co_rtt_filter_compact(rules, rule_count);
                covered = true;
                break;
            }
        }
        if (covered) {
            continue;
        }

        if (*rule_count < capacity) {
            rules[*rule_count] = candidate;
            (*rule_count)++;
            continue;
        }

        /* The bank budget is full. Prefer merging the new candidate directly
         * into a same-type rule while retaining as many significant ID bits as
         * possible. */
        {
            int merge_target = co_rtt_filter_find_best_merge_target(rules, *rule_count, &candidate);

            if (merge_target >= 0) {
                rules[merge_target] = co_rtt_filter_merge(&rules[merge_target], &candidate);
                co_rtt_filter_compact(rules, rule_count);
                continue;
            }
        }

        /* If the candidate belongs to a frame-type group not present in the
         * current table, free one slot by merging an existing same-type pair.
         * Failure here means the bounded table needs the broad fallback. */
        if (!co_rtt_filter_merge_existing_pair(rules, rule_count) || (*rule_count >= capacity)) {
            return -RT_ERROR;
        }

        rules[*rule_count] = candidate;
        (*rule_count)++;
    }

    return RT_EOK;
}

/**
 * @brief Build a broad standard-ID fallback for each required frame type.
 *
 * A zero mask accepts every standard CAN-ID for the selected DTR/RTR type. This
 * deliberately sacrifices hardware selectivity so the software dispatcher can
 * still receive every active CANopenNode rule when coarse compilation fails.
 * The caller must hold CANmodule->rxRuleMutex while inspecting rxArray.
 *
 * @param CANmodule CANopenNode CAN module with a stable RX rule array.
 * @param rules Output fallback-rule array.
 * @param capacity Available rule capacity.
 * @param rule_count Output number of fallback rules.
 * @return RT_EOK on success, otherwise -RT_ERROR when the reserved banks cannot
 *         hold all required DTR/RTR fallback groups.
 */
static rt_err_t co_rtt_build_broad_fallback_locked(CO_CANmodule_t *CANmodule, co_rtt_can_ingress_rule_t *rules,
                                                     uint16_t capacity, uint16_t *rule_count)
{
    bool_t need_dtr = false;
    bool_t need_rtr = false;
    uint16_t i;

    for (i = 0U; i < CANmodule->rxSize; i++) {
        const CO_CANrx_t *rx = &CANmodule->rxArray[i];

        if (rx->pCANrx_callback == NULL) {
            continue;
        }
        if ((rx->ident & CO_RTT_CAN_RTR_FLAG) != 0U) {
            need_rtr = true;
        } else {
            need_dtr = true;
        }
    }

    /* Keep one DTR ingress bank when no callback is active yet. This preserves
     * a valid hardware ingress configuration until CANopenNode installs rules. */
    if (!need_dtr && !need_rtr) {
        need_dtr = true;
    }

    *rule_count = 0U;
    if (need_dtr) {
        if (*rule_count >= capacity) {
            return -RT_ERROR;
        }
        rules[*rule_count].id = 0U;
        rules[*rule_count].mask = 0U;
        rules[*rule_count].rtr = RT_CAN_DTR;
        (*rule_count)++;
    }
    if (need_rtr) {
        if (*rule_count >= capacity) {
            return -RT_ERROR;
        }
        rules[*rule_count].id = 0U;
        rules[*rule_count].mask = 0U;
        rules[*rule_count].rtr = RT_CAN_RTR;
        (*rule_count)++;
    }

    return RT_EOK;
}

/**
 * @brief Program the complete reserved HDR bank range with compiled rules.
 *
 * Every reserved bank is written on each refresh. When fewer unique rules exist
 * than reserved banks, rules are repeated intentionally so a driver that does
 * not reliably deactivate omitted banks cannot leave stale filters active.
 *
 * @param CANmodule CANopenNode CAN module.
 * @param rules Compiled or fallback ingress rules.
 * @param rule_count Number of unique rules.
 * @param bank_count Number of reserved hardware banks to overwrite.
 * @return Result returned by RT_CAN_CMD_SET_FILTER, or -RT_EINVAL for invalid input.
 */
static rt_err_t co_rtt_apply_ingress_rules(CO_CANmodule_t *CANmodule, const co_rtt_can_ingress_rule_t *rules,
                                            uint16_t rule_count, uint16_t bank_count)
{
    struct rt_can_filter_item items[PKG_CANOPENNODE_RX_HDR_BANK_COUNT];
    struct rt_can_filter_config config;
    uint16_t i;

    if ((rule_count == 0U) || (bank_count == 0U) || (bank_count > PKG_CANOPENNODE_RX_HDR_BANK_COUNT)) {
        return -RT_EINVAL;
    }

    memset(items, 0, sizeof(items));
    for (i = 0U; i < bank_count; i++) {
        const co_rtt_can_ingress_rule_t *rule = &rules[i % rule_count];

        items[i].id = rule->id;
        items[i].ide = RT_CAN_STDID;
        items[i].rtr = rule->rtr;
        items[i].mode = RT_CAN_MODE_MASK;
        items[i].mask = rule->mask;
        items[i].hdr_bank = (rt_int32_t)(PKG_CANOPENNODE_RX_HDR_BANK_BASE + i);
        items[i].rxfifo = CAN_RX_FIFO0;
    }

    config.count = bank_count;
    config.actived = 1U;
    config.items = items;

    return rt_device_control(CANmodule->dev, RT_CAN_CMD_SET_FILTER, &config);
}

/**
 * @brief Rebuild the bounded coarse HDR ingress set from CANopenNode RX rules.
 *
 * The first dirty check is a lock-free fast path. After rxRuleMutex is acquired,
 * the dirty flag is checked again because another refresher may have completed
 * while this caller was waiting. Compilation, fallback construction, and the
 * hardware update all use the same stable RX-rule snapshot under this mutex.
 *
 * The hardware table always receives the complete reserved bank count. Unused
 * banks are filled with duplicates of compiled rules so stale filters from an
 * older, wider table cannot remain active on RT-Thread drivers that do not
 * implement filter deactivation reliably.
 *
 * @param CANmodule CANopenNode CAN module.
 * @param force Rebuild even when no software RX rule is marked dirty.
 * @return RT_EOK on success, otherwise a negative RT-Thread error code.
 */
rt_err_t co_rtt_can_filter_refresh(CO_CANmodule_t *CANmodule, bool_t force)
{
    co_rtt_can_ingress_rule_t rules[PKG_CANOPENNODE_RX_HDR_BANK_COUNT];
    rt_can_t can = (rt_can_t)CANmodule->dev;
    const rt_uint32_t base = PKG_CANOPENNODE_RX_HDR_BANK_BASE;
    const rt_uint32_t capacity = PKG_CANOPENNODE_RX_HDR_BANK_COUNT;
    uint16_t rule_count = 0U;
    uint16_t i;
    rt_err_t ret;
    bool_t fallback = false;

    if (!force && (rt_atomic_load(&CANmodule->rxFilterDirty) == 0)) {
        return RT_EOK;
    }

    if ((can == RT_NULL) || (can->config.maxhdr == 0U) || (base >= can->config.maxhdr)
        || (capacity > (can->config.maxhdr - base))) {
        CANmodule->useCANrxFilters = false;
        CO_RTT_LOG_E("CANopen RX HDR bank range invalid: base=%lu count=%lu maxhdr=%lu",
                     (unsigned long)base, (unsigned long)capacity,
                     (unsigned long)((can != RT_NULL) ? can->config.maxhdr : 0U));
        return -RT_ERROR;
    }

    if (rt_mutex_take(&CANmodule->rxRuleMutex, RT_WAITING_FOREVER) != RT_EOK) {
        CANmodule->useCANrxFilters = false;
        return -RT_ERROR;
    }

    /* Re-check after taking the mutex. Another refresher may have rebuilt the
     * HDR table and cleared rxFilterDirty while this thread was waiting, so
     * avoid compiling and applying the same filter table twice. */
    if (!force && (rt_atomic_load(&CANmodule->rxFilterDirty) == 0)) {
        (void)rt_mutex_release(&CANmodule->rxRuleMutex);
        return RT_EOK;
    }

    ret = co_rtt_compile_ingress_rules_locked(CANmodule, rules, (uint16_t)capacity, &rule_count);
    if (ret != RT_EOK) {
        fallback = true;
        ret = co_rtt_build_broad_fallback_locked(CANmodule, rules, (uint16_t)capacity, &rule_count);
    }

    if (ret == RT_EOK) {
        ret = co_rtt_apply_ingress_rules(CANmodule, rules, rule_count, (uint16_t)capacity);
    }
    if ((ret != RT_EOK) && !fallback) {
        fallback = true;
        ret = co_rtt_build_broad_fallback_locked(CANmodule, rules, (uint16_t)capacity, &rule_count);
        if (ret == RT_EOK) {
            ret = co_rtt_apply_ingress_rules(CANmodule, rules, rule_count, (uint16_t)capacity);
        }
    }

    if (ret == RT_EOK) {
        rt_atomic_store(&CANmodule->rxFilterDirty, 0);
        CANmodule->useCANrxFilters = true;
    } else {
        CANmodule->useCANrxFilters = false;
    }

    (void)rt_mutex_release(&CANmodule->rxRuleMutex);

    if (ret != RT_EOK) {
        CO_RTT_LOG_E("CANopen RX ingress HDR refresh failed: ret=%ld", (long)ret);
        return ret;
    }

    if (fallback) {
        CO_RTT_LOG_W("CANopen RX ingress uses broad fallback: rules=%u banks=%lu..%lu",
                     rule_count, (unsigned long)base, (unsigned long)(base + capacity - 1U));
    } else {
        CO_RTT_LOG_I("CANopen RX ingress compiled: coarse_rules=%u banks=%lu..%lu",
                     rule_count, (unsigned long)base, (unsigned long)(base + capacity - 1U));
    }

    for (i = 0U; i < rule_count; i++) {
        const uint16_t id_min = (uint16_t)(rules[i].id & rules[i].mask & CO_RTT_CAN_STD_MASK);
        const uint16_t id_max = (uint16_t)(id_min | ((uint16_t)~rules[i].mask & CO_RTT_CAN_STD_MASK));
        const char *frame_type = (rules[i].rtr == RT_CAN_RTR) ? "RTR" : "DTR";

        /* span is the minimum/maximum matching CAN-ID; non-contiguous masks may
         * contain holes inside this span, so id/mask remain the exact rule. */
        CO_RTT_LOG_I("CANopen RX HDR rule[%u]: %s id=0x%03x mask=0x%03x span=0x%03x-0x%03x",
                     i, frame_type, rules[i].id, rules[i].mask, id_min, id_max);
    }

    return RT_EOK;
}
#endif /* defined(RT_CAN_USING_HDR) && defined(PKG_CANOPENNODE_USING_RTT_CAN_FILTER) */
