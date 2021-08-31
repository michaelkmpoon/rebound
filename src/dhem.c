// TES is an open source integration package for modelling exoplanet evolution.
// Copyright (C) <2021>  <Peter Bartram, Alexander Wittig>

// TES is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>. 

#include "radau.h"
#include "dhem.h"
#include "UniversalVars.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static DHEM * dhem;

#define NORM(x, i) sqrt(x[3*i]*x[3*i]+x[3*i+1]*x[3*i+1]+x[3*i+2]*x[3*i+2])
#define NORMl(x, i) sqrtq(x[3*i]*x[3*i]+x[3*i+1]*x[3*i+1]+x[3*i+2]*x[3*i+2])
#define DOT(x, y, i, j) (x[3*i+0]*y[3*j+0]+x[3*i+1]*y[3*j+1]+x[3*i+2]*y[3*j+2])

static inline void add_cs(double* out, double* cs, double inp);

void dhem_rhs_wrapped(struct reb_simulation* r, double * dQ, double * dP, double * dQ_dot,
              double * dP_dot, double * dQ_ddot, double * dP_ddot, uint32_t stageNumber,
              double * cs1, double * cs2)
{
  // Set up the pointer to the previously calculated osculating orbit values.
  dhem->Xosc = dhem->XoscArr[stageNumber];
  dhem->Qosc = dhem->Xosc;
  dhem->Posc = &dhem->Qosc[3*r->N];
  dhem->Xosc_dot = dhem->Xosc_dotArr[stageNumber];
  dhem->Qosc_dot = dhem->Xosc_dot;
  dhem->Posc_dot = &dhem->Qosc_dot[3*r->N];

  // cs vars
  dhem->Xosc_cs = dhem->XoscArr_cs[stageNumber];
  dhem->Qosc_cs = dhem->Xosc_cs;
  dhem->Posc_cs = &dhem->Qosc_cs[3*r->N];

  dhem_rhs(r, dQ, dP, dQ_dot, dP_dot, dQ_ddot, dP_ddot);

}


/*
 * dQ, dP are the input deltas and dQ_dot, dP_dot are the first derivatives terms.
 * dQ_ddot and dP_ddot are the second derivative terms.
 */
void dhem_rhs(struct reb_simulation* r, double const * __restrict__ const dQ, double const * __restrict__ const dP, double * __restrict__ const dQ_dot,
              double * __restrict__ const dP_dot, double * __restrict__ const dQ_ddot, double * __restrict__ const dP_ddot)
{
  // Not necessary but makes code more reable.
  const double * const __restrict__ m = dhem->m;
  const uint32_t n = r->N;
  const uint32_t n3 = 3*r->N;
  const double G = r->G;
  const double * const __restrict__ Qosc = dhem->Qosc;
  const double * const __restrict__ Posc = dhem->Posc;
  const double * const __restrict__ Posc_dot = dhem->Posc_dot;
  double * __restrict__ Q = dhem->Q;
  double * __restrict__ P = dhem->P;

  memset(dQ_ddot, 0, r->ri_tes.stateVectorSize/2);

  #pragma GCC ivdep
  for(uint32_t i = 3; i < n3; i++)
  {
      Q[i] = Qosc[i] + dQ[i];
      P[i] = Posc[i] + dP[i]; 
  }    

  double vCentral[3] = {0,0,0};

  #pragma GCC ivdep
  for(uint32_t i = 1; i < n; i++)
  {
    vCentral[0] += P[3*i];
    vCentral[1] += P[3*i+1];
    vCentral[2] += P[3*i+2];
  }

  #pragma GCC ivdep
  for(uint32_t i = 0; i < 3; i ++)
  {
    vCentral[i] *= dhem->m_inv[0];
  }

  #pragma GCC ivdep
  for(uint32_t i = 1; i < n; i++)
  {
      dQ_dot[3*i] =   (dP[3*i]   / m[i]) + vCentral[0];
      dQ_dot[3*i+1] = (dP[3*i+1] / m[i]) + vCentral[1];
      dQ_dot[3*i+2] = (dP[3*i+2] / m[i]) + vCentral[2];
  }

  #pragma GCC ivdep
  for(uint32_t i = 1; i < n; i++)
  {
    const double GMM = G*m[0]*m[i];
    
    const double dQx = dQ[3*i+0];
    const double dQy = dQ[3*i+1];
    const double dQz = dQ[3*i+2];
    
    const double Qx = Q[3*i+0];
    const double Qy = Q[3*i+1];
    const double Qz = Q[3*i+2];      
    
    // Calculate the osculating orbit contribution term.
    const double Qosc_norm = NORM(Qosc, i);
    const double Qosc_norm3 = Qosc_norm*Qosc_norm*Qosc_norm;
    const double GMM_Qosc_norm3Inv = GMM/Qosc_norm3;

    const double drx = dQx-2*Qx;
    const double dry = dQy-2*Qy;
    const double drz = dQz-2*Qz;
    const double q = (dQx*drx+dQy*dry+dQz*drz) /  (Qx*Qx+Qy*Qy+Qz*Qz);
    const double q1 = (1+q);
    const double q3 = q1*q1*q1;
    const double fq = -q*(3+3*q+q*q) / (1+sqrt(q3));
    const double GMM_Qosc_norm3Inv_fq = GMM_Qosc_norm3Inv*fq;

    dP_dot[3*i]   = -dQx*GMM_Qosc_norm3Inv + GMM_Qosc_norm3Inv_fq*Qx;
    dP_dot[3*i+1] = -dQy*GMM_Qosc_norm3Inv + GMM_Qosc_norm3Inv_fq*Qy;
    dP_dot[3*i+2] = -dQz*GMM_Qosc_norm3Inv + GMM_Qosc_norm3Inv_fq*Qz;
  }
  
  const double istart = 1;
  for(uint32_t i = istart; i < n; i++)
  {
      const double GM = G*m[i];
      #pragma GCC ivdep
      for(uint32_t j = istart; j < i; j++)
      {
          const double GMM = GM*m[j];
          const double dx = Q[3*j+0] - Q[3*i+0];
          const double dy = Q[3*j+1] - Q[3*i+1];
          const double dz = Q[3*j+2] - Q[3*i+2];

          const double sepNorm = sqrt(dx*dx+dy*dy+dz*dz);
          const double sepNorm2 = sepNorm*sepNorm;
          const double sepNorm3 = sepNorm2*sepNorm;

          const double GMM_SepNorm3Inv = (GMM/sepNorm3);

          dP_dot[3*i+0] += dx*GMM_SepNorm3Inv;
          dP_dot[3*i+1] += dy*GMM_SepNorm3Inv;
          dP_dot[3*i+2] += dz*GMM_SepNorm3Inv;

          dP_dot[3*j+0] -= dx*GMM_SepNorm3Inv;
          dP_dot[3*j+1] -= dy*GMM_SepNorm3Inv;
          dP_dot[3*j+2] -= dz*GMM_SepNorm3Inv;
      }
  }


  double vCentralDot[3] = {0,0,0};

  /** Calculate the momentum of the central body. **/
  #pragma GCC ivdep
  for(uint32_t i = 1; i < n; i++)
  {
    vCentralDot[0] += (Posc_dot[3*i] + dP_dot[3*i]);
    vCentralDot[1] += (Posc_dot[3*i+1] + dP_dot[3*i+1]);
    vCentralDot[2] += (Posc_dot[3*i+2] + dP_dot[3*i+2]);
  }

  #pragma GCC ivdep
  for(uint32_t i = 0; i < 3; i ++)
  {
    vCentralDot[i] *= dhem->m_inv[0];
  }

  #pragma GCC ivdep
  for(uint32_t i = 1; i < n; i++)
  {
    dQ_ddot[3*i]   += vCentralDot[0];
    dQ_ddot[3*i+1] += vCentralDot[1];
    dQ_ddot[3*i+2] += vCentralDot[2];
    
    dQ_ddot[3*i] += dP_dot[3*i] * dhem->m_inv[i];
    dQ_ddot[3*i+1] += dP_dot[3*i+1] * dhem->m_inv[i];
    dQ_ddot[3*i+2] += dP_dot[3*i+2] * dhem->m_inv[i];
  }
}

double dhem_CalculateHamiltonian(struct reb_simulation* r, double * Q, double * P)
{
  double * m = r->ri_tes.mass;
  double hamiltonian = 0;
  double Psun[3] = {0,0,0};
  double sepVec[3] = {0,0,0};

  for(uint32_t i = 1; i < r->N; i++)
  {
    double Pnorm = (NORM(P, i));
    hamiltonian += Pnorm*Pnorm / (2*m[i]);
    hamiltonian -= r->G*m[0]*m[i] / NORM(Q, i);

    Psun[0] += P[3*i];
    Psun[1] += P[3*i+1];
    Psun[2] += P[3*i+2];
  }
  double PsunNorm = NORM(Psun, 0);
  hamiltonian += (1.0/(2.0*m[0])) * PsunNorm*PsunNorm;

  for(uint32_t i = 1; i < r->N; i++)
  {
    for(uint32_t j = i+1; j < r->N; j++)
    {
      sepVec[0] = Q[3*i]-Q[3*j];
      sepVec[1] = Q[3*i+1]-Q[3*j+1];
      sepVec[2] = Q[3*i+2]-Q[3*j+2];

      hamiltonian -= (r->G*m[i]*m[j]) / NORM(sepVec, 0);
    }
  }

  return hamiltonian;
}

void dhem_PerformSummation(struct reb_simulation* r, double * Q, double * P,
                               double * dQ, double * dP, uint32_t stageNumber)
{
  // Need to setup a pointer to the osculating orbits in memory.
  dhem->Xosc = dhem->XoscArr[stageNumber];
  dhem->Qosc = dhem->Xosc;
  dhem->Posc = &dhem->Qosc[3*r->N];

  // Always zero in our frame of reference.
  for(uint32_t i = 0; i < 3; i++)
  {
    Q[i] = 0;
    P[i] = 0;
  }

  for(uint32_t i = 1; i < r->N; i++)
  {
    Q[3*i+0] = dhem->Qosc[3*i+0] + (dQ[3*i+0] + (dhem->Qosc_cs[3*i+0] + r->ri_tes.radau->cs_dq[3*i+0]));
    Q[3*i+1] = dhem->Qosc[3*i+1] + (dQ[3*i+1] + (dhem->Qosc_cs[3*i+1] + r->ri_tes.radau->cs_dq[3*i+1]));
    Q[3*i+2] = dhem->Qosc[3*i+2] + (dQ[3*i+2] + (dhem->Qosc_cs[3*i+2] + r->ri_tes.radau->cs_dq[3*i+2]));

    P[3*i+0] = dhem->Posc[3*i+0] + (dP[3*i+0] + (dhem->Posc_cs[3*i+0] + r->ri_tes.radau->cs_dp[3*i+0]));
    P[3*i+1] = dhem->Posc[3*i+1] + (dP[3*i+1] + (dhem->Posc_cs[3*i+1] + r->ri_tes.radau->cs_dp[3*i+1]));
    P[3*i+2] = dhem->Posc[3*i+2] + (dP[3*i+2] + (dhem->Posc_cs[3*i+2] + r->ri_tes.radau->cs_dp[3*i+2]));
  }

}

void dhem_InitialiseOsculatingOrbits(struct reb_simulation* r, double * Q, double * P, double t)
{
  for(uint32_t i = 1; i < r->N; i++)
  {
    RebasisOsculatingOrbits_Momenta(r, Q, P, t, i);
  }
}

uint32_t dhem_RectifyOrbits(struct reb_simulation* r, double t, double * Q, double * P,
                            double * dQ, double * dP, uint32_t * rectifiedArray, uint32_t stageNumber)
{
  uint32_t rectifyFlag = 0;
  uint32_t rectifiedCount = 0;
  double dQ_norm = 0;
  double dP_norm = 0;

  // Need to setup a pointer to the osculating orbits in memory.
  dhem->Xosc = dhem->XoscArr[stageNumber];
  dhem->Qosc = dhem->Xosc;
  dhem->Posc = &dhem->Qosc[3*r->N];
  dhem->Xosc_dot = dhem->Xosc_dotArr[stageNumber];
  dhem->Qosc_dot = dhem->Xosc_dot;
  dhem->Posc_dot = &dhem->Qosc_dot[3*r->N];

  // CS variables
  dhem->Xosc_cs = dhem->XoscArr_cs[stageNumber];
  dhem->Qosc_cs = dhem->Xosc_cs;
  dhem->Posc_cs = &dhem->Qosc_cs[3*r->N];

  for(uint32_t i = 1; i < r->N; i++)
  {
    rectifiedArray[3*i] = 0;
    rectifiedArray[3*i+1] = 0;
    rectifiedArray[3*i+2] = 0;

    rectifiedArray[r->N*3+3*i] = 0;
    rectifiedArray[r->N*3+3*i+1] = 0;
    rectifiedArray[r->N*3+3*i+2] = 0;

    dQ_norm = NORM(dQ, i) / NORM(dhem->Qosc, i);
    dP_norm = NORM(dP, i) / NORM(dhem->Posc, i);

    if(t > dhem->rectifyTimeArray[i] ||
      dQ_norm > r->ri_tes.dq_max)
    {
      rectifyFlag = 1;
      break;
    }
  }

  for(uint32_t i = 1; i < r->N; i++)
  {
    if(rectifyFlag != 0)
    {
      rectifiedCount++;

      for(uint32_t j = 0; j < 3; j++)
      {
        double temp_cs = 0;

        Q[3*i+j] = dhem->Qosc[3*i+j];
        add_cs(&Q[3*i+j], &temp_cs, dQ[3*i+j]);
        add_cs(&Q[3*i+j], &temp_cs, r->ri_tes.uVars->uv_csq[3*i+j]);            
        add_cs(&Q[3*i+j], &temp_cs, r->ri_tes.radau->cs_dq[3*i+j]);

        dQ[3*i+j] = -temp_cs;
        r->ri_tes.radau->cs_dq[3*i+j] = 0;
        r->ri_tes.uVars->uv_csq[3*i+j] = 0;
      }

      for(uint32_t j = 0; j < 3; j++)
      {
        double temp_cs = 0;

        P[3*i+j] = dhem->Posc[3*i+j];
        add_cs(&P[3*i+j], &temp_cs, dP[3*i+j]);
        add_cs(&P[3*i+j], &temp_cs, r->ri_tes.uVars->uv_csp[3*i+j]);            
        add_cs(&P[3*i+j], &temp_cs, r->ri_tes.radau->cs_dp[3*i+j]);

        dP[3*i+j] = -temp_cs;
        r->ri_tes.radau->cs_dp[3*i+j] = 0;
        r->ri_tes.uVars->uv_csp[3*i+j] = 0;
      }          

      RebasisOsculatingOrbits_Momenta(r, Q, P, t, i);
      //dhem->rectifyTimeArray[i] += dhem->rectificationPeriod[i];
      // This option will add some randomness to the rectification process.
      dhem->rectifyTimeArray[i] = t + dhem->rectificationPeriod[i];

      rectifiedArray[3*i] = 1;
      rectifiedArray[3*i+1] = 1;
      rectifiedArray[3*i+2] = 1;
      rectifiedArray[r->N*3+3*i] = 1;
      rectifiedArray[r->N*3+3*i+1] = 1;
      rectifiedArray[r->N*3+3*i+2] = 1;
    }    
  }


  return rectifiedCount;
}

void dhem_CalculateOsculatingOrbitDerivatives_Momenta(struct reb_simulation* r, double const * const __restrict__ Qosc, double const * const __restrict__ Posc, 
                                                      double * const __restrict__ Qosc_dot, double * const __restrict__ Posc_dot)
{
  const double GM0 = -r->G*dhem->m[0];

  for(uint32_t i = 1; i < r->N; i++)
  {
    const double m = r->ri_tes.mass[i];
    const double GMM = GM0*m;
    // Copy across the velocity term.
    Qosc_dot[3*i+0] = Posc[3*i] / m;
    Qosc_dot[3*i+1] = Posc[3*i+1] / m;
    Qosc_dot[3*i+2] = Posc[3*i+2] / m;

    // Calculate the change in momenta term.
    double Q = NORM(Qosc, i);
    double const GMM_Q3 =  GMM/(Q*Q*Q);
    Posc_dot[3*i+0] = GMM_Q3*Qosc[3*i];
    Posc_dot[3*i+1] = GMM_Q3*Qosc[3*i+1];
    Posc_dot[3*i+2] = GMM_Q3*Qosc[3*i+2];
  }
}

void dhem_CalcOscOrbitsForAllStages(struct reb_simulation* r, double t0, double h, double * hArr, uint32_t z_stagesPerStep, uint32_t z_rebasis)
{
  if(z_rebasis != 0)
  {
    CalculateOsculatingOrbitsForSingleStep(r, dhem->XoscArr, t0, h, hArr, z_stagesPerStep, z_rebasis);  
  }
  else
  {
    CalculateOsculatingOrbitsForSingleStep(r, dhem->XoscPredArr, t0, h, hArr, z_stagesPerStep, z_rebasis);  
  }

  for(uint32_t i = 0; i < z_stagesPerStep; i++)
  {
      double const * const __restrict__ Qout = dhem->XoscArr[i];
      double const * const __restrict__ Pout = &dhem->XoscArr[i][3*r->N];
      double * const __restrict__ Q_dot_out = dhem->Xosc_dotArr[i];
      double * const __restrict__ P_dot_out = &dhem->Xosc_dotArr[i][3*r->N];

      dhem_CalculateOsculatingOrbitDerivatives_Momenta(r, Qout, Pout, Q_dot_out, P_dot_out);
  }
}

void dhem_Init(struct reb_simulation* r, double z_rectificationPeriodDefault, uint32_t z_stagesPerStep)
{
  // Get memory for the dhem state vectors.
  dhem = (DHEM*)malloc(sizeof(DHEM));
  memset(dhem, 0, sizeof(DHEM));

  // Configure function pointers for other modules.
  r->ri_tes.rhs = dhem;

  dhem->final_stage_index = 8;

  dhem->X = (double*)malloc(r->ri_tes.stateVectorSize);
  dhem->X_dot = (double*)malloc(r->ri_tes.stateVectorSize);
  dhem->rectifyTimeArray = (double*)malloc(r->ri_tes.controlVectorSize);
  dhem->rectificationPeriod = (double*)malloc(r->ri_tes.controlVectorSize);

  // Create space to allow for all of the osculating orbits for a step to be stored.
  dhem->XoscStore = (double*)malloc(z_stagesPerStep*r->ri_tes.stateVectorSize);
  dhem->XoscArr = (double **)malloc(z_stagesPerStep*sizeof(double*));
  dhem->Xosc_dotStore = (double*)malloc(z_stagesPerStep*r->ri_tes.stateVectorSize);
  dhem->Xosc_dotArr = (double **)malloc(z_stagesPerStep*sizeof(double*));
  dhem->Vosc = (double*)malloc(r->ri_tes.stateVectorSize / 2);

  // Create space to allow for all of the osculating orbits for a step to be stored.
  dhem->XoscPredStore = (double*)malloc(z_stagesPerStep*r->ri_tes.stateVectorSize);
  dhem->XoscPredArr = (double **)malloc(z_stagesPerStep*sizeof(double*));


  // Creat space for osculating orbit compensated summation variables
  dhem->XoscStore_cs = (double*)malloc(z_stagesPerStep*r->ri_tes.stateVectorSize);
  dhem->XoscArr_cs = (double **)malloc(z_stagesPerStep*sizeof(double*));
  
  // Compensated summation rhs variables
  dhem->dQdot_cs = (double*)malloc((int)r->ri_tes.stateVectorSize/2);
  dhem->dQddot_cs = (double*)malloc((int)r->ri_tes.stateVectorSize/2);
  dhem->dPdot_cs = (double*)malloc((int)r->ri_tes.stateVectorSize/2);
  memset(dhem->dQdot_cs, 0, r->ri_tes.stateVectorSize / 2);
  memset(dhem->dQddot_cs, 0, r->ri_tes.stateVectorSize / 2);
  memset(dhem->dPdot_cs, 0, r->ri_tes.stateVectorSize / 2);


  // Set required arrays to zero.
  memset(dhem->X, 0, r->ri_tes.stateVectorSize);
  memset(dhem->X_dot, 0, r->ri_tes.stateVectorSize);
  memset(dhem->rectifyTimeArray, 0, r->ri_tes.controlVectorSize);
  memset(dhem->rectificationPeriod, 0, r->ri_tes.controlVectorSize);

  memset(dhem->XoscStore, 0, z_stagesPerStep*r->ri_tes.stateVectorSize);
  memset(dhem->XoscArr, 0, z_stagesPerStep*sizeof(double *));
  memset(dhem->Xosc_dotStore, 0, z_stagesPerStep*r->ri_tes.stateVectorSize);
  memset(dhem->Xosc_dotArr, 0, z_stagesPerStep*sizeof(double *));
  memset(dhem->Vosc, 0, r->ri_tes.stateVectorSize / 2);

  memset(dhem->XoscPredStore, 0, z_stagesPerStep*r->ri_tes.stateVectorSize);
  memset(dhem->XoscPredArr, 0, z_stagesPerStep*sizeof(double *));

  memset(dhem->XoscStore_cs, 0, z_stagesPerStep*r->ri_tes.stateVectorSize);
  memset(dhem->XoscArr_cs, 0, z_stagesPerStep*sizeof(double *));

  // To enable easier access to the osculating orbits.
  for(uint32_t i = 0; i < z_stagesPerStep; i++)
  {
    dhem->XoscArr[i] = &dhem->XoscStore[i*r->ri_tes.stateVectorLength];
    dhem->XoscPredArr[i] = &dhem->XoscPredStore[i*r->ri_tes.stateVectorLength];
    dhem->Xosc_dotArr[i] = &dhem->Xosc_dotStore[i*r->ri_tes.stateVectorLength];

    dhem->XoscArr_cs[i] = &dhem->XoscStore_cs[i*r->ri_tes.stateVectorLength];
  }

  // Setup pointers for more human readable access.
  dhem->Qosc = dhem->XoscArr[0];
  dhem->Posc = &dhem->Qosc[3*r->N];

  dhem->Qosc_cs = dhem->XoscArr_cs[0];
  dhem->Posc_cs = &dhem->Qosc_cs[3*r->N];

  dhem->Qosc_dot = dhem->Xosc_dotArr[0];
  dhem->Posc_dot = &dhem->Qosc_dot[3*r->N];

  dhem->Q = dhem->X;
  dhem->P = &dhem->X[3*r->N];

  dhem->Q_dot = dhem->X_dot;
  dhem->P_dot = &dhem->X_dot[3*r->N];

  dhem->m = r->ri_tes.mass;
  dhem->mTotal = 0;

  dhem->m_inv = (double*)malloc(r->N*sizeof(double));

  for(uint32_t i = 0; i < r->N; i++)
  {
    dhem->m_inv[i] = 1.0 / dhem->m[i];
    dhem->mTotal += dhem->m[i];
    dhem->rectifyTimeArray[i] = r->ri_tes.t0 + z_rectificationPeriodDefault;
    dhem->rectificationPeriod[i] = z_rectificationPeriodDefault;
  }
}

void dhem_Free(void)
{
  // @todo clean up properly - should run a memcheck on the entire program.
  // free(dhem->X);
  // free(dhem->X_dot);
  // free(dhem->rectifyTimeArray);
  // free(dhem->rectificationPeriod);
  // free(dhem->XoscStore);
  // free(dhem->XoscArr);
  // free(dhem->Xosc_dotStore);
  // free(dhem->Xosc_dotArr);
  // free(dhem->Vosc);
  // free(dhem);
  // free(dhem->fStore);
  // free(dhem->fAccessArray);
  // free(dhem->gStore);
  // free(dhem->gAccessArray);
  // sim->rhs = NULL;
}

static inline void add_cs(double* out, double* cs, double inp)
{
    const double y = inp - cs[0];
    const double t = out[0] + y;
    cs[0] = (t - out[0]) - y;
    out[0] = t;
}
