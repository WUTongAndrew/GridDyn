/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil;  eval: (c-set-offset 'innamespace 0); -*- */
/*
   * LLNS Copyright Start
 * Copyright (c) 2016, Lawrence Livermore National Security
 * This work was performed under the auspices of the U.S. Department
 * of Energy by Lawrence Livermore National Laboratory in part under
 * Contract W-7405-Eng-48 and in part under Contract DE-AC52-07NA27344.
 * Produced at the Lawrence Livermore National Laboratory.
 * All rights reserved.
 * For details, see the LICENSE file.
 * LLNS Copyright End
*/

#include "submodels/otherGenModels.h"
#include "generators/gridDynGenerator.h"
#include "gridBus.h"
#include "vectorOps.hpp"
#include "arrayData.h"
#include "gridCoreTemplates.h"

#include <cmath>
#include <cassert>

gridDynGenModel6::gridDynGenModel6 (const std::string &objName) : gridDynGenModel5 (objName)
{
  //changing some of the default values;
  Xqp = 0.30;
  D = 0.03;
}

gridCoreObject *gridDynGenModel6::clone (gridCoreObject *obj) const
{
  gridDynGenModel6 *gd = cloneBase<gridDynGenModel6, gridDynGenModel5> (this, obj);
  if (gd == nullptr)
    {
      return obj;
    }
  return gd;
}

void gridDynGenModel6::objectInitializeA (double /*time0*/, unsigned long /*flags*/)
{
  offsets.local->local.diffSize = 6;
  offsets.local->local.algSize = 2;
  offsets.local->local.jacSize = 40;

}

// initial conditions
void gridDynGenModel6::objectInitializeB (const IOdata &args, const IOdata &outputSet, IOdata &inputSet)
{
  if (Tqop == 0)
    {
      Tqop = 0.01;
    }

  if (Tdop == 0)
    {
      Tdop = 0.01;
    }
  computeInitialAngleAndCurrent (args, outputSet, Rs, Xq);
  double *gm = m_state.data ();


  // Edp and Eqp  and Edpp

  gm[7] = Vq + Rs * gm[1] - (Xdpp) * gm[0];
  gm[6] = Vd + Rs * gm[0] + (Xqpp) * gm[1];


  // record Pm = Pset
  //this should be close to P from above
  double Pmt = gm[6] * gm[0] + gm[7] * gm[1] + (Xdpp - Xqpp) * gm[0] * gm[1];

  gm[5] = gm[7] - (Xdp - Xdpp) * gm[0];

  gm[4] = gm[6] + (Xqp - Xqpp) * gm[1];

  // exciter - assign Ef
  double Eft = gm[5] - (Xd - Xdp) * gm[0];

  // double g4 = gm[6] + (Xqp - Xqpp + qrat * (Xq - Xqp)) * gm[1];
  //preset the inputs that should be initialized
  inputSet[2] = Eft;
  inputSet[3] = Pmt;

}

void gridDynGenModel6::algebraicUpdate (const IOdata &args, const stateData *sD, double update[], const solverMode &sMode, double /*alpha*/)
{
  Lp Loc = offsets.getLocations (sD, update, sMode, this);
  updateLocalCache (args, sD, sMode);

  solve2x2 (Rs, (Xqpp), -(Xdpp), Rs, Loc.diffStateLoc[4] - Vd, Loc.diffStateLoc[5] - Vq, Loc.destLoc[0], Loc.destLoc[1]);
  m_output = -(Loc.destLoc[1] * Vq + Loc.destLoc[0] * Vd);


}


void gridDynGenModel6::derivative (const IOdata &args, const stateData *sD, double deriv[], const solverMode &sMode)
{
  if (isAlgebraicOnly (sMode))
    {
      return;
    }
  Lp Loc = offsets.getLocations (sD,deriv, sMode, this);

  const double *gm = Loc.algStateLoc;
  const double *gmd = Loc.diffStateLoc;
  // const double *gmp = Loc.dstateLoc;

  //double *rva = Loc.destLoc;
  double *rvd = Loc.destDiffLoc;

  //Get the exciter field
  double Eft = args[genModelEftInLocation];
  double Pmt = args[genModelPmechInLocation];



  // delta
  rvd[0] = m_baseFreq * (gmd[1] - 1.0);
  // Edp and Eqp
  rvd[2] = (-gmd[2] - (Xq - Xqp) * gm[1]) / Tqop;
  rvd[3] = (-gmd[3] + (Xd - Xdp) * gm[0] + Eft) / Tdop;
  //Edpp
  rvd[4] = (-gmd[4] + gmd[2] - (Xqp - Xqpp) * gm[1]) / Tqopp;
  rvd[5] = (-gmd[5] + gmd[3] + (Xdp - Xdpp) * gm[0]) / Tdopp;
  // omega
  double Pe = gmd[4] * gm[0] + gmd[5] * gm[1] + (Xdpp - Xqpp) * gm[0] * gm[1];
  //double Pe2 = (Vq + Rs * gm[1]) * gm[1] + (Vd + Rs * gm[0]) * gm[0];
  rvd[1] = 0.5  * (Pmt - Pe - D * (gmd[1] - 1.0)) / H;

  // if (parent->parent->name == "BUS_31")
  //   {
  //   printf("[%d]t=%f gmp[1]=%f Vq=%f, Vd=%f Pdiff=%f A=%f, B=%f, C=%f Id=%f, Iq=%f, Eft=%f\n", getID(), ttime, gmp[1], Vq,Vd,Pmt - Pe, gmd[4] * gm[0], gmd[5] * gm[1], (Xdpp - Xqpp) * gm[0] * gm[1], gm[0], gm[1], Eft);
  //   }

}


void gridDynGenModel6::residual (const IOdata &args, const stateData *sD, double resid[],  const solverMode &sMode)
{
  Lp Loc = offsets.getLocations (sD,resid, sMode, this);

  const double *gm = Loc.algStateLoc;
  const double *gmd = Loc.diffStateLoc;
  const double *gmp = Loc.dstateLoc;

  double *rva = Loc.destLoc;
  double *rvd = Loc.destDiffLoc;
  updateLocalCache (args, sD, sMode);

  if (hasAlgebraic (sMode))
    {
      // Id and Iq
      rva[0] = Vd + Rs * gm[0] + (Xqpp) * gm[1] - gmd[4];
      rva[1] = Vq + Rs * gm[1] - (Xdpp) * gm[0] - gmd[5];
    }

  if (hasDifferential (sMode))
    {
      derivative (args, sD, resid, sMode);
      /// delta
      rvd[0] -= gmp[0];
      rvd[1] -= gmp[1];
      rvd[2] -= gmp[2];
      rvd[3] -= gmp[3];
      rvd[4] -= gmp[4];
      rvd[5] -= gmp[5];
    }



}



void gridDynGenModel6::jacobianElements (const IOdata &args, const stateData *sD,
                                         arrayData<double> *ad,
                                         const IOlocs &argLocs, const solverMode &sMode)
{
  Lp Loc = offsets.getLocations  (sD, sMode, this);

  double V = args[voltageInLocation];
  const double *gm = Loc.algStateLoc;
  const double *gmd = Loc.diffStateLoc;

  updateLocalCache (args, sD, sMode);

  auto refAlg = Loc.algOffset;
  auto refDiff = Loc.diffOffset;


  auto VLoc = argLocs[voltageInLocation];
  auto TLoc = argLocs[angleInLocation];



  // P
  if (hasAlgebraic (sMode))
    {
      if (TLoc != kNullLocation)
        {
          ad->assign (refAlg, TLoc, Vq);
          ad->assign (refAlg + 1, TLoc, -Vd);
        }

      // Q
      if (VLoc != kNullLocation)
        {
          ad->assign (refAlg, VLoc, Vd / V);
          ad->assign (refAlg + 1, VLoc, Vq / V);
        }

      ad->assign (refAlg, refAlg, Rs);
      ad->assign (refAlg, refAlg + 1, (Xqpp));



      ad->assign (refAlg + 1, refAlg, -(Xdpp));
      ad->assign (refAlg + 1, refAlg + 1, Rs);

      if (isAlgebraicOnly (sMode))
        {
          return;
        }

      // Id Differential
      ad->assign (refAlg, refDiff, -Vq);
      ad->assign (refAlg, refDiff + 4, -1);

      // Iq Differential
      ad->assign (refAlg + 1, refDiff, Vd);
      ad->assign (refAlg + 1, refDiff + 5, -1.0);
    }


  // delta
  ad->assign (refDiff, refDiff, -sD->cj);
  ad->assign (refDiff, refDiff + 1, m_baseFreq);

  // omega
  double kVal = -0.5  / H;
  if (hasAlgebraic (sMode))
    {
      ad->assign (refDiff + 1, refAlg, -0.5 *  (gmd[4] + (Xdpp - Xqpp) * gm[1]) / H);
      ad->assign (refDiff + 1, refAlg + 1, -0.5 *  (gmd[5] + (Xdpp - Xqpp) * gm[0]) / H);
    }

  ad->assign (refDiff + 1, refDiff + 1, -0.5  * D / H - sD->cj);
  ad->assign (refDiff + 1, refDiff + 4, -0.5 * gm[0] / H);
  ad->assign (refDiff + 1, refDiff + 5, -0.5 *  gm[1] / H);


  ad->assignCheckCol (refDiff + 1, argLocs[genModelPmechInLocation], -kVal); // governor: Pm


  // Ed' and Eq'
  if (hasAlgebraic (sMode))
    {
      ad->assign (refDiff + 2, refAlg + 1, -(Xq - Xqp) / Tqop);
      ad->assign (refDiff + 3, refAlg, (Xd - Xdp) / Tdop);
    }
  ad->assign (refDiff + 2, refDiff + 2, -1.0 / Tqop - sD->cj);
  ad->assign (refDiff + 3, refDiff + 3, -1.0 / Tdop - sD->cj);


  ad->assignCheckCol (refDiff + 3, argLocs[genModelEftInLocation], 1.0 / Tdop); // exciter: Ef
  //Edpp
  if (hasAlgebraic (sMode))
    {
      ad->assign (refDiff + 4, refAlg + 1, -(Xqp - Xqpp) / Tqopp);
    }
  ad->assign (refDiff + 4, refDiff + 2, 1.0 / Tqopp);
  ad->assign (refDiff + 4, refDiff + 4, -1.0 / Tqopp - sD->cj);


  //Eqpp
  if (hasAlgebraic (sMode))
    {
      ad->assign (refDiff + 5, refAlg, (Xdp - Xdpp) / Tdopp);
    }
  ad->assign (refDiff + 5, refDiff + 3, 1.0 / Tdopp);
  ad->assign (refDiff + 5, refDiff + 5, -1.0 / Tdopp - sD->cj);

}


static const stringVec genModel6Names {
  "id","iq","delta","freq","edp","eqp","edpp","eqpp"
};

stringVec gridDynGenModel6::localStateNames () const
{
  return genModel6Names;
}

