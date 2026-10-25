#include <assert.h>
#include <stdio.h>
#include "kernel/tu102_flow.h"
struct sim { int fail, q, h, release_error, released, calls; };
static int step(void *p,enum tu102_phase phase) { struct sim *s=p; s->calls++; return (int)phase==s->fail?-99:0; }
static int quiet(void *p) { return ((struct sim *)p)->q; }
static int handoff(void *p) { return ((struct sim *)p)->h; }
static int release(void *p) { struct sim *s=p; s->released++; return s->release_error; }
int main(void) {
    int count=0;
    for(int fail=-1;fail<TU_PHASE_COUNT;fail++)
    for(int q=0;q<=1;q++) for(int h=0;h<=1;h++) for(int cleanup=0;cleanup<=1;cleanup++) {
        struct sim s={.fail=fail,.q=q,.h=h,.release_error=cleanup};
        struct tu102_flow_ops ops={&s,step,quiet,handoff,release};
        struct tu102_flow_result r=tu102_flow_run(&ops,1);
        int mutated=fail<0 || fail>=TU_FWSEC_RESET;
        if(mutated && (!q || !h)) assert(r.outcome==TU_RETAIN_STOP && s.released==0);
        else if(cleanup) assert(r.outcome==TU_RETAIN_STOP && s.released==1);
        else assert(r.outcome==(fail<0?TU_SUCCESS:TU_SAFE_FAILURE) && s.released==1);
        assert(s.calls==(fail<0?TU_PHASE_COUNT:fail+1));
        count++;
    }
    struct sim s={.fail=-1};
    struct tu102_flow_ops ops={&s,step,quiet,handoff,release};
    struct tu102_flow_result r=tu102_flow_run(&ops,0);
    assert(r.outcome==TU_PREPARED && !r.mutation_possible && s.calls==TU_FWSEC_RESET);
    assert(tu102_flow_run(NULL,1).outcome==TU_RETAIN_STOP);
    printf("PASS %d failure-policy combinations and non-executing boundary\n",count);
    return 0;
}
