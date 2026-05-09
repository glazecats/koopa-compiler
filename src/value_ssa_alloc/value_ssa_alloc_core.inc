#if defined(__INTELLISENSE__) && !defined(VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT) && \
    !defined(VALUE_SSA_ALLOC_SPLIT_AGGREGATOR)
#define VALUE_SSA_ALLOC_SPLIT_INTELLISENSE_CONTEXT
#include "value_ssa_alloc.c"
#else

void value_ssa_allocation_result_init(ValueSsaAllocationResult *result) {
    if (!result) {
        return;
    }

    result->function_name = NULL;
    result->value_count = 0;
    result->color_budget = 0;
    result->spill_slot_count = 0;
    result->colors = NULL;
    result->has_color = NULL;
    result->spill_flags = NULL;
    result->spill_intended_flags = NULL;
    result->spill_confirmed_flags = NULL;
    result->spill_recovered_flags = NULL;
    result->spill_recovery_orders = NULL;
    result->spill_recovery_phase_ids = NULL;
    result->spill_recovery_phase_member_orders = NULL;
    result->spill_recovery_phase_member_counts = NULL;
    result->spill_recovery_phase_entries = NULL;
    result->next_spill_recovery_order = 0;
    result->next_spill_recovery_phase_id = 0;
    result->spill_priorities = NULL;
    result->spill_slots = NULL;
    result->used_preferred_color_flags = NULL;
    result->preferred_color_sources = NULL;
    result->preferred_color_partner_value_ids = NULL;
    result->used_generic_eviction_flags = NULL;
    result->generic_eviction_blocker_value_ids = NULL;
    result->generic_eviction_blocker_recolored_flags = NULL;
    result->generic_eviction_blocker_new_colors = NULL;
    result->used_targeted_eviction_flags = NULL;
    result->targeted_eviction_blocker_value_ids = NULL;
    result->targeted_eviction_blocker_recolored_flags = NULL;
    result->targeted_eviction_blocker_new_colors = NULL;
    result->used_retry_eviction_flags = NULL;
    result->retry_eviction_blocker_value_ids = NULL;
    result->retry_eviction_blocker_recolored_flags = NULL;
    result->retry_eviction_blocker_new_colors = NULL;
}

void value_ssa_allocation_result_free(ValueSsaAllocationResult *result) {
    if (!result) {
        return;
    }

    free(result->function_name);
    free(result->colors);
    free(result->has_color);
    free(result->spill_flags);
    free(result->spill_intended_flags);
    free(result->spill_confirmed_flags);
    free(result->spill_recovered_flags);
    free(result->spill_recovery_orders);
    free(result->spill_recovery_phase_ids);
    free(result->spill_recovery_phase_member_orders);
    free(result->spill_recovery_phase_member_counts);
    free(result->spill_recovery_phase_entries);
    free(result->spill_priorities);
    free(result->spill_slots);
    free(result->used_preferred_color_flags);
    free(result->preferred_color_sources);
    free(result->preferred_color_partner_value_ids);
    free(result->used_generic_eviction_flags);
    free(result->generic_eviction_blocker_value_ids);
    free(result->generic_eviction_blocker_recolored_flags);
    free(result->generic_eviction_blocker_new_colors);
    free(result->used_targeted_eviction_flags);
    free(result->targeted_eviction_blocker_value_ids);
    free(result->targeted_eviction_blocker_recolored_flags);
    free(result->targeted_eviction_blocker_new_colors);
    free(result->used_retry_eviction_flags);
    free(result->retry_eviction_blocker_value_ids);
    free(result->retry_eviction_blocker_recolored_flags);
    free(result->retry_eviction_blocker_new_colors);
    value_ssa_allocation_result_init(result);
}

void value_ssa_program_allocation_result_init(ValueSsaProgramAllocationResult *result) {
    if (!result) {
        return;
    }

    result->function_count = 0;
    result->function_results = NULL;
}

static int value_ssa_clone_allocation_result(const ValueSsaAllocationResult *src,
    ValueSsaAllocationResult *dst,
    ValueSsaError *error) {
    size_t alloc_count;

    if (!src || !dst) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-234: invalid allocation-result clone contract");
        return 0;
    }

    value_ssa_allocation_result_free(dst);
    *dst = *src;
    dst->function_name = NULL;
    dst->colors = NULL;
    dst->has_color = NULL;
    dst->spill_flags = NULL;
    dst->spill_intended_flags = NULL;
    dst->spill_confirmed_flags = NULL;
    dst->spill_recovered_flags = NULL;
    dst->spill_recovery_orders = NULL;
    dst->spill_recovery_phase_ids = NULL;
    dst->spill_recovery_phase_member_orders = NULL;
    dst->spill_recovery_phase_member_counts = NULL;
    dst->spill_recovery_phase_entries = NULL;
    dst->spill_priorities = NULL;
    dst->spill_slots = NULL;
    dst->used_preferred_color_flags = NULL;
    dst->preferred_color_sources = NULL;
    dst->preferred_color_partner_value_ids = NULL;
    dst->used_generic_eviction_flags = NULL;
    dst->generic_eviction_blocker_value_ids = NULL;
    dst->generic_eviction_blocker_recolored_flags = NULL;
    dst->generic_eviction_blocker_new_colors = NULL;
    dst->used_targeted_eviction_flags = NULL;
    dst->targeted_eviction_blocker_value_ids = NULL;
    dst->targeted_eviction_blocker_recolored_flags = NULL;
    dst->targeted_eviction_blocker_new_colors = NULL;
    dst->used_retry_eviction_flags = NULL;
    dst->retry_eviction_blocker_value_ids = NULL;
    dst->retry_eviction_blocker_recolored_flags = NULL;
    dst->retry_eviction_blocker_new_colors = NULL;

    if (src->function_name) {
        dst->function_name = value_ssa_alloc_strdup(src->function_name);
        if (!dst->function_name) {
            value_ssa_allocation_result_free(dst);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-235: out of memory cloning allocation result");
            return 0;
        }
    }

    alloc_count = src->value_count ? src->value_count : 1u;
    if (src->colors) {
        dst->colors = (size_t *)malloc(alloc_count * sizeof(size_t));
    }
    if (src->has_color) {
        dst->has_color = (unsigned char *)malloc(alloc_count * sizeof(unsigned char));
    }
    if (src->spill_flags) {
        dst->spill_flags = (unsigned char *)malloc(alloc_count * sizeof(unsigned char));
    }
    if (src->spill_intended_flags) {
        dst->spill_intended_flags = (unsigned char *)malloc(alloc_count * sizeof(unsigned char));
    }
    if (src->spill_confirmed_flags) {
        dst->spill_confirmed_flags = (unsigned char *)malloc(alloc_count * sizeof(unsigned char));
    }
    if (src->spill_recovered_flags) {
        dst->spill_recovered_flags = (unsigned char *)malloc(alloc_count * sizeof(unsigned char));
    }
    if (src->spill_recovery_orders) {
        dst->spill_recovery_orders = (size_t *)malloc(alloc_count * sizeof(size_t));
    }
    if (src->spill_recovery_phase_ids) {
        dst->spill_recovery_phase_ids = (size_t *)malloc(alloc_count * sizeof(size_t));
    }
    if (src->spill_recovery_phase_member_orders) {
        dst->spill_recovery_phase_member_orders = (size_t *)malloc(alloc_count * sizeof(size_t));
    }
    if (src->spill_recovery_phase_member_counts) {
        dst->spill_recovery_phase_member_counts = (size_t *)malloc(alloc_count * sizeof(size_t));
    }
    if (src->spill_recovery_phase_entries) {
        dst->spill_recovery_phase_entries =
            (ValueSsaAllocatorRetryFamilyAgendaItem *)malloc(alloc_count * sizeof(ValueSsaAllocatorRetryFamilyAgendaItem));
    }
    if (src->spill_priorities) {
        dst->spill_priorities = (size_t *)malloc(alloc_count * sizeof(size_t));
    }
    if (src->spill_slots) {
        dst->spill_slots = (size_t *)malloc(alloc_count * sizeof(size_t));
    }
    if (src->used_preferred_color_flags) {
        dst->used_preferred_color_flags = (unsigned char *)malloc(alloc_count * sizeof(unsigned char));
    }
    if (src->preferred_color_sources) {
        dst->preferred_color_sources =
            (ValueSsaAllocationPreferredColorSource *)malloc(alloc_count * sizeof(ValueSsaAllocationPreferredColorSource));
    }
    if (src->preferred_color_partner_value_ids) {
        dst->preferred_color_partner_value_ids = (size_t *)malloc(alloc_count * sizeof(size_t));
    }
    if (src->used_generic_eviction_flags) {
        dst->used_generic_eviction_flags = (unsigned char *)malloc(alloc_count * sizeof(unsigned char));
    }
    if (src->generic_eviction_blocker_value_ids) {
        dst->generic_eviction_blocker_value_ids = (size_t *)malloc(alloc_count * sizeof(size_t));
    }
    if (src->generic_eviction_blocker_recolored_flags) {
        dst->generic_eviction_blocker_recolored_flags = (unsigned char *)malloc(alloc_count * sizeof(unsigned char));
    }
    if (src->generic_eviction_blocker_new_colors) {
        dst->generic_eviction_blocker_new_colors = (size_t *)malloc(alloc_count * sizeof(size_t));
    }
    if (src->used_targeted_eviction_flags) {
        dst->used_targeted_eviction_flags = (unsigned char *)malloc(alloc_count * sizeof(unsigned char));
    }
    if (src->targeted_eviction_blocker_value_ids) {
        dst->targeted_eviction_blocker_value_ids = (size_t *)malloc(alloc_count * sizeof(size_t));
    }
    if (src->targeted_eviction_blocker_recolored_flags) {
        dst->targeted_eviction_blocker_recolored_flags = (unsigned char *)malloc(alloc_count * sizeof(unsigned char));
    }
    if (src->targeted_eviction_blocker_new_colors) {
        dst->targeted_eviction_blocker_new_colors = (size_t *)malloc(alloc_count * sizeof(size_t));
    }
    if (src->used_retry_eviction_flags) {
        dst->used_retry_eviction_flags = (unsigned char *)malloc(alloc_count * sizeof(unsigned char));
    }
    if (src->retry_eviction_blocker_value_ids) {
        dst->retry_eviction_blocker_value_ids = (size_t *)malloc(alloc_count * sizeof(size_t));
    }
    if (src->retry_eviction_blocker_recolored_flags) {
        dst->retry_eviction_blocker_recolored_flags = (unsigned char *)malloc(alloc_count * sizeof(unsigned char));
    }
    if (src->retry_eviction_blocker_new_colors) {
        dst->retry_eviction_blocker_new_colors = (size_t *)malloc(alloc_count * sizeof(size_t));
    }

    if ((src->colors && !dst->colors) ||
        (src->has_color && !dst->has_color) ||
        (src->spill_flags && !dst->spill_flags) ||
        (src->spill_intended_flags && !dst->spill_intended_flags) ||
        (src->spill_confirmed_flags && !dst->spill_confirmed_flags) ||
        (src->spill_recovered_flags && !dst->spill_recovered_flags) ||
        (src->spill_recovery_orders && !dst->spill_recovery_orders) ||
        (src->spill_recovery_phase_ids && !dst->spill_recovery_phase_ids) ||
        (src->spill_recovery_phase_member_orders && !dst->spill_recovery_phase_member_orders) ||
        (src->spill_recovery_phase_member_counts && !dst->spill_recovery_phase_member_counts) ||
        (src->spill_recovery_phase_entries && !dst->spill_recovery_phase_entries) ||
        (src->spill_priorities && !dst->spill_priorities) ||
        (src->spill_slots && !dst->spill_slots) ||
        (src->used_preferred_color_flags && !dst->used_preferred_color_flags) ||
        (src->preferred_color_sources && !dst->preferred_color_sources) ||
        (src->preferred_color_partner_value_ids && !dst->preferred_color_partner_value_ids) ||
        (src->used_generic_eviction_flags && !dst->used_generic_eviction_flags) ||
        (src->generic_eviction_blocker_value_ids && !dst->generic_eviction_blocker_value_ids) ||
        (src->generic_eviction_blocker_recolored_flags && !dst->generic_eviction_blocker_recolored_flags) ||
        (src->generic_eviction_blocker_new_colors && !dst->generic_eviction_blocker_new_colors) ||
        (src->used_targeted_eviction_flags && !dst->used_targeted_eviction_flags) ||
        (src->targeted_eviction_blocker_value_ids && !dst->targeted_eviction_blocker_value_ids) ||
        (src->targeted_eviction_blocker_recolored_flags && !dst->targeted_eviction_blocker_recolored_flags) ||
        (src->targeted_eviction_blocker_new_colors && !dst->targeted_eviction_blocker_new_colors) ||
        (src->used_retry_eviction_flags && !dst->used_retry_eviction_flags) ||
        (src->retry_eviction_blocker_value_ids && !dst->retry_eviction_blocker_value_ids) ||
        (src->retry_eviction_blocker_recolored_flags && !dst->retry_eviction_blocker_recolored_flags) ||
        (src->retry_eviction_blocker_new_colors && !dst->retry_eviction_blocker_new_colors)) {
        value_ssa_allocation_result_free(dst);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-235: out of memory cloning allocation result");
        return 0;
    }

    if (src->colors) {
        memcpy(dst->colors, src->colors, alloc_count * sizeof(size_t));
    }
    if (src->has_color) {
        memcpy(dst->has_color, src->has_color, alloc_count * sizeof(unsigned char));
    }
    if (src->spill_flags) {
        memcpy(dst->spill_flags, src->spill_flags, alloc_count * sizeof(unsigned char));
    }
    if (src->spill_intended_flags) {
        memcpy(dst->spill_intended_flags, src->spill_intended_flags, alloc_count * sizeof(unsigned char));
    }
    if (src->spill_confirmed_flags) {
        memcpy(dst->spill_confirmed_flags, src->spill_confirmed_flags, alloc_count * sizeof(unsigned char));
    }
    if (src->spill_recovered_flags) {
        memcpy(dst->spill_recovered_flags, src->spill_recovered_flags, alloc_count * sizeof(unsigned char));
    }
    if (src->spill_recovery_orders) {
        memcpy(dst->spill_recovery_orders, src->spill_recovery_orders, alloc_count * sizeof(size_t));
    }
    if (src->spill_recovery_phase_ids) {
        memcpy(dst->spill_recovery_phase_ids, src->spill_recovery_phase_ids, alloc_count * sizeof(size_t));
    }
    if (src->spill_recovery_phase_member_orders) {
        memcpy(dst->spill_recovery_phase_member_orders, src->spill_recovery_phase_member_orders, alloc_count * sizeof(size_t));
    }
    if (src->spill_recovery_phase_member_counts) {
        memcpy(dst->spill_recovery_phase_member_counts, src->spill_recovery_phase_member_counts, alloc_count * sizeof(size_t));
    }
    if (src->spill_recovery_phase_entries) {
        memcpy(dst->spill_recovery_phase_entries,
            src->spill_recovery_phase_entries,
            alloc_count * sizeof(ValueSsaAllocatorRetryFamilyAgendaItem));
    }
    if (src->spill_priorities) {
        memcpy(dst->spill_priorities, src->spill_priorities, alloc_count * sizeof(size_t));
    }
    if (src->spill_slots) {
        memcpy(dst->spill_slots, src->spill_slots, alloc_count * sizeof(size_t));
    }
    if (src->used_preferred_color_flags) {
        memcpy(dst->used_preferred_color_flags, src->used_preferred_color_flags, alloc_count * sizeof(unsigned char));
    }
    if (src->preferred_color_sources) {
        memcpy(dst->preferred_color_sources,
            src->preferred_color_sources,
            alloc_count * sizeof(ValueSsaAllocationPreferredColorSource));
    }
    if (src->preferred_color_partner_value_ids) {
        memcpy(dst->preferred_color_partner_value_ids, src->preferred_color_partner_value_ids, alloc_count * sizeof(size_t));
    }
    if (src->used_generic_eviction_flags) {
        memcpy(dst->used_generic_eviction_flags, src->used_generic_eviction_flags, alloc_count * sizeof(unsigned char));
    }
    if (src->generic_eviction_blocker_value_ids) {
        memcpy(dst->generic_eviction_blocker_value_ids, src->generic_eviction_blocker_value_ids, alloc_count * sizeof(size_t));
    }
    if (src->generic_eviction_blocker_recolored_flags) {
        memcpy(dst->generic_eviction_blocker_recolored_flags,
            src->generic_eviction_blocker_recolored_flags,
            alloc_count * sizeof(unsigned char));
    }
    if (src->generic_eviction_blocker_new_colors) {
        memcpy(dst->generic_eviction_blocker_new_colors, src->generic_eviction_blocker_new_colors, alloc_count * sizeof(size_t));
    }
    if (src->used_targeted_eviction_flags) {
        memcpy(dst->used_targeted_eviction_flags, src->used_targeted_eviction_flags, alloc_count * sizeof(unsigned char));
    }
    if (src->targeted_eviction_blocker_value_ids) {
        memcpy(dst->targeted_eviction_blocker_value_ids, src->targeted_eviction_blocker_value_ids, alloc_count * sizeof(size_t));
    }
    if (src->targeted_eviction_blocker_recolored_flags) {
        memcpy(dst->targeted_eviction_blocker_recolored_flags,
            src->targeted_eviction_blocker_recolored_flags,
            alloc_count * sizeof(unsigned char));
    }
    if (src->targeted_eviction_blocker_new_colors) {
        memcpy(dst->targeted_eviction_blocker_new_colors, src->targeted_eviction_blocker_new_colors, alloc_count * sizeof(size_t));
    }
    if (src->used_retry_eviction_flags) {
        memcpy(dst->used_retry_eviction_flags, src->used_retry_eviction_flags, alloc_count * sizeof(unsigned char));
    }
    if (src->retry_eviction_blocker_value_ids) {
        memcpy(dst->retry_eviction_blocker_value_ids, src->retry_eviction_blocker_value_ids, alloc_count * sizeof(size_t));
    }
    if (src->retry_eviction_blocker_recolored_flags) {
        memcpy(dst->retry_eviction_blocker_recolored_flags,
            src->retry_eviction_blocker_recolored_flags,
            alloc_count * sizeof(unsigned char));
    }
    if (src->retry_eviction_blocker_new_colors) {
        memcpy(dst->retry_eviction_blocker_new_colors, src->retry_eviction_blocker_new_colors, alloc_count * sizeof(size_t));
    }

    return 1;
}

static int value_ssa_allocation_result_equivalent_ignoring_names(
    const ValueSsaAllocationResult *lhs,
    const ValueSsaAllocationResult *rhs) {
    size_t value_index;

    if (!lhs || !rhs) {
        return 0;
    }
    if (lhs->value_count != rhs->value_count ||
        lhs->color_budget != rhs->color_budget ||
        lhs->spill_slot_count != rhs->spill_slot_count ||
        lhs->next_spill_recovery_order != rhs->next_spill_recovery_order ||
        lhs->next_spill_recovery_phase_id != rhs->next_spill_recovery_phase_id) {
        return 0;
    }

    for (value_index = 0; value_index < lhs->value_count; ++value_index) {
        if ((lhs->colors && rhs->colors && lhs->colors[value_index] != rhs->colors[value_index]) ||
            ((lhs->colors == NULL) != (rhs->colors == NULL)) ||
            (lhs->has_color && rhs->has_color && lhs->has_color[value_index] != rhs->has_color[value_index]) ||
            ((lhs->has_color == NULL) != (rhs->has_color == NULL)) ||
            (lhs->spill_flags && rhs->spill_flags &&
                lhs->spill_flags[value_index] != rhs->spill_flags[value_index]) ||
            ((lhs->spill_flags == NULL) != (rhs->spill_flags == NULL)) ||
            (lhs->spill_intended_flags && rhs->spill_intended_flags &&
                lhs->spill_intended_flags[value_index] != rhs->spill_intended_flags[value_index]) ||
            ((lhs->spill_intended_flags == NULL) != (rhs->spill_intended_flags == NULL)) ||
            (lhs->spill_confirmed_flags && rhs->spill_confirmed_flags &&
                lhs->spill_confirmed_flags[value_index] != rhs->spill_confirmed_flags[value_index]) ||
            ((lhs->spill_confirmed_flags == NULL) != (rhs->spill_confirmed_flags == NULL)) ||
            (lhs->spill_recovered_flags && rhs->spill_recovered_flags &&
                lhs->spill_recovered_flags[value_index] != rhs->spill_recovered_flags[value_index]) ||
            ((lhs->spill_recovered_flags == NULL) != (rhs->spill_recovered_flags == NULL)) ||
            (lhs->spill_recovery_orders && rhs->spill_recovery_orders &&
                lhs->spill_recovery_orders[value_index] != rhs->spill_recovery_orders[value_index]) ||
            ((lhs->spill_recovery_orders == NULL) != (rhs->spill_recovery_orders == NULL)) ||
            (lhs->spill_recovery_phase_ids && rhs->spill_recovery_phase_ids &&
                lhs->spill_recovery_phase_ids[value_index] != rhs->spill_recovery_phase_ids[value_index]) ||
            ((lhs->spill_recovery_phase_ids == NULL) != (rhs->spill_recovery_phase_ids == NULL)) ||
            (lhs->spill_recovery_phase_member_orders && rhs->spill_recovery_phase_member_orders &&
                lhs->spill_recovery_phase_member_orders[value_index] !=
                    rhs->spill_recovery_phase_member_orders[value_index]) ||
            ((lhs->spill_recovery_phase_member_orders == NULL) !=
                (rhs->spill_recovery_phase_member_orders == NULL)) ||
            (lhs->spill_recovery_phase_member_counts && rhs->spill_recovery_phase_member_counts &&
                lhs->spill_recovery_phase_member_counts[value_index] !=
                    rhs->spill_recovery_phase_member_counts[value_index]) ||
            ((lhs->spill_recovery_phase_member_counts == NULL) !=
                (rhs->spill_recovery_phase_member_counts == NULL)) ||
            (lhs->spill_priorities && rhs->spill_priorities &&
                lhs->spill_priorities[value_index] != rhs->spill_priorities[value_index]) ||
            ((lhs->spill_priorities == NULL) != (rhs->spill_priorities == NULL)) ||
            (lhs->spill_slots && rhs->spill_slots &&
                lhs->spill_slots[value_index] != rhs->spill_slots[value_index]) ||
            ((lhs->spill_slots == NULL) != (rhs->spill_slots == NULL)) ||
            (lhs->used_preferred_color_flags && rhs->used_preferred_color_flags &&
                lhs->used_preferred_color_flags[value_index] != rhs->used_preferred_color_flags[value_index]) ||
            ((lhs->used_preferred_color_flags == NULL) != (rhs->used_preferred_color_flags == NULL)) ||
            (lhs->preferred_color_sources && rhs->preferred_color_sources &&
                lhs->preferred_color_sources[value_index] != rhs->preferred_color_sources[value_index]) ||
            ((lhs->preferred_color_sources == NULL) != (rhs->preferred_color_sources == NULL)) ||
            (lhs->preferred_color_partner_value_ids && rhs->preferred_color_partner_value_ids &&
                lhs->preferred_color_partner_value_ids[value_index] !=
                    rhs->preferred_color_partner_value_ids[value_index]) ||
            ((lhs->preferred_color_partner_value_ids == NULL) !=
                (rhs->preferred_color_partner_value_ids == NULL)) ||
            (lhs->used_generic_eviction_flags && rhs->used_generic_eviction_flags &&
                lhs->used_generic_eviction_flags[value_index] != rhs->used_generic_eviction_flags[value_index]) ||
            ((lhs->used_generic_eviction_flags == NULL) != (rhs->used_generic_eviction_flags == NULL)) ||
            (lhs->generic_eviction_blocker_value_ids && rhs->generic_eviction_blocker_value_ids &&
                lhs->generic_eviction_blocker_value_ids[value_index] !=
                    rhs->generic_eviction_blocker_value_ids[value_index]) ||
            ((lhs->generic_eviction_blocker_value_ids == NULL) !=
                (rhs->generic_eviction_blocker_value_ids == NULL)) ||
            (lhs->generic_eviction_blocker_recolored_flags && rhs->generic_eviction_blocker_recolored_flags &&
                lhs->generic_eviction_blocker_recolored_flags[value_index] !=
                    rhs->generic_eviction_blocker_recolored_flags[value_index]) ||
            ((lhs->generic_eviction_blocker_recolored_flags == NULL) !=
                (rhs->generic_eviction_blocker_recolored_flags == NULL)) ||
            (lhs->generic_eviction_blocker_new_colors && rhs->generic_eviction_blocker_new_colors &&
                lhs->generic_eviction_blocker_new_colors[value_index] !=
                    rhs->generic_eviction_blocker_new_colors[value_index]) ||
            ((lhs->generic_eviction_blocker_new_colors == NULL) !=
                (rhs->generic_eviction_blocker_new_colors == NULL)) ||
            (lhs->used_targeted_eviction_flags && rhs->used_targeted_eviction_flags &&
                lhs->used_targeted_eviction_flags[value_index] != rhs->used_targeted_eviction_flags[value_index]) ||
            ((lhs->used_targeted_eviction_flags == NULL) != (rhs->used_targeted_eviction_flags == NULL)) ||
            (lhs->targeted_eviction_blocker_value_ids && rhs->targeted_eviction_blocker_value_ids &&
                lhs->targeted_eviction_blocker_value_ids[value_index] !=
                    rhs->targeted_eviction_blocker_value_ids[value_index]) ||
            ((lhs->targeted_eviction_blocker_value_ids == NULL) !=
                (rhs->targeted_eviction_blocker_value_ids == NULL)) ||
            (lhs->targeted_eviction_blocker_recolored_flags && rhs->targeted_eviction_blocker_recolored_flags &&
                lhs->targeted_eviction_blocker_recolored_flags[value_index] !=
                    rhs->targeted_eviction_blocker_recolored_flags[value_index]) ||
            ((lhs->targeted_eviction_blocker_recolored_flags == NULL) !=
                (rhs->targeted_eviction_blocker_recolored_flags == NULL)) ||
            (lhs->targeted_eviction_blocker_new_colors && rhs->targeted_eviction_blocker_new_colors &&
                lhs->targeted_eviction_blocker_new_colors[value_index] !=
                    rhs->targeted_eviction_blocker_new_colors[value_index]) ||
            ((lhs->targeted_eviction_blocker_new_colors == NULL) !=
                (rhs->targeted_eviction_blocker_new_colors == NULL)) ||
            (lhs->used_retry_eviction_flags && rhs->used_retry_eviction_flags &&
                lhs->used_retry_eviction_flags[value_index] != rhs->used_retry_eviction_flags[value_index]) ||
            ((lhs->used_retry_eviction_flags == NULL) != (rhs->used_retry_eviction_flags == NULL)) ||
            (lhs->retry_eviction_blocker_value_ids && rhs->retry_eviction_blocker_value_ids &&
                lhs->retry_eviction_blocker_value_ids[value_index] !=
                    rhs->retry_eviction_blocker_value_ids[value_index]) ||
            ((lhs->retry_eviction_blocker_value_ids == NULL) !=
                (rhs->retry_eviction_blocker_value_ids == NULL)) ||
            (lhs->retry_eviction_blocker_recolored_flags && rhs->retry_eviction_blocker_recolored_flags &&
                lhs->retry_eviction_blocker_recolored_flags[value_index] !=
                    rhs->retry_eviction_blocker_recolored_flags[value_index]) ||
            ((lhs->retry_eviction_blocker_recolored_flags == NULL) !=
                (rhs->retry_eviction_blocker_recolored_flags == NULL)) ||
            (lhs->retry_eviction_blocker_new_colors && rhs->retry_eviction_blocker_new_colors &&
                lhs->retry_eviction_blocker_new_colors[value_index] !=
                    rhs->retry_eviction_blocker_new_colors[value_index]) ||
            ((lhs->retry_eviction_blocker_new_colors == NULL) !=
                (rhs->retry_eviction_blocker_new_colors == NULL))) {
            return 0;
        }
    }

    return 1;
}

static int value_ssa_program_allocation_result_equivalent_ignoring_names(
    const ValueSsaProgramAllocationResult *lhs,
    const ValueSsaProgramAllocationResult *rhs) {
    size_t function_index;

    if (!lhs || !rhs || lhs->function_count != rhs->function_count) {
        return 0;
    }

    for (function_index = 0; function_index < lhs->function_count; ++function_index) {
        if (!value_ssa_allocation_result_equivalent_ignoring_names(
                &lhs->function_results[function_index], &rhs->function_results[function_index])) {
            return 0;
        }
    }

    return 1;
}

void value_ssa_function_allocation_layout_init(ValueSsaFunctionAllocationLayout *layout) {
    if (!layout) {
        return;
    }

    layout->function_name = NULL;
    layout->value_count = 0;
    layout->color_budget = 0;
    layout->spill_slot_count = 0;
    layout->colored_count = 0;
    layout->spilled_count = 0;
    layout->optimistic_colored_count = 0;
    layout->confirmed_spilled_count = 0;
    layout->recovered_colored_count = 0;
    layout->used_color_count = 0;
    layout->color_group_count = 0;
    layout->spill_slot_group_count = 0;
    layout->placements = NULL;
    layout->colored_value_ids = NULL;
    layout->spilled_value_ids = NULL;
    layout->optimistic_colored_value_ids = NULL;
    layout->confirmed_spilled_value_ids = NULL;
    layout->recovered_colored_value_ids = NULL;
    layout->used_colors = NULL;
    layout->color_groups = NULL;
    layout->spill_slot_groups = NULL;
}

void value_ssa_function_allocation_layout_free(ValueSsaFunctionAllocationLayout *layout) {
    if (!layout) {
        return;
    }

    free(layout->function_name);
    free(layout->placements);
    free(layout->colored_value_ids);
    free(layout->spilled_value_ids);
    free(layout->optimistic_colored_value_ids);
    free(layout->confirmed_spilled_value_ids);
    free(layout->recovered_colored_value_ids);
    free(layout->used_colors);
    if (layout->color_groups) {
        size_t color_group_index;
        for (color_group_index = 0; color_group_index < layout->color_group_count; ++color_group_index) {
            free(layout->color_groups[color_group_index].value_ids);
        }
    }
    if (layout->spill_slot_groups) {
        size_t spill_group_index;
        for (spill_group_index = 0; spill_group_index < layout->spill_slot_group_count; ++spill_group_index) {
            free(layout->spill_slot_groups[spill_group_index].value_ids);
        }
    }
    free(layout->color_groups);
    free(layout->spill_slot_groups);
    value_ssa_function_allocation_layout_init(layout);
}

void value_ssa_program_allocation_layout_init(ValueSsaProgramAllocationLayout *layout) {
    if (!layout) {
        return;
    }

    layout->function_count = 0;
    layout->function_names = NULL;
    layout->total_value_count = 0;
    layout->total_colored_count = 0;
    layout->total_spilled_count = 0;
    layout->total_optimistic_colored_count = 0;
    layout->total_confirmed_spilled_count = 0;
    layout->total_recovered_colored_count = 0;
    layout->functions_with_colors_count = 0;
    layout->functions_with_spills_count = 0;
    layout->functions_with_confirmed_spills_count = 0;
    layout->functions_with_optimistic_colors_count = 0;
    layout->functions_with_recovered_colors_count = 0;
    layout->max_color_budget = 0;
    layout->max_spill_slot_count = 0;
    layout->function_layouts = NULL;
    layout->function_indices_with_colors = NULL;
    layout->function_indices_with_spills = NULL;
    layout->function_indices_with_confirmed_spills = NULL;
    layout->function_indices_with_optimistic_colors = NULL;
    layout->function_indices_with_recovered_colors = NULL;
}

void value_ssa_program_allocation_layout_free(ValueSsaProgramAllocationLayout *layout) {
    size_t function_index;

    if (!layout) {
        return;
    }

    for (function_index = 0; function_index < layout->function_count; ++function_index) {
        value_ssa_function_allocation_layout_free(&layout->function_layouts[function_index]);
        free(layout->function_names ? layout->function_names[function_index] : NULL);
    }
    free(layout->function_layouts);
    free(layout->function_names);
    free(layout->function_indices_with_colors);
    free(layout->function_indices_with_spills);
    free(layout->function_indices_with_confirmed_spills);
    free(layout->function_indices_with_optimistic_colors);
    free(layout->function_indices_with_recovered_colors);
    value_ssa_program_allocation_layout_init(layout);
}

void value_ssa_allocate_rewrite_layout_report_init(ValueSsaAllocateRewriteLayoutReport *report) {
    if (!report) {
        return;
    }

    value_ssa_allocate_rewrite_stats_init(&report->stats);
    value_ssa_program_allocation_layout_init(&report->layout);
}

void value_ssa_allocate_rewrite_layout_report_free(ValueSsaAllocateRewriteLayoutReport *report) {
    if (!report) {
        return;
    }

    value_ssa_program_allocation_layout_free(&report->layout);
    value_ssa_allocate_rewrite_layout_report_init(report);
}

static int value_ssa_clone_function_allocation_layout(const ValueSsaFunctionAllocationLayout *src,
    ValueSsaFunctionAllocationLayout *dst,
    ValueSsaError *error) {
    size_t group_index;

    if (!src || !dst) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-222: invalid function layout clone contract");
        return 0;
    }

    value_ssa_function_allocation_layout_free(dst);
    *dst = *src;
    dst->function_name = NULL;
    dst->placements = NULL;
    dst->colored_value_ids = NULL;
    dst->spilled_value_ids = NULL;
    dst->optimistic_colored_value_ids = NULL;
    dst->confirmed_spilled_value_ids = NULL;
    dst->recovered_colored_value_ids = NULL;
    dst->used_colors = NULL;
    dst->color_groups = NULL;
    dst->spill_slot_groups = NULL;

    if (src->function_name) {
        dst->function_name = value_ssa_alloc_strdup(src->function_name);
        if (!dst->function_name) {
            value_ssa_function_allocation_layout_free(dst);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-223: out of memory cloning function layout name");
            return 0;
        }
    }
    if (src->value_count > 0) {
        dst->placements = (ValueSsaAllocatedValuePlacement *)malloc(src->value_count * sizeof(ValueSsaAllocatedValuePlacement));
        dst->colored_value_ids = (size_t *)malloc(src->value_count * sizeof(size_t));
        dst->spilled_value_ids = (size_t *)malloc(src->value_count * sizeof(size_t));
        dst->optimistic_colored_value_ids = (size_t *)malloc(src->value_count * sizeof(size_t));
        dst->confirmed_spilled_value_ids = (size_t *)malloc(src->value_count * sizeof(size_t));
        dst->recovered_colored_value_ids = (size_t *)malloc(src->value_count * sizeof(size_t));
        if (!dst->placements || !dst->colored_value_ids || !dst->spilled_value_ids ||
            !dst->optimistic_colored_value_ids || !dst->confirmed_spilled_value_ids ||
            !dst->recovered_colored_value_ids) {
            value_ssa_function_allocation_layout_free(dst);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-224: out of memory cloning function layout tables");
            return 0;
        }
        memcpy(dst->placements, src->placements, src->value_count * sizeof(ValueSsaAllocatedValuePlacement));
        memcpy(dst->colored_value_ids, src->colored_value_ids, src->colored_count * sizeof(size_t));
        memcpy(dst->spilled_value_ids, src->spilled_value_ids, src->spilled_count * sizeof(size_t));
        memcpy(dst->optimistic_colored_value_ids,
            src->optimistic_colored_value_ids,
            src->optimistic_colored_count * sizeof(size_t));
        memcpy(dst->confirmed_spilled_value_ids,
            src->confirmed_spilled_value_ids,
            src->confirmed_spilled_count * sizeof(size_t));
        memcpy(dst->recovered_colored_value_ids,
            src->recovered_colored_value_ids,
            src->recovered_colored_count * sizeof(size_t));
    }
    if (src->used_color_count > 0) {
        dst->used_colors = (size_t *)malloc(src->used_color_count * sizeof(size_t));
        if (!dst->used_colors) {
            value_ssa_function_allocation_layout_free(dst);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-225: out of memory cloning used-colors");
            return 0;
        }
        memcpy(dst->used_colors, src->used_colors, src->used_color_count * sizeof(size_t));
    }
    if (src->color_group_count > 0) {
        dst->color_groups =
            (ValueSsaAllocationColorGroup *)calloc(src->color_group_count, sizeof(ValueSsaAllocationColorGroup));
        if (!dst->color_groups) {
            value_ssa_function_allocation_layout_free(dst);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-226: out of memory cloning color groups");
            return 0;
        }
        for (group_index = 0; group_index < src->color_group_count; ++group_index) {
            dst->color_groups[group_index] = src->color_groups[group_index];
            dst->color_groups[group_index].value_ids = NULL;
            if (src->color_groups[group_index].value_count > 0) {
                dst->color_groups[group_index].value_ids =
                    (size_t *)malloc(src->color_groups[group_index].value_count * sizeof(size_t));
                if (!dst->color_groups[group_index].value_ids) {
                    value_ssa_function_allocation_layout_free(dst);
                    value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-227: out of memory cloning color-group members");
                    return 0;
                }
                memcpy(dst->color_groups[group_index].value_ids,
                    src->color_groups[group_index].value_ids,
                    src->color_groups[group_index].value_count * sizeof(size_t));
            }
        }
    }
    if (src->spill_slot_group_count > 0) {
        dst->spill_slot_groups =
            (ValueSsaAllocationSpillSlotGroup *)calloc(src->spill_slot_group_count, sizeof(ValueSsaAllocationSpillSlotGroup));
        if (!dst->spill_slot_groups) {
            value_ssa_function_allocation_layout_free(dst);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-228: out of memory cloning spill-slot groups");
            return 0;
        }
        for (group_index = 0; group_index < src->spill_slot_group_count; ++group_index) {
            dst->spill_slot_groups[group_index] = src->spill_slot_groups[group_index];
            dst->spill_slot_groups[group_index].value_ids = NULL;
            if (src->spill_slot_groups[group_index].value_count > 0) {
                dst->spill_slot_groups[group_index].value_ids =
                    (size_t *)malloc(src->spill_slot_groups[group_index].value_count * sizeof(size_t));
                if (!dst->spill_slot_groups[group_index].value_ids) {
                    value_ssa_function_allocation_layout_free(dst);
                    value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-229: out of memory cloning spill-slot-group members");
                    return 0;
                }
                memcpy(dst->spill_slot_groups[group_index].value_ids,
                    src->spill_slot_groups[group_index].value_ids,
                    src->spill_slot_groups[group_index].value_count * sizeof(size_t));
            }
        }
    }

    return 1;
}

static int value_ssa_clone_program_allocation_layout(const ValueSsaProgramAllocationLayout *src,
    ValueSsaProgramAllocationLayout *dst,
    ValueSsaError *error) {
    size_t function_index;

    if (!src || !dst) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-230: invalid program layout clone contract");
        return 0;
    }

    value_ssa_program_allocation_layout_free(dst);
    *dst = *src;
    dst->function_names = NULL;
    dst->function_layouts = NULL;
    dst->function_indices_with_colors = NULL;
    dst->function_indices_with_spills = NULL;
    dst->function_indices_with_confirmed_spills = NULL;
    dst->function_indices_with_optimistic_colors = NULL;
    dst->function_indices_with_recovered_colors = NULL;

    if (src->function_count > 0) {
        dst->function_layouts =
            (ValueSsaFunctionAllocationLayout *)calloc(src->function_count, sizeof(ValueSsaFunctionAllocationLayout));
        dst->function_names = (char **)calloc(src->function_count, sizeof(char *));
        if (!dst->function_layouts || !dst->function_names) {
            value_ssa_program_allocation_layout_free(dst);
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-231: out of memory cloning program layout tables");
            return 0;
        }
        for (function_index = 0; function_index < src->function_count; ++function_index) {
            value_ssa_function_allocation_layout_init(&dst->function_layouts[function_index]);
            if (!value_ssa_clone_function_allocation_layout(
                    &src->function_layouts[function_index], &dst->function_layouts[function_index], error)) {
                value_ssa_program_allocation_layout_free(dst);
                return 0;
            }
            if (src->function_names && src->function_names[function_index]) {
                dst->function_names[function_index] = value_ssa_alloc_strdup(src->function_names[function_index]);
                if (!dst->function_names[function_index]) {
                    value_ssa_program_allocation_layout_free(dst);
                    value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-232: out of memory cloning program function names");
                    return 0;
                }
            }
        }
    }
    if (src->functions_with_colors_count > 0) {
        dst->function_indices_with_colors =
            (size_t *)malloc(src->functions_with_colors_count * sizeof(size_t));
    }
    if (src->functions_with_spills_count > 0) {
        dst->function_indices_with_spills =
            (size_t *)malloc(src->functions_with_spills_count * sizeof(size_t));
    }
    if (src->functions_with_confirmed_spills_count > 0) {
        dst->function_indices_with_confirmed_spills =
            (size_t *)malloc(src->functions_with_confirmed_spills_count * sizeof(size_t));
    }
    if (src->functions_with_optimistic_colors_count > 0) {
        dst->function_indices_with_optimistic_colors =
            (size_t *)malloc(src->functions_with_optimistic_colors_count * sizeof(size_t));
    }
    if (src->functions_with_recovered_colors_count > 0) {
        dst->function_indices_with_recovered_colors =
            (size_t *)malloc(src->functions_with_recovered_colors_count * sizeof(size_t));
    }
    if ((src->functions_with_colors_count > 0 && !dst->function_indices_with_colors) ||
        (src->functions_with_spills_count > 0 && !dst->function_indices_with_spills) ||
        (src->functions_with_confirmed_spills_count > 0 && !dst->function_indices_with_confirmed_spills) ||
        (src->functions_with_optimistic_colors_count > 0 && !dst->function_indices_with_optimistic_colors) ||
        (src->functions_with_recovered_colors_count > 0 && !dst->function_indices_with_recovered_colors)) {
        value_ssa_program_allocation_layout_free(dst);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-233: out of memory cloning program layout filters");
        return 0;
    }
    if (src->functions_with_colors_count > 0) {
        memcpy(dst->function_indices_with_colors,
            src->function_indices_with_colors,
            src->functions_with_colors_count * sizeof(size_t));
    }
    if (src->functions_with_spills_count > 0) {
        memcpy(dst->function_indices_with_spills,
            src->function_indices_with_spills,
            src->functions_with_spills_count * sizeof(size_t));
    }
    if (src->functions_with_confirmed_spills_count > 0) {
        memcpy(dst->function_indices_with_confirmed_spills,
            src->function_indices_with_confirmed_spills,
            src->functions_with_confirmed_spills_count * sizeof(size_t));
    }
    if (src->functions_with_optimistic_colors_count > 0) {
        memcpy(dst->function_indices_with_optimistic_colors,
            src->function_indices_with_optimistic_colors,
            src->functions_with_optimistic_colors_count * sizeof(size_t));
    }
    if (src->functions_with_recovered_colors_count > 0) {
        memcpy(dst->function_indices_with_recovered_colors,
            src->function_indices_with_recovered_colors,
            src->functions_with_recovered_colors_count * sizeof(size_t));
    }

    return 1;
}

int value_ssa_allocate_rewrite_layout_report_set_stats(ValueSsaAllocateRewriteLayoutReport *report,
    const ValueSsaAllocateRewriteStats *stats,
    ValueSsaError *error) {
    if (!report || !stats) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-218: invalid layout-report stats set contract");
        return 0;
    }

    report->stats = *stats;
    return 1;
}

int value_ssa_allocate_rewrite_layout_report_set_program_layout(ValueSsaAllocateRewriteLayoutReport *report,
    const ValueSsaProgramAllocationLayout *layout,
    ValueSsaError *error) {
    if (!report || !layout) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-219: invalid layout-report layout set contract");
        return 0;
    }

    return value_ssa_clone_program_allocation_layout(layout, &report->layout, error);
}

int value_ssa_build_allocate_rewrite_layout_report(const ValueSsaProgramAllocationResult *result,
    const ValueSsaAllocateRewriteStats *stats,
    ValueSsaAllocateRewriteLayoutReport *out_report,
    ValueSsaError *error) {
    if (!result || !stats || !out_report) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-208: invalid allocate+rewrite layout report build contract");
        return 0;
    }

    value_ssa_allocate_rewrite_layout_report_free(out_report);
    if (!value_ssa_allocate_rewrite_layout_report_set_stats(out_report, stats, error) ||
        !value_ssa_build_program_allocation_layout(result, &out_report->layout, error)) {
        value_ssa_allocate_rewrite_layout_report_free(out_report);
        return 0;
    }

    return 1;
}

int value_ssa_allocate_rewrite_layout_report_attach_program_context(
    ValueSsaAllocateRewriteLayoutReport *report,
    const ValueSsaProgram *program,
    ValueSsaError *error) {
    if (!report || !program) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-207: invalid layout-report context attachment contract");
        return 0;
    }

    return value_ssa_program_allocation_layout_attach_program_context(&report->layout, program, error);
}

int value_ssa_function_allocation_layout_equivalent_ignoring_names(
    const ValueSsaFunctionAllocationLayout *lhs,
    const ValueSsaFunctionAllocationLayout *rhs) {
    size_t index;

    if (!lhs || !rhs) {
        return 0;
    }
    if (lhs->value_count != rhs->value_count || lhs->color_budget != rhs->color_budget ||
        lhs->spill_slot_count != rhs->spill_slot_count || lhs->colored_count != rhs->colored_count ||
        lhs->spilled_count != rhs->spilled_count ||
        lhs->optimistic_colored_count != rhs->optimistic_colored_count ||
        lhs->confirmed_spilled_count != rhs->confirmed_spilled_count ||
        lhs->recovered_colored_count != rhs->recovered_colored_count ||
        lhs->used_color_count != rhs->used_color_count || lhs->color_group_count != rhs->color_group_count ||
        lhs->spill_slot_group_count != rhs->spill_slot_group_count) {
        return 0;
    }

    for (index = 0; index < lhs->value_count; ++index) {
        const ValueSsaAllocatedValuePlacement *lhs_placement = &lhs->placements[index];
        const ValueSsaAllocatedValuePlacement *rhs_placement = &rhs->placements[index];

        if (lhs_placement->value_id != rhs_placement->value_id || lhs_placement->kind != rhs_placement->kind ||
            lhs_placement->color != rhs_placement->color || lhs_placement->spill_slot != rhs_placement->spill_slot ||
            lhs_placement->spill_intended != rhs_placement->spill_intended ||
            lhs_placement->spill_confirmed != rhs_placement->spill_confirmed ||
            lhs_placement->spill_recovered != rhs_placement->spill_recovered) {
            return 0;
        }
    }
    for (index = 0; index < lhs->colored_count; ++index) {
        if (lhs->colored_value_ids[index] != rhs->colored_value_ids[index]) {
            return 0;
        }
    }
    for (index = 0; index < lhs->spilled_count; ++index) {
        if (lhs->spilled_value_ids[index] != rhs->spilled_value_ids[index]) {
            return 0;
        }
    }
    for (index = 0; index < lhs->optimistic_colored_count; ++index) {
        if (lhs->optimistic_colored_value_ids[index] != rhs->optimistic_colored_value_ids[index]) {
            return 0;
        }
    }
    for (index = 0; index < lhs->confirmed_spilled_count; ++index) {
        if (lhs->confirmed_spilled_value_ids[index] != rhs->confirmed_spilled_value_ids[index]) {
            return 0;
        }
    }
    for (index = 0; index < lhs->recovered_colored_count; ++index) {
        if (lhs->recovered_colored_value_ids[index] != rhs->recovered_colored_value_ids[index]) {
            return 0;
        }
    }
    for (index = 0; index < lhs->used_color_count; ++index) {
        if (lhs->used_colors[index] != rhs->used_colors[index]) {
            return 0;
        }
    }
    for (index = 0; index < lhs->color_group_count; ++index) {
        size_t member_index;
        const ValueSsaAllocationColorGroup *lhs_group = &lhs->color_groups[index];
        const ValueSsaAllocationColorGroup *rhs_group = &rhs->color_groups[index];

        if (lhs_group->color != rhs_group->color || lhs_group->value_count != rhs_group->value_count) {
            return 0;
        }
        for (member_index = 0; member_index < lhs_group->value_count; ++member_index) {
            if (lhs_group->value_ids[member_index] != rhs_group->value_ids[member_index]) {
                return 0;
            }
        }
    }
    for (index = 0; index < lhs->spill_slot_group_count; ++index) {
        size_t member_index;
        const ValueSsaAllocationSpillSlotGroup *lhs_group = &lhs->spill_slot_groups[index];
        const ValueSsaAllocationSpillSlotGroup *rhs_group = &rhs->spill_slot_groups[index];

        if (lhs_group->spill_slot != rhs_group->spill_slot || lhs_group->value_count != rhs_group->value_count) {
            return 0;
        }
        for (member_index = 0; member_index < lhs_group->value_count; ++member_index) {
            if (lhs_group->value_ids[member_index] != rhs_group->value_ids[member_index]) {
                return 0;
            }
        }
    }

    return 1;
}

int value_ssa_program_allocation_layout_equivalent_ignoring_names(
    const ValueSsaProgramAllocationLayout *lhs,
    const ValueSsaProgramAllocationLayout *rhs) {
    size_t function_index;
    size_t index;

    if (!lhs || !rhs) {
        return 0;
    }
    if (lhs->function_count != rhs->function_count ||
        lhs->total_value_count != rhs->total_value_count ||
        lhs->total_colored_count != rhs->total_colored_count ||
        lhs->total_spilled_count != rhs->total_spilled_count ||
        lhs->total_optimistic_colored_count != rhs->total_optimistic_colored_count ||
        lhs->total_confirmed_spilled_count != rhs->total_confirmed_spilled_count ||
        lhs->total_recovered_colored_count != rhs->total_recovered_colored_count ||
        lhs->functions_with_colors_count != rhs->functions_with_colors_count ||
        lhs->functions_with_spills_count != rhs->functions_with_spills_count ||
        lhs->functions_with_confirmed_spills_count != rhs->functions_with_confirmed_spills_count ||
        lhs->functions_with_optimistic_colors_count != rhs->functions_with_optimistic_colors_count ||
        lhs->functions_with_recovered_colors_count != rhs->functions_with_recovered_colors_count ||
        lhs->max_color_budget != rhs->max_color_budget ||
        lhs->max_spill_slot_count != rhs->max_spill_slot_count) {
        return 0;
    }
    for (index = 0; index < lhs->functions_with_colors_count; ++index) {
        if (lhs->function_indices_with_colors[index] != rhs->function_indices_with_colors[index]) {
            return 0;
        }
    }
    for (index = 0; index < lhs->functions_with_spills_count; ++index) {
        if (lhs->function_indices_with_spills[index] != rhs->function_indices_with_spills[index]) {
            return 0;
        }
    }
    for (index = 0; index < lhs->functions_with_confirmed_spills_count; ++index) {
        if (lhs->function_indices_with_confirmed_spills[index] != rhs->function_indices_with_confirmed_spills[index]) {
            return 0;
        }
    }
    for (index = 0; index < lhs->functions_with_optimistic_colors_count; ++index) {
        if (lhs->function_indices_with_optimistic_colors[index] != rhs->function_indices_with_optimistic_colors[index]) {
            return 0;
        }
    }
    for (index = 0; index < lhs->functions_with_recovered_colors_count; ++index) {
        if (lhs->function_indices_with_recovered_colors[index] != rhs->function_indices_with_recovered_colors[index]) {
            return 0;
        }
    }

    for (function_index = 0; function_index < lhs->function_count; ++function_index) {
        if (!value_ssa_function_allocation_layout_equivalent_ignoring_names(
                &lhs->function_layouts[function_index], &rhs->function_layouts[function_index])) {
            return 0;
        }
    }

    return 1;
}

int value_ssa_allocate_rewrite_layout_report_equivalent_ignoring_names(
    const ValueSsaAllocateRewriteLayoutReport *lhs,
    const ValueSsaAllocateRewriteLayoutReport *rhs) {
    if (!lhs || !rhs) {
        return 0;
    }
    if (lhs->stats.allocation_rounds != rhs->stats.allocation_rounds ||
        lhs->stats.rewrite_rounds != rhs->stats.rewrite_rounds) {
        return 0;
    }

    return value_ssa_program_allocation_layout_equivalent_ignoring_names(&lhs->layout, &rhs->layout);
}

int value_ssa_allocate_rewrite_layout_report_get_stats(const ValueSsaAllocateRewriteLayoutReport *report,
    size_t *out_allocation_rounds,
    size_t *out_rewrite_rounds) {
    if (!report) {
        return 0;
    }
    if (out_allocation_rounds) {
        *out_allocation_rounds = report->stats.allocation_rounds;
    }
    if (out_rewrite_rounds) {
        *out_rewrite_rounds = report->stats.rewrite_rounds;
    }
    return 1;
}

int value_ssa_allocate_rewrite_layout_report_get_program_layout(const ValueSsaAllocateRewriteLayoutReport *report,
    const ValueSsaProgramAllocationLayout **out_layout) {
    if (out_layout) {
        *out_layout = NULL;
    }
    if (!report || !out_layout) {
        return 0;
    }

    *out_layout = &report->layout;
    return 1;
}

int value_ssa_allocate_rewrite_layout_report_get_function_layout(
    const ValueSsaAllocateRewriteLayoutReport *report,
    size_t function_index,
    const ValueSsaFunctionAllocationLayout **out_function_layout) {
    if (out_function_layout) {
        *out_function_layout = NULL;
    }
    if (!report) {
        return 0;
    }

    return value_ssa_program_allocation_layout_get_function_layout(
        &report->layout, function_index, out_function_layout);
}

int value_ssa_allocate_rewrite_layout_report_get_function_layout_by_name(
    const ValueSsaAllocateRewriteLayoutReport *report,
    const char *function_name,
    size_t *out_function_index,
    const ValueSsaFunctionAllocationLayout **out_function_layout) {
    if (out_function_index) {
        *out_function_index = 0;
    }
    if (out_function_layout) {
        *out_function_layout = NULL;
    }
    if (!report) {
        return 0;
    }

    return value_ssa_program_allocation_layout_get_function_layout_by_name(
        &report->layout, function_name, out_function_index, out_function_layout);
}

int value_ssa_allocate_rewrite_layout_report_get_function_name(const ValueSsaAllocateRewriteLayoutReport *report,
    size_t function_index,
    const char **out_function_name) {
    if (out_function_name) {
        *out_function_name = NULL;
    }
    if (!report) {
        return 0;
    }

    return value_ssa_program_allocation_layout_get_function_name(
        &report->layout, function_index, out_function_name);
}

int value_ssa_allocate_rewrite_layout_report_find_function_index_by_name(
    const ValueSsaAllocateRewriteLayoutReport *report,
    const char *function_name,
    size_t *out_function_index) {
    if (out_function_index) {
        *out_function_index = 0;
    }
    if (!report) {
        return 0;
    }

    return value_ssa_program_allocation_layout_find_function_index_by_name(
        &report->layout, function_name, out_function_index);
}

int value_ssa_allocate_rewrite_layout_report_get_function_placement(
    const ValueSsaAllocateRewriteLayoutReport *report,
    size_t function_index,
    size_t value_id,
    const ValueSsaAllocatedValuePlacement **out_placement) {
    if (out_placement) {
        *out_placement = NULL;
    }
    if (!report) {
        return 0;
    }

    return value_ssa_program_allocation_layout_get_function_placement(
        &report->layout, function_index, value_id, out_placement);
}

int value_ssa_allocate_rewrite_layout_report_get_function_summary(
    const ValueSsaAllocateRewriteLayoutReport *report,
    size_t function_index,
    const char **out_function_name,
    size_t *out_value_count,
    size_t *out_color_budget,
    size_t *out_spill_slot_count,
    size_t *out_colored_count,
    size_t *out_spilled_count,
    size_t *out_optimistic_colored_count,
    size_t *out_confirmed_spilled_count,
    size_t *out_recovered_colored_count,
    size_t *out_used_color_count,
    size_t *out_color_group_count,
    size_t *out_spill_slot_group_count) {
    if (!report) {
        return 0;
    }

    return value_ssa_program_allocation_layout_get_function_summary(&report->layout,
        function_index,
        out_function_name,
        out_value_count,
        out_color_budget,
        out_spill_slot_count,
        out_colored_count,
        out_spilled_count,
        out_optimistic_colored_count,
        out_confirmed_spilled_count,
        out_recovered_colored_count,
        out_used_color_count,
        out_color_group_count,
        out_spill_slot_group_count);
}

int value_ssa_allocate_rewrite_layout_report_get_functions_with_colors(
    const ValueSsaAllocateRewriteLayoutReport *report,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (!report) {
        return 0;
    }

    return value_ssa_program_allocation_layout_get_functions_with_colors(
        &report->layout, out_count, out_function_indices);
}

int value_ssa_allocate_rewrite_layout_report_get_functions_with_spills(
    const ValueSsaAllocateRewriteLayoutReport *report,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (!report) {
        return 0;
    }

    return value_ssa_program_allocation_layout_get_functions_with_spills(
        &report->layout, out_count, out_function_indices);
}

int value_ssa_allocate_rewrite_layout_report_get_functions_with_confirmed_spills(
    const ValueSsaAllocateRewriteLayoutReport *report,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (!report) {
        return 0;
    }

    return value_ssa_program_allocation_layout_get_functions_with_confirmed_spills(
        &report->layout, out_count, out_function_indices);
}

int value_ssa_allocate_rewrite_layout_report_get_functions_with_optimistic_colors(
    const ValueSsaAllocateRewriteLayoutReport *report,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (!report) {
        return 0;
    }

    return value_ssa_program_allocation_layout_get_functions_with_optimistic_colors(
        &report->layout, out_count, out_function_indices);
}

int value_ssa_allocate_rewrite_layout_report_get_functions_with_recovered_colors(
    const ValueSsaAllocateRewriteLayoutReport *report,
    size_t *out_count,
    const size_t **out_function_indices) {
    if (!report) {
        return 0;
    }

    return value_ssa_program_allocation_layout_get_functions_with_recovered_colors(
        &report->layout, out_count, out_function_indices);
}

int value_ssa_allocate_rewrite_layout_report_get_summary(const ValueSsaAllocateRewriteLayoutReport *report,
    size_t *out_allocation_rounds,
    size_t *out_rewrite_rounds,
    size_t *out_function_count,
    size_t *out_total_value_count,
    size_t *out_total_colored_count,
    size_t *out_total_spilled_count,
    size_t *out_total_optimistic_colored_count,
    size_t *out_total_confirmed_spilled_count,
    size_t *out_total_recovered_colored_count,
    size_t *out_max_color_budget,
    size_t *out_max_spill_slot_count) {
    if (!report) {
        return 0;
    }
    if (!value_ssa_allocate_rewrite_layout_report_get_stats(
            report, out_allocation_rounds, out_rewrite_rounds)) {
        return 0;
    }
    return value_ssa_program_allocation_layout_get_summary(&report->layout,
        out_function_count,
        out_total_value_count,
        out_total_colored_count,
        out_total_spilled_count,
        out_total_optimistic_colored_count,
        out_total_confirmed_spilled_count,
        out_total_recovered_colored_count,
        out_max_color_budget,
        out_max_spill_slot_count);
}

void value_ssa_allocate_rewrite_stats_init(ValueSsaAllocateRewriteStats *stats) {
    if (!stats) {
        return;
    }

    stats->allocation_rounds = 0;
    stats->rewrite_rounds = 0;
}

void value_ssa_allocation_select_stats_init(ValueSsaAllocationSelectStats *stats) {
    if (!stats) {
        return;
    }

    stats->colored_count = 0;
    stats->spilled_count = 0;
    stats->spill_intended_count = 0;
    stats->spill_confirmed_count = 0;
    stats->spill_recovered_count = 0;
    stats->retry_eviction_recovered_count = 0;
    stats->optimistic_direct_count = 0;
    stats->optimistic_recovered_count = 0;
    stats->optimistic_colored_count = 0;
}

const char *value_ssa_allocation_preferred_color_source_name(ValueSsaAllocationPreferredColorSource source) {
    switch (source) {
    case VALUE_SSA_ALLOC_PREFERRED_COLOR_NONE:
        return "none";
    case VALUE_SSA_ALLOC_PREFERRED_COLOR_COALESCE_DIRECT:
        return "coalesce-direct";
    case VALUE_SSA_ALLOC_PREFERRED_COLOR_COALESCE_CLASS:
        return "coalesce-class";
    case VALUE_SSA_ALLOC_PREFERRED_COLOR_AFFINITY:
        return "affinity";
    default:
        return "unknown";
    }
}

const char *value_ssa_allocated_value_placement_kind_name(ValueSsaAllocatedValuePlacementKind kind) {
    switch (kind) {
    case VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_UNALLOCATED:
        return "unallocated";
    case VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_COLOR:
        return "color";
    case VALUE_SSA_ALLOCATED_VALUE_PLACEMENT_SPILL:
        return "spill";
    default:
        return "unknown";
    }
}

void value_ssa_program_allocation_result_free(ValueSsaProgramAllocationResult *result) {
    size_t function_index;

    if (!result) {
        return;
    }

    for (function_index = 0; function_index < result->function_count; ++function_index) {
        value_ssa_allocation_result_free(&result->function_results[function_index]);
    }
    free(result->function_results);
    value_ssa_program_allocation_result_init(result);
}

static int value_ssa_alloc_family_pressure_move_work_rank(ValueSsaAllocatorMoveWorkClass move_work_class) {
    switch (move_work_class) {
    case VALUE_SSA_ALLOC_MOVE_WORK_INTERNAL:
        return 0;
    case VALUE_SSA_ALLOC_MOVE_WORK_RELEASED:
        return 1;
    case VALUE_SSA_ALLOC_MOVE_WORK_FREEZE_PENDING:
        return 2;
    case VALUE_SSA_ALLOC_MOVE_WORK_COALESCE_READY:
    default:
        return 3;
    }
}

static int value_ssa_alloc_compare_family_pressure_items(const ValueSsaAllocatorPlanItem *lhs,
    const ValueSsaAllocatorPlanItem *rhs) {
    if (!lhs || !rhs) {
        return 0;
    }

    if (value_ssa_alloc_family_pressure_move_work_rank(lhs->move_work_class) !=
        value_ssa_alloc_family_pressure_move_work_rank(rhs->move_work_class)) {
        return value_ssa_alloc_family_pressure_move_work_rank(lhs->move_work_class) <
                value_ssa_alloc_family_pressure_move_work_rank(rhs->move_work_class)
            ? -1
            : 1;
    }
    if (lhs->family_coalesce_ready_neighbor_count != rhs->family_coalesce_ready_neighbor_count) {
        return lhs->family_coalesce_ready_neighbor_count < rhs->family_coalesce_ready_neighbor_count ? -1 : 1;
    }
    if (lhs->family_coalesce_ready_affinity_weight_sum != rhs->family_coalesce_ready_affinity_weight_sum) {
        return lhs->family_coalesce_ready_affinity_weight_sum < rhs->family_coalesce_ready_affinity_weight_sum ? -1 : 1;
    }
    if ((int)lhs->family_best_coalesce_ready_partner_is_mutual !=
        (int)rhs->family_best_coalesce_ready_partner_is_mutual) {
        return (int)lhs->family_best_coalesce_ready_partner_is_mutual <
                (int)rhs->family_best_coalesce_ready_partner_is_mutual
            ? -1
            : 1;
    }
    if (lhs->family_best_coalesce_ready_partner_affinity_weight !=
        rhs->family_best_coalesce_ready_partner_affinity_weight) {
        return lhs->family_best_coalesce_ready_partner_affinity_weight <
                rhs->family_best_coalesce_ready_partner_affinity_weight
            ? -1
            : 1;
    }
    if (lhs->family_external_affinity_weight_sum != rhs->family_external_affinity_weight_sum) {
        return lhs->family_external_affinity_weight_sum < rhs->family_external_affinity_weight_sum ? -1 : 1;
    }
    if (lhs->family_external_neighbor_count != rhs->family_external_neighbor_count) {
        return lhs->family_external_neighbor_count < rhs->family_external_neighbor_count ? -1 : 1;
    }
    if (lhs->family_active_member_count != rhs->family_active_member_count) {
        return lhs->family_active_member_count < rhs->family_active_member_count ? -1 : 1;
    }

    return 0;
}

static int value_ssa_alloc_compare_family_pressure_values(const ValueSsaAllocatorPlan *plan,
    size_t lhs_value_id,
    size_t rhs_value_id,
    int *out_compare) {
    const ValueSsaAllocatorPlanItem *lhs_item = NULL;
    const ValueSsaAllocatorPlanItem *rhs_item = NULL;

    if (out_compare) {
        *out_compare = 0;
    }
    if (!plan || !out_compare || !value_ssa_allocator_plan_find_value(plan, lhs_value_id, NULL, &lhs_item) ||
        !value_ssa_allocator_plan_find_value(plan, rhs_value_id, NULL, &rhs_item) || !lhs_item || !rhs_item) {
        return 0;
    }

    *out_compare = value_ssa_alloc_compare_family_pressure_items(lhs_item, rhs_item);
    return 1;
}

typedef struct {
    size_t root_value_id;
    size_t colored_member_count;
    size_t spilled_member_count;
} ValueSsaAllocatorRuntimeFamilyActivity;

typedef struct {
    size_t root_value_id;
    size_t color;
    size_t colored_member_count;
    size_t direct_coalesce_weight_sum;
    size_t best_member_value_id;
    size_t best_direct_partner_value_id;
    size_t best_direct_partner_weight;
} ValueSsaAllocatorRuntimeFamilyColorSupport;

static size_t value_ssa_alloc_plan_family_root_for_value(const ValueSsaAllocatorPlan *plan, size_t value_id) {
    const ValueSsaAllocatorPlanItem *item = NULL;

    if (!plan || value_id >= plan->value_count) {
        return value_id;
    }
    if (plan->final_runtime_coalesce_roots) {
        return plan->final_runtime_coalesce_roots[value_id];
    }
    if (!value_ssa_allocator_plan_find_value(plan, value_id, NULL, &item) || !item) {
        return value_id;
    }
    if (item->coalesce_class_size == 0 || item->coalesce_root_value_id >= plan->value_count) {
        return value_id;
    }
    return item->coalesce_root_value_id;
}

static size_t value_ssa_alloc_runtime_family_root_for_value(const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    size_t value_count,
    size_t value_id) {
    if (plan && value_count == plan->value_count && plan->final_runtime_coalesce_roots && value_id < value_count) {
        return plan->final_runtime_coalesce_roots[value_id];
    }
    if (coalesce_analysis && coalesce_analysis->value_count == value_count && coalesce_analysis->value_roots &&
        value_id < value_count) {
        return coalesce_analysis->value_roots[value_id];
    }
    return value_ssa_alloc_plan_family_root_for_value(plan, value_id);
}

static int value_ssa_alloc_value_has_explicit_family_color_authority(const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    size_t value_count,
    size_t value_id) {
    const ValueSsaAllocatorPlanItem *plan_item = NULL;

    if (value_id >= value_count) {
        return 0;
    }
    if (plan && plan->value_count == value_count && plan->final_runtime_coalesce_roots &&
        plan->final_runtime_coalesce_class_sizes &&
        plan->final_runtime_coalesce_roots[value_id] < value_count &&
        plan->final_runtime_coalesce_class_sizes[plan->final_runtime_coalesce_roots[value_id]] > 1) {
        return 1;
    }
    if (plan && plan->value_count == value_count &&
        value_ssa_allocator_plan_find_value(plan, value_id, NULL, &plan_item) && plan_item &&
        plan_item->coalesce_class_size > 1 && plan_item->coalesce_root_value_id < value_count) {
        return 1;
    }
    if (coalesce_analysis && coalesce_analysis->value_count == value_count && coalesce_analysis->items &&
        coalesce_analysis->item_count > 0 && coalesce_analysis->value_roots && coalesce_analysis->class_sizes &&
        coalesce_analysis->value_roots[value_id] < value_count &&
        coalesce_analysis->class_sizes[coalesce_analysis->value_roots[value_id]] > 1) {
        return 1;
    }
    return 0;
}

static int value_ssa_alloc_collect_runtime_family_activity(const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    ValueSsaAllocatorRuntimeFamilyActivity *out_activity) {
    size_t member_value_id;
    size_t root_value_id;

    if (out_activity) {
        memset(out_activity, 0, sizeof(*out_activity));
    }
    if (!plan || !result || !out_activity || value_id >= result->value_count || result->value_count != plan->value_count) {
        return 0;
    }

    root_value_id = value_ssa_alloc_plan_family_root_for_value(plan, value_id);
    out_activity->root_value_id = root_value_id;
    for (member_value_id = 0; member_value_id < result->value_count; ++member_value_id) {
        if (value_ssa_alloc_plan_family_root_for_value(plan, member_value_id) != root_value_id) {
            continue;
        }
        if (result->spill_flags && result->spill_flags[member_value_id]) {
            ++out_activity->spilled_member_count;
            continue;
        }
        if (result->has_color && result->has_color[member_value_id]) {
            ++out_activity->colored_member_count;
        }
    }

    return 1;
}

static int value_ssa_alloc_collect_runtime_family_color_support(
    const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    size_t color,
    int use_analysis_fallback,
    int include_self,
    ValueSsaAllocatorRuntimeFamilyColorSupport *out_support) {
    size_t member_value_id;
    size_t root_value_id;

    if (out_support) {
        memset(out_support, 0, sizeof(*out_support));
        out_support->best_member_value_id = (size_t)-1;
        out_support->best_direct_partner_value_id = (size_t)-1;
        out_support->color = color;
    }
    if (!plan || !result || !out_support || value_id >= result->value_count || result->value_count != plan->value_count) {
        return 0;
    }

    root_value_id = use_analysis_fallback
        ? value_ssa_alloc_runtime_family_root_for_value(plan, coalesce_analysis, result->value_count, value_id)
        : value_ssa_alloc_plan_family_root_for_value(plan, value_id);
    out_support->root_value_id = root_value_id;
    for (member_value_id = 0; member_value_id < result->value_count; ++member_value_id) {
        const ValueSsaAllocatorCoalesceItem *direct_item = NULL;
        size_t direct_weight = 0;

        if ((use_analysis_fallback
                ? value_ssa_alloc_runtime_family_root_for_value(
                    plan, coalesce_analysis, result->value_count, member_value_id)
                : value_ssa_alloc_plan_family_root_for_value(plan, member_value_id)) != root_value_id) {
            continue;
        }
        if (!include_self && member_value_id == value_id) {
            continue;
        }
        if (!result->has_color || !result->has_color[member_value_id] || result->spill_flags[member_value_id] ||
            result->colors[member_value_id] != color) {
            continue;
        }

        ++out_support->colored_member_count;
        if (out_support->best_member_value_id == (size_t)-1 || member_value_id < out_support->best_member_value_id) {
            out_support->best_member_value_id = member_value_id;
        }
        if (member_value_id != value_id && coalesce_analysis &&
            value_ssa_allocator_coalesce_analysis_find_pair(
                coalesce_analysis, value_id, member_value_id, NULL, &direct_item) &&
            direct_item && direct_item->can_coalesce) {
            direct_weight = direct_item->weight;
        }
        out_support->direct_coalesce_weight_sum += direct_weight;
        if (member_value_id != value_id &&
            (out_support->best_direct_partner_value_id == (size_t)-1 ||
            direct_weight > out_support->best_direct_partner_weight ||
            (direct_weight == out_support->best_direct_partner_weight &&
                member_value_id < out_support->best_direct_partner_value_id))) {
            out_support->best_direct_partner_value_id = member_value_id;
            out_support->best_direct_partner_weight = direct_weight;
        }
    }

    return 1;
}

static int value_ssa_alloc_compare_runtime_family_activity_values(const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocationResult *result,
    size_t lhs_value_id,
    size_t rhs_value_id,
    int *out_compare) {
    ValueSsaAllocatorRuntimeFamilyActivity lhs_activity;
    ValueSsaAllocatorRuntimeFamilyActivity rhs_activity;

    if (out_compare) {
        *out_compare = 0;
    }
    if (!plan || !result || !out_compare ||
        !value_ssa_alloc_collect_runtime_family_activity(plan, result, lhs_value_id, &lhs_activity) ||
        !value_ssa_alloc_collect_runtime_family_activity(plan, result, rhs_value_id, &rhs_activity)) {
        return 0;
    }

    if (lhs_activity.colored_member_count != rhs_activity.colored_member_count) {
        *out_compare = lhs_activity.colored_member_count < rhs_activity.colored_member_count ? -1 : 1;
        return 1;
    }
    if (lhs_activity.spilled_member_count != rhs_activity.spilled_member_count) {
        *out_compare = lhs_activity.spilled_member_count > rhs_activity.spilled_member_count ? -1 : 1;
        return 1;
    }
    return 1;
}

static size_t value_ssa_alloc_cross_block_spill_penalty(const ValueSsaAllocationPrep *prep, size_t value_id) {
    if (!prep || value_id >= prep->value_count || !prep->single_block_live_ranges) {
        return 0;
    }
    return prep->single_block_live_ranges[value_id] ? 0 : 1;
}

static void value_ssa_alloc_set_error(ValueSsaError *error, int line, int column, const char *fmt, ...) {
    va_list args;

    if (!error || !fmt) {
        return;
    }

    error->line = line;
    error->column = column;
    va_start(args, fmt);
    vsnprintf(error->message, sizeof(error->message), fmt, args);
    va_end(args);
}

static int value_ssa_alloc_next_growth_capacity(size_t current,
    size_t initial,
    size_t elem_size,
    size_t *out_next_capacity) {
    size_t next_capacity;

    if (!out_next_capacity || initial == 0 || elem_size == 0) {
        return 0;
    }

    if (current == 0) {
        next_capacity = initial;
    } else {
        if (current > ((size_t)-1) / 2) {
            return 0;
        }
        next_capacity = current * 2;
    }

    if (next_capacity > ((size_t)-1) / elem_size) {
        return 0;
    }

    *out_next_capacity = next_capacity;
    return 1;
}

static int value_ssa_alloc_prepare_result(ValueSsaAllocationResult *result,
    size_t value_count,
    size_t color_budget,
    ValueSsaError *error) {
    size_t alloc_count;
    size_t index;

    if (!result) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-001: invalid allocation result");
        return 0;
    }

    value_ssa_allocation_result_free(result);
    result->value_count = value_count;
    result->color_budget = color_budget;
    alloc_count = value_count ? value_count : 1;
    result->colors = (size_t *)malloc(alloc_count * sizeof(size_t));
    result->has_color = (unsigned char *)calloc(alloc_count, sizeof(unsigned char));
    result->spill_flags = (unsigned char *)calloc(alloc_count, sizeof(unsigned char));
    result->spill_intended_flags = (unsigned char *)calloc(alloc_count, sizeof(unsigned char));
    result->spill_confirmed_flags = (unsigned char *)calloc(alloc_count, sizeof(unsigned char));
    result->spill_recovered_flags = (unsigned char *)calloc(alloc_count, sizeof(unsigned char));
    result->spill_recovery_orders = (size_t *)malloc(alloc_count * sizeof(size_t));
    result->spill_recovery_phase_ids = (size_t *)malloc(alloc_count * sizeof(size_t));
    result->spill_recovery_phase_member_orders = (size_t *)malloc(alloc_count * sizeof(size_t));
    result->spill_recovery_phase_member_counts = (size_t *)malloc(alloc_count * sizeof(size_t));
    result->spill_recovery_phase_entries =
        (ValueSsaAllocatorRetryFamilyAgendaItem *)malloc(alloc_count * sizeof(ValueSsaAllocatorRetryFamilyAgendaItem));
    result->spill_priorities = (size_t *)calloc(alloc_count, sizeof(size_t));
    result->spill_slots = (size_t *)malloc(alloc_count * sizeof(size_t));
    result->used_preferred_color_flags = (unsigned char *)calloc(alloc_count, sizeof(unsigned char));
    result->preferred_color_sources =
        (ValueSsaAllocationPreferredColorSource *)calloc(alloc_count, sizeof(ValueSsaAllocationPreferredColorSource));
    result->preferred_color_partner_value_ids = (size_t *)malloc(alloc_count * sizeof(size_t));
    result->used_generic_eviction_flags = (unsigned char *)calloc(alloc_count, sizeof(unsigned char));
    result->generic_eviction_blocker_value_ids = (size_t *)malloc(alloc_count * sizeof(size_t));
    result->generic_eviction_blocker_recolored_flags = (unsigned char *)calloc(alloc_count, sizeof(unsigned char));
    result->generic_eviction_blocker_new_colors = (size_t *)malloc(alloc_count * sizeof(size_t));
    result->used_targeted_eviction_flags = (unsigned char *)calloc(alloc_count, sizeof(unsigned char));
    result->targeted_eviction_blocker_value_ids = (size_t *)malloc(alloc_count * sizeof(size_t));
    result->targeted_eviction_blocker_recolored_flags = (unsigned char *)calloc(alloc_count, sizeof(unsigned char));
    result->targeted_eviction_blocker_new_colors = (size_t *)malloc(alloc_count * sizeof(size_t));
    result->used_retry_eviction_flags = (unsigned char *)calloc(alloc_count, sizeof(unsigned char));
    result->retry_eviction_blocker_value_ids = (size_t *)malloc(alloc_count * sizeof(size_t));
    result->retry_eviction_blocker_recolored_flags = (unsigned char *)calloc(alloc_count, sizeof(unsigned char));
    result->retry_eviction_blocker_new_colors = (size_t *)malloc(alloc_count * sizeof(size_t));
    if (!result->colors || !result->has_color || !result->spill_flags ||
        !result->spill_intended_flags || !result->spill_confirmed_flags || !result->spill_recovered_flags ||
        !result->spill_recovery_orders || !result->spill_recovery_phase_ids ||
        !result->spill_recovery_phase_member_orders || !result->spill_recovery_phase_member_counts ||
        !result->spill_recovery_phase_entries ||
        !result->spill_priorities || !result->spill_slots || !result->used_preferred_color_flags ||
        !result->preferred_color_sources || !result->preferred_color_partner_value_ids ||
        !result->used_generic_eviction_flags || !result->generic_eviction_blocker_value_ids ||
        !result->generic_eviction_blocker_recolored_flags || !result->generic_eviction_blocker_new_colors ||
        !result->used_targeted_eviction_flags || !result->targeted_eviction_blocker_value_ids ||
        !result->targeted_eviction_blocker_recolored_flags || !result->targeted_eviction_blocker_new_colors ||
        !result->used_retry_eviction_flags || !result->retry_eviction_blocker_value_ids ||
        !result->retry_eviction_blocker_recolored_flags || !result->retry_eviction_blocker_new_colors) {
        value_ssa_allocation_result_free(result);
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-002: out of memory preparing result");
        return 0;
    }

    for (index = 0; index < alloc_count; ++index) {
        result->colors[index] = (size_t)-1;
        result->spill_recovery_orders[index] = (size_t)-1;
        result->spill_recovery_phase_ids[index] = (size_t)-1;
        result->spill_recovery_phase_member_orders[index] = (size_t)-1;
        result->spill_recovery_phase_member_counts[index] = (size_t)-1;
        value_ssa_alloc_retry_family_agenda_entry_clear(&result->spill_recovery_phase_entries[index]);
        result->spill_slots[index] = (size_t)-1;
        result->preferred_color_partner_value_ids[index] = (size_t)-1;
        result->generic_eviction_blocker_value_ids[index] = (size_t)-1;
        result->generic_eviction_blocker_new_colors[index] = (size_t)-1;
        result->targeted_eviction_blocker_value_ids[index] = (size_t)-1;
        result->targeted_eviction_blocker_new_colors[index] = (size_t)-1;
        result->retry_eviction_blocker_value_ids[index] = (size_t)-1;
        result->retry_eviction_blocker_new_colors[index] = (size_t)-1;
    }
    result->next_spill_recovery_order = 0;
    result->next_spill_recovery_phase_id = 0;

    return 1;
}

static int value_ssa_alloc_count_runtime_family_members_on_color(const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocationResult *result,
    size_t value_id,
    size_t color,
    size_t *out_count) {
    ValueSsaAllocatorRuntimeFamilyColorSupport support;

    if (out_count) {
        *out_count = 0;
    }
    if (!plan || !result || !out_count || value_id >= result->value_count || result->value_count != plan->value_count) {
        return 0;
    }
    if (!value_ssa_alloc_collect_runtime_family_color_support(
            plan, coalesce_analysis, result, value_id, color, 0, 1, &support)) {
        return 0;
    }

    *out_count = support.colored_member_count;
    return 1;
}

static int value_ssa_alloc_compare_runtime_family_color_support(const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocationResult *result,
    size_t lhs_value_id,
    size_t lhs_color,
    size_t rhs_value_id,
    size_t rhs_color,
    int *out_compare) {
    ValueSsaAllocatorRuntimeFamilyColorSupport lhs_support;
    ValueSsaAllocatorRuntimeFamilyColorSupport rhs_support;

    if (out_compare) {
        *out_compare = 0;
    }
    if (!plan || !result || !out_compare ||
        !value_ssa_alloc_collect_runtime_family_color_support(
            plan, coalesce_analysis, result, lhs_value_id, lhs_color, 1, 1, &lhs_support) ||
        !value_ssa_alloc_collect_runtime_family_color_support(
            plan, coalesce_analysis, result, rhs_value_id, rhs_color, 1, 1, &rhs_support)) {
        return 0;
    }

    if (lhs_support.colored_member_count != rhs_support.colored_member_count) {
        *out_compare = lhs_support.colored_member_count < rhs_support.colored_member_count ? -1 : 1;
    }
    return 1;
}

static int value_ssa_alloc_compare_weighted_runtime_family_color_support(const ValueSsaAllocatorPlan *plan,
    const ValueSsaAllocatorCoalesceAnalysis *coalesce_analysis,
    const ValueSsaAllocationResult *result,
    size_t lhs_value_id,
    size_t lhs_color,
    size_t rhs_value_id,
    size_t rhs_color,
    int *out_compare) {
    ValueSsaAllocatorRuntimeFamilyColorSupport lhs_support;
    ValueSsaAllocatorRuntimeFamilyColorSupport rhs_support;

    if (out_compare) {
        *out_compare = 0;
    }
    if (!plan || !result || !out_compare ||
        !value_ssa_alloc_collect_runtime_family_color_support(
            plan, coalesce_analysis, result, lhs_value_id, lhs_color, 0, 1, &lhs_support) ||
        !value_ssa_alloc_collect_runtime_family_color_support(
            plan, coalesce_analysis, result, rhs_value_id, rhs_color, 0, 1, &rhs_support)) {
        return 0;
    }

    if (lhs_support.colored_member_count != rhs_support.colored_member_count) {
        *out_compare = lhs_support.colored_member_count < rhs_support.colored_member_count ? -1 : 1;
        return 1;
    }
    if (lhs_support.direct_coalesce_weight_sum != rhs_support.direct_coalesce_weight_sum) {
        *out_compare = lhs_support.direct_coalesce_weight_sum < rhs_support.direct_coalesce_weight_sum ? -1 : 1;
        return 1;
    }
    if (lhs_support.best_direct_partner_weight != rhs_support.best_direct_partner_weight) {
        *out_compare = lhs_support.best_direct_partner_weight < rhs_support.best_direct_partner_weight ? -1 : 1;
    }
    return 1;
}

int value_ssa_allocation_result_get_color(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_has_color,
    size_t *out_color) {
    if (!result || !out_has_color || !out_color || value_id >= result->value_count) {
        return 0;
    }

    *out_has_color = (int)result->has_color[value_id];
    *out_color = *out_has_color ? result->colors[value_id] : 0;
    return 1;
}

int value_ssa_allocation_result_is_spilled(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_is_spilled) {
    if (!result || !out_is_spilled || value_id >= result->value_count) {
        return 0;
    }

    *out_is_spilled = (int)result->spill_flags[value_id];
    return 1;
}

int value_ssa_allocation_result_is_spill_intended(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_is_spill_intended) {
    if (!result || !out_is_spill_intended || value_id >= result->value_count) {
        return 0;
    }

    *out_is_spill_intended = result->spill_intended_flags ? (int)result->spill_intended_flags[value_id] : 0;
    return 1;
}

int value_ssa_allocation_result_is_spill_confirmed(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_is_spill_confirmed) {
    if (!result || !out_is_spill_confirmed || value_id >= result->value_count) {
        return 0;
    }

    *out_is_spill_confirmed = result->spill_confirmed_flags ? (int)result->spill_confirmed_flags[value_id] : 0;
    return 1;
}

int value_ssa_allocation_result_is_spill_recovered(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_is_spill_recovered) {
    if (!result || !out_is_spill_recovered || value_id >= result->value_count) {
        return 0;
    }

    *out_is_spill_recovered = result->spill_recovered_flags ? (int)result->spill_recovered_flags[value_id] : 0;
    return 1;
}

int value_ssa_allocation_result_get_spill_recovery_order(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_has_recovery_order,
    size_t *out_recovery_order) {
    if (!result || !out_has_recovery_order || !out_recovery_order || value_id >= result->value_count) {
        return 0;
    }

    *out_has_recovery_order = result->spill_recovery_orders && result->spill_recovery_orders[value_id] != (size_t)-1;
    *out_recovery_order = *out_has_recovery_order ? result->spill_recovery_orders[value_id] : 0;
    return 1;
}

int value_ssa_allocation_result_get_spill_recovery_phase(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_has_recovery_phase,
    size_t *out_recovery_phase_id,
    size_t *out_recovery_family_root_value_id) {
    if (!result || !out_has_recovery_phase || !out_recovery_phase_id || !out_recovery_family_root_value_id ||
        value_id >= result->value_count) {
        return 0;
    }

    *out_has_recovery_phase = result->spill_recovery_phase_ids &&
        result->spill_recovery_phase_ids[value_id] != (size_t)-1 &&
        result->spill_recovery_phase_entries &&
        result->spill_recovery_phase_entries[value_id].root_value_id != (size_t)-1;
    *out_recovery_phase_id = *out_has_recovery_phase ? result->spill_recovery_phase_ids[value_id] : 0;
    *out_recovery_family_root_value_id =
        *out_has_recovery_phase ? result->spill_recovery_phase_entries[value_id].root_value_id : 0;
    return 1;
}

int value_ssa_allocation_result_get_spill_recovery_phase_member_order(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_has_phase_member_order,
    size_t *out_phase_member_order) {
    if (!result || !out_has_phase_member_order || !out_phase_member_order || value_id >= result->value_count) {
        return 0;
    }

    *out_has_phase_member_order = result->spill_recovery_phase_member_orders &&
        result->spill_recovery_phase_member_orders[value_id] != (size_t)-1;
    *out_phase_member_order =
        *out_has_phase_member_order ? result->spill_recovery_phase_member_orders[value_id] : 0;
    return 1;
}

int value_ssa_allocation_result_get_spill_recovery_phase_member_count(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_has_phase_member_count,
    size_t *out_phase_member_count) {
    if (!result || !out_has_phase_member_count || !out_phase_member_count || value_id >= result->value_count) {
        return 0;
    }

    *out_has_phase_member_count = result->spill_recovery_phase_member_counts &&
        result->spill_recovery_phase_member_counts[value_id] != (size_t)-1;
    *out_phase_member_count =
        *out_has_phase_member_count ? result->spill_recovery_phase_member_counts[value_id] : 0;
    return 1;
}

int value_ssa_allocation_result_get_spill_recovery_phase_entry(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_has_phase_entry,
    size_t *out_entry_value_id,
    size_t *out_entry_recoverable_count,
    size_t *out_entry_intended_count) {
    if (!result || !out_has_phase_entry || !out_entry_value_id || !out_entry_recoverable_count ||
        !out_entry_intended_count || value_id >= result->value_count) {
        return 0;
    }

    *out_has_phase_entry = result->spill_recovery_phase_entries &&
        result->spill_recovery_phase_entries[value_id].representative_value_id != (size_t)-1 &&
        result->spill_recovery_phase_entries[value_id].recoverable_member_count != (size_t)-1 &&
        result->spill_recovery_phase_entries[value_id].recoverable_intended_count != (size_t)-1;
    *out_entry_value_id =
        *out_has_phase_entry ? result->spill_recovery_phase_entries[value_id].representative_value_id : 0;
    *out_entry_recoverable_count =
        *out_has_phase_entry ? result->spill_recovery_phase_entries[value_id].recoverable_member_count : 0;
    *out_entry_intended_count =
        *out_has_phase_entry ? result->spill_recovery_phase_entries[value_id].recoverable_intended_count : 0;
    return 1;
}

int value_ssa_allocation_result_get_spill_recovery_phase_entry_split_family(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_has_phase_entry,
    size_t *out_split_family_root_value_id,
    size_t *out_split_family_member_count,
    size_t *out_split_family_intended_count) {
    if (!result || !out_has_phase_entry || !out_split_family_root_value_id || !out_split_family_member_count ||
        !out_split_family_intended_count || value_id >= result->value_count) {
        return 0;
    }

    *out_has_phase_entry = result->spill_recovery_phase_entries &&
        result->spill_recovery_phase_entries[value_id].representative_value_id != (size_t)-1 &&
        result->spill_recovery_phase_entries[value_id].split_family_root_value_id != (size_t)-1;
    *out_split_family_root_value_id =
        *out_has_phase_entry ? result->spill_recovery_phase_entries[value_id].split_family_root_value_id : 0;
    *out_split_family_member_count =
        *out_has_phase_entry ? result->spill_recovery_phase_entries[value_id].split_family_member_count : 0;
    *out_split_family_intended_count =
        *out_has_phase_entry ? result->spill_recovery_phase_entries[value_id].split_family_intended_count : 0;
    return 1;
}

int value_ssa_allocation_result_used_retry_eviction(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_used_retry_eviction,
    size_t *out_blocker_value_id) {
    if (!result || !out_used_retry_eviction || !out_blocker_value_id || value_id >= result->value_count) {
        return 0;
    }

    *out_used_retry_eviction = result->used_retry_eviction_flags ? (int)result->used_retry_eviction_flags[value_id] : 0;
    *out_blocker_value_id = (*out_used_retry_eviction && result->retry_eviction_blocker_value_ids)
        ? result->retry_eviction_blocker_value_ids[value_id]
        : 0;
    return 1;
}

int value_ssa_allocation_result_get_targeted_eviction_outcome(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_used_targeted_eviction,
    size_t *out_blocker_value_id,
    int *out_blocker_recolored,
    size_t *out_blocker_new_color) {
    if (!result || !out_used_targeted_eviction || !out_blocker_value_id || !out_blocker_recolored ||
        !out_blocker_new_color || value_id >= result->value_count) {
        return 0;
    }

    *out_used_targeted_eviction =
        result->used_targeted_eviction_flags ? (int)result->used_targeted_eviction_flags[value_id] : 0;
    *out_blocker_value_id = (*out_used_targeted_eviction && result->targeted_eviction_blocker_value_ids)
        ? result->targeted_eviction_blocker_value_ids[value_id]
        : 0;
    *out_blocker_recolored =
        (*out_used_targeted_eviction && result->targeted_eviction_blocker_recolored_flags)
        ? (int)result->targeted_eviction_blocker_recolored_flags[value_id]
        : 0;
    *out_blocker_new_color =
        (*out_used_targeted_eviction && *out_blocker_recolored && result->targeted_eviction_blocker_new_colors &&
            result->targeted_eviction_blocker_new_colors[value_id] != (size_t)-1)
        ? result->targeted_eviction_blocker_new_colors[value_id]
        : 0;
    return 1;
}

int value_ssa_allocation_result_get_generic_eviction_outcome(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_used_generic_eviction,
    size_t *out_blocker_value_id,
    int *out_blocker_recolored,
    size_t *out_blocker_new_color) {
    if (!result || !out_used_generic_eviction || !out_blocker_value_id || !out_blocker_recolored ||
        !out_blocker_new_color || value_id >= result->value_count) {
        return 0;
    }

    *out_used_generic_eviction =
        result->used_generic_eviction_flags ? (int)result->used_generic_eviction_flags[value_id] : 0;
    *out_blocker_value_id = (*out_used_generic_eviction && result->generic_eviction_blocker_value_ids)
        ? result->generic_eviction_blocker_value_ids[value_id]
        : 0;
    *out_blocker_recolored =
        (*out_used_generic_eviction && result->generic_eviction_blocker_recolored_flags)
        ? (int)result->generic_eviction_blocker_recolored_flags[value_id]
        : 0;
    *out_blocker_new_color =
        (*out_used_generic_eviction && *out_blocker_recolored && result->generic_eviction_blocker_new_colors &&
            result->generic_eviction_blocker_new_colors[value_id] != (size_t)-1)
        ? result->generic_eviction_blocker_new_colors[value_id]
        : 0;
    return 1;
}

int value_ssa_allocation_result_get_retry_eviction_outcome(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_used_retry_eviction,
    size_t *out_blocker_value_id,
    int *out_blocker_recolored,
    size_t *out_blocker_new_color) {
    if (!result || !out_used_retry_eviction || !out_blocker_value_id || !out_blocker_recolored ||
        !out_blocker_new_color || value_id >= result->value_count) {
        return 0;
    }

    *out_used_retry_eviction = result->used_retry_eviction_flags ? (int)result->used_retry_eviction_flags[value_id] : 0;
    *out_blocker_value_id = (*out_used_retry_eviction && result->retry_eviction_blocker_value_ids)
        ? result->retry_eviction_blocker_value_ids[value_id]
        : 0;
    *out_blocker_recolored =
        (*out_used_retry_eviction && result->retry_eviction_blocker_recolored_flags)
        ? (int)result->retry_eviction_blocker_recolored_flags[value_id]
        : 0;
    *out_blocker_new_color =
        (*out_used_retry_eviction && *out_blocker_recolored && result->retry_eviction_blocker_new_colors &&
            result->retry_eviction_blocker_new_colors[value_id] != (size_t)-1)
        ? result->retry_eviction_blocker_new_colors[value_id]
        : 0;
    return 1;
}

int value_ssa_allocation_result_get_spill_priority(const ValueSsaAllocationResult *result,
    size_t value_id,
    size_t *out_priority) {
    if (!result || !out_priority || value_id >= result->value_count) {
        return 0;
    }

    *out_priority = result->spill_priorities[value_id];
    return 1;
}

int value_ssa_allocation_result_get_spill_slot(const ValueSsaAllocationResult *result,
    size_t value_id,
    int *out_has_spill_slot,
    size_t *out_spill_slot) {
    if (!result || !out_has_spill_slot || !out_spill_slot || value_id >= result->value_count) {
        return 0;
    }

    *out_has_spill_slot = result->spill_slots[value_id] != (size_t)-1;
    *out_spill_slot = *out_has_spill_slot ? result->spill_slots[value_id] : 0;
    return 1;
}

int value_ssa_allocation_result_count_spilled_values(const ValueSsaAllocationResult *result, size_t *out_count) {
    ValueSsaFunctionAllocationLayout layout;
    size_t spilled_count = 0;

    if (!result || !out_count) {
        return 0;
    }

    value_ssa_function_allocation_layout_init(&layout);
    if (!value_ssa_build_function_allocation_layout(result, &layout, NULL)) {
        return 0;
    }
    if (!value_ssa_function_allocation_layout_get_summary(
            &layout, NULL, NULL, NULL, NULL, NULL, &spilled_count, NULL, NULL, NULL, NULL, NULL, NULL)) {
        value_ssa_function_allocation_layout_free(&layout);
        return 0;
    }
    *out_count = spilled_count;
    value_ssa_function_allocation_layout_free(&layout);
    return 1;
}

int value_ssa_allocation_result_count_colored_values(const ValueSsaAllocationResult *result, size_t *out_count) {
    ValueSsaFunctionAllocationLayout layout;
    size_t colored_count = 0;

    if (!result || !out_count) {
        return 0;
    }

    value_ssa_function_allocation_layout_init(&layout);
    if (!value_ssa_build_function_allocation_layout(result, &layout, NULL)) {
        return 0;
    }
    if (!value_ssa_function_allocation_layout_get_summary(
            &layout, NULL, NULL, NULL, NULL, &colored_count, NULL, NULL, NULL, NULL, NULL, NULL, NULL)) {
        value_ssa_function_allocation_layout_free(&layout);
        return 0;
    }
    *out_count = colored_count;
    value_ssa_function_allocation_layout_free(&layout);
    return 1;
}

int value_ssa_compute_allocation_select_stats(const ValueSsaAllocationResult *result,
    ValueSsaAllocationSelectStats *out_stats) {
    size_t value_id;

    if (!result || !out_stats) {
        return 0;
    }

    value_ssa_allocation_select_stats_init(out_stats);
    for (value_id = 0; value_id < result->value_count; ++value_id) {
        if (result->has_color[value_id]) {
            ++out_stats->colored_count;
        }
        if (result->spill_flags[value_id]) {
            ++out_stats->spilled_count;
        }
        if (result->spill_intended_flags && result->spill_intended_flags[value_id]) {
            ++out_stats->spill_intended_count;
            if (result->has_color[value_id] &&
                (!result->spill_confirmed_flags || !result->spill_confirmed_flags[value_id])) {
                if (result->spill_recovered_flags && result->spill_recovered_flags[value_id]) {
                    ++out_stats->optimistic_recovered_count;
                } else {
                    ++out_stats->optimistic_direct_count;
                }
                ++out_stats->optimistic_colored_count;
            }
        }
        if (result->spill_confirmed_flags && result->spill_confirmed_flags[value_id]) {
            ++out_stats->spill_confirmed_count;
        }
        if (result->spill_recovered_flags && result->spill_recovered_flags[value_id]) {
            ++out_stats->spill_recovered_count;
            if (result->used_retry_eviction_flags && result->used_retry_eviction_flags[value_id]) {
                ++out_stats->retry_eviction_recovered_count;
            }
        }
    }

    return 1;
}

static int value_ssa_alloc_insert_instruction(ValueSsaBasicBlock *block,
    size_t instruction_index,
    const ValueSsaInstruction *instruction,
    ValueSsaError *error) {
    size_t next_capacity;
    ValueSsaInstruction *instructions;
    size_t move_count;

    if (!block || !instruction || instruction_index > block->instruction_count) {
        value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-026: invalid instruction insertion");
        return 0;
    }

    if (block->instruction_count == block->instruction_capacity) {
        if (!value_ssa_alloc_next_growth_capacity(
                block->instruction_capacity, 4, sizeof(ValueSsaInstruction), &next_capacity)) {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-027: instruction table capacity overflow");
            return 0;
        }
        instructions = (ValueSsaInstruction *)realloc(block->instructions, next_capacity * sizeof(ValueSsaInstruction));
        if (!instructions) {
            value_ssa_alloc_set_error(error, 0, 0, "VALUE-SSA-ALLOC-028: out of memory growing instruction table");
            return 0;
        }
        block->instructions = instructions;
        block->instruction_capacity = next_capacity;
    }

    move_count = block->instruction_count - instruction_index;
    if (move_count > 0) {
        memmove(&block->instructions[instruction_index + 1],
            &block->instructions[instruction_index],
            move_count * sizeof(ValueSsaInstruction));
    }
    memcpy(&block->instructions[instruction_index], instruction, sizeof(ValueSsaInstruction));
    ++block->instruction_count;
    return 1;
}

static int value_ssa_alloc_append_format(ValueSsaAllocStringBuilder *builder, const char *fmt, ...) {
    va_list args;
    va_list measure_args;
    int written;
    size_t required;
    size_t next_capacity;
    char *next_data;

    if (!builder || !fmt) {
        return 0;
    }

    va_start(args, fmt);
    va_copy(measure_args, args);
    written = vsnprintf(NULL, 0, fmt, measure_args);
    va_end(measure_args);
    if (written < 0) {
        va_end(args);
        return 0;
    }

    required = builder->length + (size_t)written + 1;
    if (required > builder->capacity) {
        next_capacity = builder->capacity;
        while (required > next_capacity) {
            if (!value_ssa_alloc_next_growth_capacity(
                    next_capacity, 64, sizeof(char), &next_capacity)) {
                va_end(args);
                return 0;
            }
        }
        next_data = (char *)realloc(builder->data, next_capacity * sizeof(char));
        if (!next_data) {
            va_end(args);
            return 0;
        }
        builder->data = next_data;
        builder->capacity = next_capacity;
    }

    if (vsnprintf(builder->data + builder->length, required - builder->length, fmt, args) != written) {
        va_end(args);
        return 0;
    }
    va_end(args);
    builder->length += (size_t)written;
    return 1;
}

#endif
