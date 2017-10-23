#ifndef ILQG_H
#define ILQG_H

#include <stdio.h>

#ifndef FULL_DDP
    #define FULL_DDP 1
#endif

#ifndef CONSTRAINT_UX
    #define CONSTRAINT_UX 1
#endif

#include "iLQG_problem.h"

#define INIT_OPTSET {0}

#ifndef PRNT
#define PRNT printf
#endif

#ifndef NUMBER_OF_THREADS
#define NUMBER_OF_THREADS 1
#endif

#define DO_PREFIX1(VAL)  1 ## VAL
#define PREFIX1(VAL)     DO_PREFIX1(VAL)


typedef struct paramDesc {
  const char *name;
  const int size;
  const int is_var;
} tParamDesc;

typedef struct logLine {
    int new_deriv;
    int derivs_fail;
    int n_back_pass;
    double w_pen_l;
    double w_pen_f;
    double lambda;
    int back_pass_failed;
    int qp_res;
    double g_norm;
    int forward_pass_fail;
    int n_line_searches;
    int neg_exp_red;
    double alpha;
    double z;
    double cost;
    double dcost;
    double expected_red;
    int line_search_res;
    int res;
} tLogLine;

typedef struct optSet {
    int n_hor;
    VectorX x0;
    double new_cost, cost, dcost, lambda, g_norm;
    double **p;
    const double *alpha;
    int n_alpha;
    double lambdaMax;
    double lambdaMin;
    double lambdaInit;
    double dlambdaInit;
    double lambdaFactor;
    int max_iter;
    double tolGrad;
    double tolFun;
    double tolConstraint;
    double zMin;
    int regType;
    double dV[2];
    
    double w_pen_l;
    double w_pen_f;
    double w_pen_max_l;
    double w_pen_max_f;
    double w_pen_init_l;
    double w_pen_init_f;
    double w_pen_fact1;
    double w_pen_fact2;
    
    double h_fd;
    
    traj_t *nominal;
    traj_t *candidates[NUMBER_OF_THREADS]; 
    
    traj_t trajectories[NUMBER_OF_THREADS+1];
    
    multipliers_t multipliers;

    int iterations;
    tLogLine *log;
    tLogLine *log_line;
    
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
} tOptSet;

void printParams(double **p, int k);
void printLog(tOptSet *o);
void standard_parameters(tOptSet *o);
int iterate(tOptSet *o);
const char *setOptParam(tOptSet *o, const char *name, const double *value, const int n);
int forward_pass(traj_t *c, const tOptSet *o, double alpha, double &csum, int cost_only);
void makeCandidateNominal(tOptSet *o, int idx);
int calc_derivs(const tOptSet *o);
int init_opt(tOptSet *o);
int update_multipliers(tOptSet *o, int init);
int get_g_size();
int calcG(double g[], const trajEl_t *t, int k, double *p[]);

static double max(double a, double b) {
    return (a > b)? a: b;
}

static double min(double a, double b) {
    return (a < b)? a: b;
}


extern int n_params;
extern int n_vars;
extern tParamDesc *paramdesc[];


#endif /* ILQG_H */
