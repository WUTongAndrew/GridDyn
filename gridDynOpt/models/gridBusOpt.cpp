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

// headers
#include "gridAreaOpt.h"
#include "gridGenOpt.h"
#include "gridLoadOpt.h"
#include "gridLinkOpt.h"
#include "gridBusOpt.h"
#include "gridBus.h"
#include "generators/gridDynGenerator.h"
#include "loadModels/gridLoad.h"
#include "optObjectFactory.h"
#include "vectorOps.hpp"
#include "vectData.h"
#include "stringOps.h"

#include <cmath>
#include <utility>


static optObjectFactory<gridBusOpt, gridBus> opbus ("basic", "bus");

using namespace gridUnits;

gridBusOpt::gridBusOpt (const std::string &objName) : gridOptObject (objName)
{

}

gridBusOpt::gridBusOpt (gridCoreObject *obj, const std::string &objName) : gridOptObject (objName), bus (dynamic_cast<gridBus *> (obj))
{

  if (bus)
    {
      if (name.empty ())
        {
          name = bus->getName ();
        }
      id = bus->getUserID ();
    }
}

gridCoreObject *gridBusOpt::clone (gridCoreObject *obj) const
{
  gridBusOpt *nobj;
  if (obj == nullptr)
    {
      nobj = new gridBusOpt ();
    }
  else
    {
      nobj = dynamic_cast<gridBusOpt *> (obj);
      if (nobj == nullptr)
        {
          //if we can't cast the pointer clone at the next lower level
          gridOptObject::clone (obj);
          return obj;
        }
    }
  gridOptObject::clone (nobj);


  //now clone all the loads and generators
  //cloning the links from this component would be bad
  //clone the generators and loads


  for (size_t kk = 0; kk < genList.size (); ++kk)
    {
      if (kk >= nobj->genList.size ())
        {
          nobj->add (static_cast<gridDynGenerator *> (genList[kk]->clone (nullptr)));
        }
      else
        {
          genList[kk]->clone (nobj->genList[kk]);
        }
    }
  for (size_t kk = 0; kk < loadList.size (); ++kk)
    {
      if (kk >= nobj->loadList.size ())
        {
          nobj->add (static_cast<gridLoad *> (loadList[kk]->clone (nullptr)));
        }
      else
        {
          loadList[kk]->clone (nobj->loadList[kk]);
        }
    }
  return nobj;
}



void gridBusOpt::objectInitializeA (unsigned long flags)
{

  for (auto ld : loadList)
    {
      ld->objectInitializeA (flags);
    }
  for (auto gen : genList)
    {
      gen->objectInitializeA (flags);
    }
}

void gridBusOpt::loadSizes (const optimMode &oMode)
{
  optimOffsets *oo = offsets.getOffsets (oMode);
  oo->reset ();
  switch (oMode.flowMode)
    {
    case flowModel_t::none:
    case flowModel_t::transport:
      oo->local.constraintsSize = 1;
      break;
    case flowModel_t::dc:
      oo->local.aSize = 1;
      oo->local.constraintsSize = 1;
      break;
    case flowModel_t::ac:
      oo->local.aSize = 1;
      oo->local.vSize = 1;
      oo->local.constraintsSize = 2;
      break;
    }
  oo->localLoad ();
  for (auto ld : loadList)
    {
      ld->loadSizes (oMode);
      oo->addSizes (ld->offsets.getOffsets (oMode));
    }
  for (auto gen : genList)
    {
      gen->loadSizes (oMode);
      oo->addSizes (gen->offsets.getOffsets (oMode));
    }
  oo->loaded = true;
}


void gridBusOpt::setValues (const optimData *oD, const optimMode &oMode)
{
  for (auto ld : loadList)
    {
      ld->setValues (oD, oMode);
    }
  for (auto gen : genList)
    {
      gen->setValues (oD, oMode);
    }
}
//for saving the state
void gridBusOpt::guess (double ttime, double val[], const optimMode &oMode)
{
  for (auto ld : loadList)
    {
      ld->guess (ttime,val,oMode);
    }
  for (auto gen : genList)
    {
      gen->guess (ttime,val, oMode);
    }
}

void gridBusOpt::getVariableType (double sdata[], const optimMode &oMode)
{
  for (auto ld : loadList)
    {
      ld->getVariableType (sdata, oMode);
    }
  for (auto gen : genList)
    {
      gen->getVariableType (sdata, oMode);
    }
}

void gridBusOpt::getTols (double tols[], const optimMode &oMode)
{
  for (auto ld : loadList)
    {
      ld->getTols (tols, oMode);
    }
  for (auto gen : genList)
    {
      gen->getTols (tols, oMode);
    }
}


//void alert (gridCoreObject *object, int code);

void gridBusOpt::valueBounds (double ttime, double upperLimit[], double lowerLimit[], const optimMode &oMode)
{
  for (auto ld : loadList)
    {
      ld->valueBounds (ttime, upperLimit, lowerLimit, oMode);
    }
  for (auto gen : genList)
    {
      gen->valueBounds (ttime, upperLimit, lowerLimit, oMode);
    }
}

void gridBusOpt::linearObj (const optimData *oD, vectData *linObj, const optimMode &oMode)
{
  for (auto ld : loadList)
    {
      ld->linearObj (oD, linObj, oMode);
    }
  for (auto gen : genList)
    {
      gen->linearObj (oD, linObj, oMode);
    }
}
void gridBusOpt::quadraticObj (const optimData *oD, vectData *linObj, vectData *quadObj, const optimMode &oMode)
{
  for (auto ld : loadList)
    {
      ld->quadraticObj (oD, linObj, quadObj, oMode);
    }
  for (auto gen : genList)
    {
      gen->quadraticObj (oD, linObj, quadObj, oMode);
    }
}

double gridBusOpt::objValue (const optimData *oD, const optimMode &oMode)
{
  double cost = 0;
  for (auto ld : loadList)
    {
      cost += ld->objValue (oD, oMode);
    }
  for (auto gen : genList)
    {
      cost += gen->objValue (oD, oMode);
    }
  return cost;
}

void gridBusOpt::derivative (const optimData *oD, double deriv[], const optimMode &oMode)
{
  for (auto ld : loadList)
    {
      ld->derivative (oD, deriv, oMode);
    }
  for (auto gen : genList)
    {
      gen->derivative (oD, deriv, oMode);
    }
}
void gridBusOpt::jacobianElements (const optimData *oD, arrayData<double> *ad, const optimMode &oMode)
{
  for (auto ld : loadList)
    {
      ld->jacobianElements (oD, ad, oMode);
    }
  for (auto gen : genList)
    {
      gen->jacobianElements (oD, ad, oMode);
    }
}
void gridBusOpt::getConstraints (const optimData *oD, arrayData<double> *cons, double upperLimit[], double lowerLimit[], const optimMode &oMode)
{
  for (auto ld : loadList)
    {
      ld->getConstraints (oD, cons, upperLimit,lowerLimit,oMode);
    }
  for (auto gen : genList)
    {
      gen->getConstraints (oD, cons, upperLimit, lowerLimit, oMode);
    }
}

void gridBusOpt::constraintValue (const optimData *oD, double cVals[], const optimMode &oMode)
{
  for (auto ld : loadList)
    {
      ld->constraintValue (oD, cVals, oMode);
    }
  for (auto gen : genList)
    {
      gen->constraintValue (oD, cVals, oMode);
    }
}

void gridBusOpt::constraintJacobianElements (const optimData *oD, arrayData<double> *ad, const optimMode &oMode)
{
  for (auto ld : loadList)
    {
      ld->constraintJacobianElements (oD, ad, oMode);
    }
  for (auto gen : genList)
    {
      gen->constraintJacobianElements (oD, ad, oMode);
    }
}


void gridBusOpt::getObjName (stringVec &objNames, const optimMode &oMode, const std::string &prefix)
{
  for (auto ld : loadList)
    {
      ld->getObjName (objNames, oMode,prefix);
    }
  for (auto gen : genList)
    {
      gen->getObjName (objNames, oMode, prefix);
    }
}


void gridBusOpt::disable ()
{
  enabled = false;
  for (auto &link : linkList)
    {
      link->disable ();
    }
}

void gridBusOpt::setOffsets (const optimOffsets &newOffsets, const optimMode &oMode)
{
  offsets.setOffsets (newOffsets,oMode);
  optimOffsets no (offsets.getOffsets (oMode));
  no.localLoad ();
  for (auto ld : loadList)
    {
      ld->setOffsets (no, oMode);
      no.increment (ld->offsets.getOffsets (oMode));
    }
  for (auto gen : genList)
    {
      gen->setOffsets (no, oMode);
      no.increment (gen->offsets.getOffsets (oMode));
    }


}

void gridBusOpt::setOffset (index_t offset, index_t constraintOffset, const optimMode &oMode)
{

  for (auto ld : loadList)
    {
      ld->setOffset (offset, constraintOffset, oMode);
      constraintOffset += ld->constraintSize (oMode);
      offset += ld->objSize (oMode);

    }
  for (auto gen : genList)
    {
      gen->setOffset (offset, constraintOffset, oMode);
      constraintOffset += gen->constraintSize (oMode);
      offset += gen->objSize (oMode);
    }

  offsets.setConstraintOffset (constraintOffset, oMode);
  offsets.setOffset (offset, oMode);

}


// destructor
gridBusOpt::~gridBusOpt ()
{
  for (auto &ld : loadList)
    {
      condDelete (ld, this);
    }

  for (auto &gen : genList)
    {
      condDelete (gen, this);
    }
}

int gridBusOpt::add (gridCoreObject *obj)
{
  auto ld = dynamic_cast<gridLoadOpt *> (obj);
  if (ld)
    {
      return add (ld);
    }

  auto gen = dynamic_cast<gridGenOpt *> (obj);
  if (gen)
    {
      return add (gen);
    }

  auto lnk = dynamic_cast<gridLinkOpt *> (obj);
  if (lnk)
    {
      return add (lnk);
    }

  if (dynamic_cast<gridBus *> (obj))
    {
      bus = static_cast<gridBus *> (obj);
      if (name.empty ())
        {
          name = bus->getName ();
        }
      id = bus->getUserID ();
      return OBJECT_ADD_SUCCESS;
    }

  return OBJECT_NOT_RECOGNIZED;
}

// add load
int gridBusOpt::add (gridLoadOpt *ld)
{
  gridCoreObject *obj = find (ld->getName ());
  if (obj == nullptr)
    {
      ld->locIndex = static_cast<index_t> (loadList.size ());
      loadList.push_back (ld);
      ld->setParent (this);
      return OBJECT_ADD_SUCCESS;
    }
  else if (ld->getID () == obj->getID ())
    {
      return OBJECT_ALREADY_MEMBER;
    }
  return OBJECT_ADD_FAILURE;
}

// add generator
int gridBusOpt::add (gridGenOpt *gen)
{
  gridCoreObject *obj = find (gen->getName ());
  if (obj == nullptr)
    {
      gen->locIndex = static_cast<index_t> (genList.size ());
      genList.push_back (gen);
      gen->setParent (this);
      return OBJECT_ADD_SUCCESS;
    }
  else if (gen->getID () == obj->getID ())
    {
      return OBJECT_ALREADY_MEMBER;
    }
  return (-2);
}

// add link
int gridBusOpt::add (gridLinkOpt *lnk)
{
  for (auto &links : linkList)
    {
      if (links->getID () == lnk->getID ())
        {
          return OBJECT_ALREADY_MEMBER;
        }
    }
  linkList.push_back (lnk);
  return OBJECT_ADD_SUCCESS;
}

int gridBusOpt::remove (gridCoreObject *obj)
{
  gridLoadOpt *ld = dynamic_cast<gridLoadOpt *> (obj);
  if (ld)
    {
      return (remove (ld));
    }

  gridGenOpt *gen = dynamic_cast<gridGenOpt *> (obj);
  if (gen)
    {
      return(remove (gen));
    }

  gridLinkOpt *lnk = dynamic_cast<gridLinkOpt *> (obj);
  if (lnk)
    {
      return(remove (lnk));
    }
  return OBJECT_REMOVE_SUCCESS;
}

// remove load
int gridBusOpt::remove (gridLoadOpt *ld)
{
  size_t kk;
  int out = OBJECT_REMOVE_FAILURE;
  for (kk = 0; kk < loadList.size (); ++kk)
    {
      if (ld == loadList[kk])
        {
          loadList[kk]->setParent (nullptr);
          loadList.erase (loadList.begin () + kk);
          out = OBJECT_REMOVE_SUCCESS;
          break;
        }
    }
  return out;
}

// remove generator
int gridBusOpt::remove (gridGenOpt *gen)
{
  size_t kk;
  int out = OBJECT_REMOVE_FAILURE;
  for (kk = 0; kk < genList.size (); ++kk)
    {
      if (gen == genList[kk])
        {
          genList[kk]->setParent (nullptr);
          genList.erase (genList.begin () + kk);
          out = OBJECT_REMOVE_SUCCESS;
          break;
        }
    }
  return out;
}

// remove link
int gridBusOpt::remove (gridLinkOpt *lnk)
{
  size_t kk;
  int out = OBJECT_REMOVE_FAILURE;
  for (kk = 0; kk < linkList.size (); ++kk)
    {
      if (lnk == linkList[kk])
        {
          linkList.erase (linkList.begin () + kk);
          out = OBJECT_REMOVE_SUCCESS;
          break;
        }
    }
  return out;
}





void gridBusOpt::setAll (const std::string &type, std::string param, double val, gridUnits::units_t unitType)
{

  if ((type == "gen") || (type == "generator"))
    {
      for (auto &gen : genList)
        {
          gen->set (param, val, unitType);
        }
    }
  else if (type == "load")
    {
      for (auto &ld : loadList)
        {
          ld->set (param, val, unitType);
        }
    }

}

// set properties
int gridBusOpt::set (const std::string &param,  const std::string &val)
{
  int out = PARAMETER_FOUND;
  if (param == "status")
    {
      auto v2 = convertToLowerCase (val);
      if (v2 == "out")
        {
          if (enabled)
            {
              disable ();
            }

        }
      else if (v2 == "in")
        {
          if (!enabled)
            {
              enable ();
            }
        }
      else
        {
          out = gridOptObject::set (param, val);
        }
    }
  else
    {
      out = gridOptObject::set (param, val);
    }
  return out;
}

int gridBusOpt::set (const std::string &param, double val, units_t unitType)
{
  int out = PARAMETER_FOUND;

  if ((param == "voltagetolerance")||(param == "vtol"))
    {

    }
  else if ((param == "angleetolerance") || (param == "atol"))
    {

    }
  else
    {
      out = gridOptObject::set (param, val, unitType);
    }


  return out;
}




gridCoreObject *gridBusOpt::find (const std::string &objname) const
{
  gridCoreObject *obj = nullptr;
  if ((objname == this->name) || (objname == "bus"))
    {
      return const_cast<gridBusOpt *> (this);
    }
  if (objname == "root")
    {
      if (parent)
        {
          return (parent->find (objname));
        }
      else
        {
          return const_cast<gridBusOpt *> (this);
        }
    }
  for (auto ob : genList)
    {
      if (objname == ob->getName ())
        {
          obj = ob;
          break;
        }
    }
  if (obj == nullptr)
    {
      for (auto ob : loadList)
        {
          if (objname == ob->getName ())
            {
              obj = ob;
              break;
            }
        }
    }
  return obj;
}

gridCoreObject *gridBusOpt::getSubObject (const std::string &typeName, index_t num) const
{
  if (typeName == "link")
    {
      return getLink (num - 1);
    }
  else if (typeName == "load")
    {
      return getLoad (num - 1);
    }
  else if ((typeName == "gen") || (typeName == "generator"))
    {
      return getGen (num - 1);
    }
  else
    {
      return nullptr;
    }
}

gridCoreObject *gridBusOpt::findByUserID (const std::string &typeName, index_t searchID) const
{
  if (typeName == "load")
    {
      for (auto &LD : loadList)
        {
          if (LD->getUserID () == searchID)
            {
              return LD;
            }
        }
    }
  else if ((typeName == "gen") || (typeName == "generator"))
    {
      for (auto &gen : genList)
        {
          if (gen->getUserID () == searchID)
            {
              return gen;
            }
        }
    }
  return nullptr;
}

gridOptObject *gridBusOpt::getLink (index_t x) const
{
  return (x < linkList.size ()) ? linkList[x] : nullptr;

}

gridOptObject *gridBusOpt::getLoad (index_t x) const
{
  return (x < loadList.size ()) ? loadList[x] : nullptr;
}

gridOptObject *gridBusOpt::getGen (index_t x) const
{
  return (x < genList.size ()) ? genList[x] : nullptr;
}


double gridBusOpt::get (const std::string &param, gridUnits::units_t unitType) const
{
  double fval = kNullVal;
  count_t ival = kInvalidCount;
  if (param == "gencount")
    {
      ival = static_cast<count_t> (genList.size ());
    }
  else if (param == "linkcount")
    {
      ival = static_cast<count_t> (linkList.size ());
    }
  else if (param == "loadcount")
    {
      ival = static_cast<count_t> (loadList.size ());
    }
  else
    {
      fval = gridCoreObject::get (param,unitType);
    }
  return (ival != kInvalidCount) ? static_cast<double> (ival) : fval;
}




gridBusOpt * getMatchingBus (gridBusOpt *bus, gridOptObject *src, gridOptObject *sec)
{
  gridBusOpt *B2 = nullptr;
  if (bus->getParent () == nullptr)
    {
      return nullptr;
    }
  if (bus->getParent ()->getID () == src->getID ())    //if this is true then things are easy
    {
      B2 = static_cast<gridBusOpt *> (sec->getBus (bus->locIndex));
    }
  else
    {
      gridOptObject *par;
      std::vector<index_t> lkind;
      par = dynamic_cast<gridOptObject *> (bus->getParent ());
      if (par == nullptr)
        {
          return nullptr;
        }
      lkind.push_back (bus->locIndex);
      while (par->getID () != src->getID ())
        {
          lkind.push_back (par->locIndex);
          par = dynamic_cast<gridOptObject *> (par->getParent ());
          if (par == nullptr)
            {
              return nullptr;
            }
        }
      //now work our way backwards through the secondary
      par = sec;
      for (auto kk = lkind.size () - 1; kk > 0; --kk)
        {
          par = par->getArea (lkind[kk]);
        }
      B2 = static_cast<gridBusOpt *> (par->getBus (lkind[0]));

    }
  return B2;
}
