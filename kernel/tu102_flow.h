/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef TU102_FLOW_H
#define TU102_FLOW_H
/* Host-testable sequencing contract; no hardware callbacks are supplied here. */
enum tu102_phase {
    TU_ID, TU_ASSETS, TU_LAYOUT, TU_DMA, TU_TABLES,
    TU_FWSEC_RESET, TU_FWSEC_START, TU_FWSEC_VERIFY,
    TU_BOOTER_RESET, TU_BOOTER_START, TU_SELECTORS, TU_HANDOFF,
    TU_PHASE_COUNT
};
enum tu102_outcome { TU_PREPARED, TU_SUCCESS, TU_SAFE_FAILURE, TU_RETAIN_STOP };
struct tu102_flow_ops {
    void *ctx;
    int (*step)(void *, enum tu102_phase);
    int (*quiescent)(void *);
    int (*handoff_verified)(void *);
    int (*release)(void *);
};
struct tu102_flow_result {
    enum tu102_phase phase;
    enum tu102_outcome outcome;
    int error;
    int mutation_possible;
};
static inline struct tu102_flow_result tu102_flow_run(struct tu102_flow_ops *ops, int execute)
{
    struct tu102_flow_result r = { TU_ID, TU_RETAIN_STOP, -1, 0 };
    int phase, stop = execute ? TU_PHASE_COUNT : TU_FWSEC_RESET;
    if (!ops || !ops->step || !ops->release || !ops->quiescent || !ops->handoff_verified)
        return r;
    for (phase = TU_ID; phase < stop; phase++) {
        r.phase = (enum tu102_phase)phase;
        if (phase >= TU_FWSEC_RESET) r.mutation_possible = 1;
        r.error = ops->step(ops->ctx, r.phase);
        if (r.error) break;
    }
    if (r.mutation_possible &&
        (ops->quiescent(ops->ctx) != 1 || ops->handoff_verified(ops->ctx) != 1)) {
        r.outcome = TU_RETAIN_STOP;
        if (!r.error) r.error = -1;
        return r;
    }
    if (ops->release(ops->ctx)) {
        r.outcome = TU_RETAIN_STOP;
        if (!r.error) r.error = -1;
        return r;
    }
    r.outcome = r.error ? TU_SAFE_FAILURE : (execute ? TU_SUCCESS : TU_PREPARED);
    return r;
}
#endif
