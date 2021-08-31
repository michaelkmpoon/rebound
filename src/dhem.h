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

#ifndef _DHEM_H_
#define _DHEM_H_

#include "rebound.h"

typedef struct DHEM
{
  // Pointers to osculating orbits at a single point in time.
  double * Xosc;
  double * Qosc;
  double * Posc;
  double * Vosc;
  // Osculating orbits for all stages within a step.
  double * XoscStore;
  double ** XoscArr;
  double * XoscPredStore;
  double ** XoscPredArr;  
  // CS variables for the osculating orbits.
  double * Xosc_cs;
  double * XoscStore_cs;
  double ** XoscArr_cs;
  double * Qosc_cs;
  double * Posc_cs;
  // Pointers to osculating orbit derivatives at a single point in time.
  double * Xosc_dot;
  double * Qosc_dot;
  double * Posc_dot;
  // Osculating orbits derivatives for all stages within a step.
  double * Xosc_dotStore;
  double ** Xosc_dotArr;
  // Variables for summing Xosc+dX into.
  double * X;
  double * Q;
  double * P;
  // Mass variables.
  double * __restrict__ m;
  double * __restrict__ m_inv;
  double mTotal;
  // Rectification variables.
  double * rectifyTimeArray;    /// The time at which we need to rectify each body.
  double * rectificationPeriod; /// Elapsed time to trigger a rectification.
}DHEM;

void dhem_PerformSummation(struct reb_simulation* r, double * Q, double * P,
                               double * dQ, double * dP, uint32_t stageNumber);
void dhem_CalcOscOrbitsForAllStages(struct reb_simulation* r, double t0, double h, double * hArr, uint32_t z_stagesPerStep, uint32_t z_rebasis);
double dhem_CalculateHamiltonian(struct reb_simulation* r, double * Q, double * P);
void dhem_ConvertToDHCoords(double * Q, double * V, double * Qout, double * Pout);
void dhem_ConvertToCOM(double * Q, double * V, double * Qout, double * Vout);
void dhem_InitialiseOsculatingOrbits(struct reb_simulation* r, double * Q, double * P, double t);
void dhem_rhs(struct reb_simulation* r, double const * __restrict__ const dQ, double const * __restrict__ const dP, double * __restrict__ const dQ_dot,
              double * __restrict__ const dP_dot, double * __restrict__ const dQ_ddot, double * __restrict__ const dP_ddot);
void dhem_rhs_wrapped(struct reb_simulation* r, double * dQ, double * dP, double * dQ_dot,
                      double * dP_dot, double * dQ_ddot, double * dP_ddot, uint32_t stageNumber,
                      double * cs1, double * cs2);      
void dhem_CalculateOsculatingOrbitDerivatives_Momenta(struct reb_simulation* r, double const * const __restrict__ Qosc, double const * const __restrict__ Posc, 
                                                      double * const __restrict__ Qosc_dot, double * const __restrict__ Posc_dot);
uint32_t dhem_RectifyOrbits(struct reb_simulation* r, double t, double * Q, double * P,
                            double * dQ, double * dP, uint32_t * rectifiedArray, uint32_t stageNumber);
void dhem_Init(struct reb_simulation* r, double z_rectificationPeriodDefault, uint32_t z_stagesPerStep);
void dhem_Free(void);

#endif
