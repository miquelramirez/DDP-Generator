#include <cmath>

#include "ddp.h"
#include "line_search.h"
 
// return number of line searches if successfull, -1 if max number of line searches reached, -2 if inf or nan encountered
int line_search(tOptSet *o) {
    double expected= 0.0, z= 0.0, alpha= 0.0, dcost= 0.0, cnew= 0.0;
    int i, ret= -1;
    
    for(i= 0; i < o->n_alpha; i++) {
        if(o->log_line) o->log_line->n_line_searches= i+1;
        
        alpha= o->alpha[i];
        
        if(forward_pass(o->candidates[0], o, alpha, cnew, 0)) {
            dcost= o->cost - cnew;
            expected= -alpha*(o->dV[0] + alpha*o->dV[1]);
            if(expected > 0)
                z = dcost/expected;
            else {
                z= 0;
                if(o->log_line) o->log_line->neg_exp_red++;
            }

            if(z > o->zMin) {
                ret= 1;
                break;
            }
        } else {
            ret= -2;
            break;
        }
    }
    
    if(ret>-2 && o->log_line) {
        o->log_line->alpha= alpha;
        o->log_line->z= z;
        o->log_line->cost= cnew;
        o->log_line->dcost= dcost;
        o->log_line->expected_red= expected;
    }
    
    o->new_cost= cnew;
    o->dcost= dcost;
    o->last_z= z;
    o->n_ls= i+1;

    return ret;
}
