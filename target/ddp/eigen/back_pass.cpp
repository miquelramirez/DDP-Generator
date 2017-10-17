// iLQG back pass c implementation of http://www.mathworks.com/matlabcentral/fileexchange/52069-ilqg-ddp-trajectory-optimization by Yuval Tassa
// Copyright (c) 2016 Jens Geisler
//
// BIBTeX:
// @INPROCEEDINGS{
// author={Tassa, Y. and Mansard, N. and Todorov, E.},
// booktitle={Robotics and Automation (ICRA), 2014 IEEE International Conference on},
// title={Control-Limited Differential Dynamic Programming},
// year={2014}, month={May}, doi={10.1109/ICRA.2014.6907001}}

#include <math.h>

#include "back_pass.h"
#include "iLQG.hpp"
#include "boxQP.h"

#ifndef DEBUG_BACKPASS
#define DEBUG_BACKPASS 0
#else
    #if PREFIX1(DEBUG_BACKPASS)==1
    #define DEBUG_BACKPASS 1
    #endif
#endif

#define TRACE(x) do { if (DEBUG_BACKPASS) PRNT x; } while (0)
// #define printVec_(x) do { if (DEBUG_BACKPASS) printVec x; } while (0)
// #define printMat_(x) do { if (DEBUG_BACKPASS) printMat x; } while (0)

using namespace Eigen;

#if FULL_DDP
static void addVecTensUU(Ref<MatrixUU> Quu, const Ref<const VectorX> Vx, trajEl_t *t);
static void addVecTensXX(Ref<MatrixXX> Qxx, const Ref<const VectorX> Vx, trajEl_t *t);
static void addVecTensXU(Ref<MatrixXU> Qxu, const Ref<const VectorX> Vx, trajEl_t *t);
#endif

int back_pass(tOptSet *o) {
    double g_norm_i, g_norm_max, g_norm_sum;
    int N= o->n_hor;
    VectorU Qu;
    VectorU_int is_clamped;
    VectorX Vx, Qx;
    MatrixXX Vxx, Qxx;
    MatrixUU_dyn lltHfree;
    MatrixUU QuuF, Quu;
    MatrixXU Qxu_reg, Qxu;
    trajEl_t *t= o->nominal->t + N - 1;
    trajFin_t *f= &o->nominal->f;
    
    g_norm_sum= 0.0;

    o->dV[0]= 0.0;
    o->dV[1]= 0.0;
    
    Vx= f->cx;
    Vxx= f->cxx;

    for(int k= N-1; k>=0; k--, t--) {
        Qu= t->cu;
        Qu.noalias()+= t->fu.transpose() * Vx;
        Qx= t->cx;
        Qx.noalias()+= t->fx.transpose() * Vx;
        Qxu= t->cxu;
        Qxu.noalias()+= t->fx.transpose() * Vxx.selfadjointView<Upper>() * t->fu;
#if FULL_DDP
        addVecTensXU(Qxu, Vx, t);
#endif
        Quu.triangularView<Upper>()= t->cuu;
        Quu.triangularView<Upper>()+= t->fu.transpose() * Vxx.selfadjointView<Upper>() * t->fu;
#if FULL_DDP
        addVecTensUU(Quu, Vx, t);
#endif
        Qxx.triangularView<Upper>()= t->cxx;
        Qxx.triangularView<Upper>()+= t->fx.transpose() * Vxx.selfadjointView<Upper>() * t->fx;
#if FULL_DDP
        addVecTensXX(Qxx, Vx, t);
#endif
        
//         TRACE(("regularization\n"));
        QuuF= Quu;
        Qxu_reg= Qxu;
        if(o->regType==2) {
//             TRACE(("type 2\n"));
            QuuF.triangularView<Upper>()+= o->lambda * (t->fu.transpose() * t->fu);
            Qxu_reg.noalias()+= o->lambda * (t->fx.transpose() * t->fu);
        }
        if(o->regType==1) {
//             TRACE(("type 1\n"));
            QuuF.diagonal().array()+= o->lambda;          
        }

        // solve Quadratic Program
//         TRACE(("boxQP\n"));
        if(k==o->n_hor-1)
            t->l.setZero();
        else
            t->l= (t+1)->l;

        LLT<MatrixUU_dyn, Upper> lltQuu_free;
        int qpRes, m_free;
        if((qpRes= boxQP(QuuF, Qu, t->lower, t->upper, t->l, is_clamped, m_free, lltQuu_free))<1) {
            TRACE(("@k= %d: qpRes= %d \n", k, qpRes));
            return 1;
        }
        
        if(m_free>0) {
            MatrixUX_dyn Qux_free(m_free, N_X);

            for(int i= 0, i_free= 0; i<N_U; i++)
                if(!is_clamped(i)) {
                    Qux_free.row(i_free)= Qxu_reg.transpose().row(i);
#if CONSTRAINT_UX
                    for(int j= 0; j<N_U; j++) {
                        if(is_clamped(j)) {
                            double QuuFij;
                            if(i<=j) QuuFij= QuuF(i, j);
                            else QuuFij= QuuF(j, i);
                            
                            if(is_clamped(j)==1) {
                                Qux_free.row(i_free)-= QuuFij * t->lower_sign(j) * t->lower_hx.col(j);
                            } else if(is_clamped(j)==2) {
                                Qux_free.row(i_free)-= QuuFij * t->upper_sign(j) * t->upper_hx.col(j);
                            }
                        }
                    }
#endif
                    i_free++;
                }

            MatrixUX_dyn L_free= -lltQuu_free.solve(Qux_free);

            for(int i= 0, i_free= 0; i<N_U; i++)
                if(!is_clamped(i)) {
                    t->L.row(i)= L_free.row(i_free);
                    i_free++;
                } 
#if CONSTRAINT_UX
                else if(is_clamped[i]==1) {
                    t->L.row(i)= t->lower_sign(i) * t->lower_hx.col(i);
                } else {
                    t->L.row(i)= t->upper_sign(i) * t->upper_hx.col(i);
                }
#else
                else t->L.row(i).setZero();
#endif
        }
        
        // dV          = dV + [k_i'*Qu  .5*k_i'*Quu*k_i];
        o->dV[0]+= t->l.transpose() * Qu;
        o->dV[1]+= 0.5 * (t->l.transpose() * Quu.selfadjointView<Upper>()) * t->l;
//         TRACE(("dV= %g, %g, %g, %g, %g\n", o->dV[0], o->dV[1], t->l[0], Qu[0], Qu[1]));

        // Vx(:,i)     = Qx  + K_i'*Quu*k_i + K_i'*Qu  + Qux'*k_i;
        Vx= Qx;
        Vx.noalias()+= t->L.transpose()*Quu.selfadjointView<Upper>()*t->l;
        Vx.noalias()+= t->L.transpose()*Qu;
        Vx.noalias()+= Qxu*t->l;
    
        // Vxx(:,:,i)  = Qxx + K_i'*Quu*K_i + K_i'*Qux + Qux'*K_i;
        Vxx.triangularView<Upper>()= Qxx;
        Vxx.triangularView<Upper>()+= t->L.transpose()*Quu.selfadjointView<Upper>()*t->L;
        Qxx.noalias()= Qxu * t->L;
        Vxx.triangularView<Upper>()+= Qxx + Qxx.transpose();
        
        // g_norm= mean(max(abs(l) ./ (abs(u)+1),[],1));
        g_norm_max= 0.0;
        for(int i_= 0; i_<N_U; i_++) {
            g_norm_i= fabs(t->l(i_)) / (fabs(t->u(i_))+1.0);
            if(g_norm_i>g_norm_max) g_norm_max= g_norm_i;
        }
        g_norm_sum+= g_norm_max;
    }
    
    o->g_norm= g_norm_sum/((double)(o->n_hor-1));
    
    return 0;
}

#if FULL_DDP
static void addVecTensUU(Ref<MatrixUU> Quu, const Ref<const VectorX> Vx, trajEl_t *t) {
    // TODO check if triangular part is faster
    for(int i= 0; i<N_X; i++)
        Quu+= Vx(i) * t->fuu[i];
}

static void addVecTensXX(Ref<MatrixXX> Qxx, const Ref<const VectorX> Vx, trajEl_t *t) {
    // TODO check if triangular part is faster
    for(int i= 0; i<N_X; i++)
        Qxx+= Vx(i) * t->fxx[i];
}

static void addVecTensXU(Ref<MatrixXU> Qxu, const Ref<const VectorX> Vx, trajEl_t *t) {
    // TODO check if triangular part is faster
    for(int i= 0; i<N_X; i++)
        Qxu+= Vx(i) * t->fxu[i];
}
#endif