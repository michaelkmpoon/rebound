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
#include "radau_step.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include "dhem.h"
#include "UniversalVars.h"

#define FINAL_STAGE_INDEX 8
#define MAX_STEP_SIZE_GROWTH 4.0
#define MIN_STEP_SIZE 1E-5
#define OSCULATING_ORBIT_SLOTS 9  // stages + 2 for t=0 and t=1.

static RADAU * radau;

double t = 0.0;
double tEnd = 0.0;
double h = 0.0;
uint32_t iterations = 0;

static double hArr[9] = {0.0, 0.0562625605369221464656521910318, 0.180240691736892364987579942780, 0.352624717113169637373907769648, 0.547153626330555383001448554766, 0.734210177215410531523210605558, 0.885320946839095768090359771030, 0.977520613561287501891174488626, 1.0};


double Radau_SingleStep(struct reb_simulation* r, double z_t, double dt, double dt_last_done)
{
    double dt_new = 0.0;
    radau->h = dt;
    radau->t = z_t;

    uint32_t rectificationCount = dhem_RectifyOrbits(r, z_t, r->ri_tes.Q_dh, r->ri_tes.P_dh, radau->dQ,
                                        radau->dP, radau->rectifiedArray, FINAL_STAGE_INDEX);
    radau->rectifications += rectificationCount;

    // Calculate the osculating orbits.
    dhem_CalcOscOrbitsForAllStages(r, z_t, dt, hArr, OSCULATING_ORBIT_SLOTS, 1);

    ClearRectifiedBFields(r, radau->B, radau->rectifiedArray);
    ClearRectifiedBFields(r, radau->B_1st, radau->rectifiedArray);

    radau->CalculateGfromB(r); 

    radau->step(r, &iterations, z_t, dt);
    radau->convergenceIterations += iterations;

    dt_new = r->ri_tes.epsilon > 0 ? Radau_CalculateStepSize(r, dt, dt_last_done, z_t) : dt;

    radau->AnalyticalContinuation(r, radau->B_1st, radau->Blast_1st, dt, dt_new, radau->rectifiedArray);
    radau->AnalyticalContinuation(r, radau->B, radau->Blast, dt, dt_new, radau->rectifiedArray);

    return dt_new;
}

void Radau_Init(struct reb_simulation* r)
{
  radau = (RADAU *)malloc(sizeof(RADAU));
  memset(radau, 0, sizeof(RADAU));

  r->ri_tes.radau = radau;
  radau->dX = (double*)malloc(r->ri_tes.stateVectorSize);
  radau->dXtemp = (double*)malloc(r->ri_tes.stateVectorSize);
  radau->dX0 = (double*)malloc(r->ri_tes.stateVectorSize);
  radau->X = (double*)malloc(r->ri_tes.stateVectorSize);
  radau->Xout = (double*)malloc(r->ri_tes.stateVectorSize);
  radau->predictors = (double*)malloc(r->ri_tes.stateVectorSize);

  radau->q = (double*)malloc(r->ri_tes.stateVectorSize/2);
  radau->p = (double*)malloc(r->ri_tes.stateVectorSize/2);
  radau->q_dot = (double*)malloc(r->ri_tes.stateVectorSize/2);
  radau->q_ddot = (double*)malloc(r->ri_tes.stateVectorSize/2);
  radau->p_dot = (double*)malloc(r->ri_tes.stateVectorSize/2);

  memset(radau->q, 0, r->ri_tes.stateVectorSize/2);
  memset(radau->p, 0, r->ri_tes.stateVectorSize/2);
  memset(radau->q_dot, 0, r->ri_tes.stateVectorSize/2);
  memset(radau->q_ddot, 0, r->ri_tes.stateVectorSize/2);
  memset(radau->p_dot, 0, r->ri_tes.stateVectorSize/2);

  radau->q0 = (double*)malloc(r->ri_tes.stateVectorSize/2);
  radau->p0 = (double*)malloc(r->ri_tes.stateVectorSize/2);
  radau->q_dot0 = (double*)malloc(r->ri_tes.stateVectorSize/2);
  radau->q_ddot0 = (double*)malloc(r->ri_tes.stateVectorSize/2);
  radau->p_dot0 = (double*)malloc(r->ri_tes.stateVectorSize/2);

  memset(radau->q0, 0, r->ri_tes.stateVectorSize/2);
  memset(radau->p0, 0, r->ri_tes.stateVectorSize/2);
  memset(radau->q_dot0, 0, r->ri_tes.stateVectorSize/2);
  memset(radau->q_ddot0, 0, r->ri_tes.stateVectorSize/2);
  memset(radau->p_dot0, 0, r->ri_tes.stateVectorSize/2);

  memset(radau->dX, 0, r->ri_tes.stateVectorSize);
  memset(radau->dXtemp, 0, r->ri_tes.stateVectorSize);
  memset(radau->dX0, 0, r->ri_tes.stateVectorSize);
  memset(radau->X, 0, r->ri_tes.stateVectorSize);
  memset(radau->Xout, 0, r->ri_tes.stateVectorSize);
  memset(radau->predictors, 0, r->ri_tes.stateVectorSize);

  radau->Q = radau->X;
  radau->P = &radau->X[3*r->N];
  radau->dQ = radau->dX;
  radau->dP = &radau->dX[3*r->N];
  radau->Qout = radau->Xout;
  radau->Pout = &radau->Xout[r->ri_tes.stateVectorLength/2];

// Copy to here so that we are ready to output to a file before we calculate osculating orbtis.
  memcpy(radau->Qout, r->ri_tes.Q_dh, r->ri_tes.stateVectorSize / 2);
  memcpy(radau->Pout, r->ri_tes.P_dh, r->ri_tes.stateVectorSize / 2);

  radau->rectifiedArray = (uint32_t*)malloc(sizeof(uint32_t)*r->ri_tes.stateVectorLength);
  memset(radau->rectifiedArray, 0, sizeof(uint32_t)*r->ri_tes.stateVectorLength);

  radau->b6_store = (double*)malloc(r->ri_tes.stateVectorSize);
  radau->Xsize = (double*)malloc(2*r->ri_tes.controlVectorSize);

  memset(radau->b6_store, 0, r->ri_tes.stateVectorSize);
  memset(radau->Xsize, 0, 2*r->ri_tes.controlVectorSize);

  //@todo should be able to remove these, but test.
  radau->fCalls = 0;
  radau->rectifications = 0;
  radau->stepsTaken = 0;
  radau->convergenceIterations = 0;
  radau->nextOutputTime = 0.0;

  // Copy across our tolerance fields.
  radau->rTol = r->ri_tes.epsilon;

  RadauStep15_Init(r);
}

void Radau_Free(void)
{
  RadauStep15_Free();
  free(radau->dX);
  free(radau->dX0);
  free(radau->X);
  free(radau->Xout);
  free(radau->predictors);
  free(radau->rectifiedArray);
  free(radau);
}

double Radau_CalculateStepSize(struct reb_simulation* r, double h, double hLast, double t)
{
  double hTrial = 0.0;

  // Get the error estimate and orbit size estimate.
  double errMax = radau->ReturnStepError(r, h, t);
    
  if(isnormal(errMax))
  {
    hTrial = h*pow(radau->rTol / errMax, (1.0/7.0));
  }
  else
  {
    hTrial = 1.1*h;
  }

  // Impose a minimum step size.
  hTrial = hTrial < MIN_STEP_SIZE ? MIN_STEP_SIZE : hTrial;

  // Limit step size growth to 4x per step.
  hTrial = (hTrial > MAX_STEP_SIZE_GROWTH*h) ? h*MAX_STEP_SIZE_GROWTH : hTrial;

  return hTrial;
}


void ClearRectifiedBFields(struct reb_simulation* r, controlVars * B, uint32_t * rectifiedArray)
{
  for(uint32_t i = 0; i < r->ri_tes.stateVectorLength; i++)
  {
    if(rectifiedArray[i] > 0)
    {
      B->p0[i] = 0.0;
      B->p1[i] = 0.0;
      B->p2[i] = 0.0;
      B->p3[i] = 0.0;
      B->p4[i] = 0.0;
      B->p5[i] = 0.0;
      B->p6[i] = 0.0;
    }
  }
}