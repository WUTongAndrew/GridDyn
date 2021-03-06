/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil;  eval: (c-set-offset 'innamespace 0); -*- */
/*
* LLNS Copyright Start
* Copyright (c) 2014, Lawrence Livermore National Security
* This work was performed under the auspices of the U.S. Department
* of Energy by Lawrence Livermore National Laboratory in part under
* Contract W-7405-Eng-48 and in part under Contract DE-AC52-07NA27344.
* Produced at the Lawrence Livermore National Laboratory.
* All rights reserved.
* For details, see the LICENSE file.
* LLNS Copyright End
*/

#include "solvers/sundialsArrayData.h"
#include <algorithm>
#include <cstring>

arrayDataSundialsDense::arrayDataSundialsDense ()
{
}

arrayDataSundialsDense::arrayDataSundialsDense (DlsMat mat) : J (mat)
{
  rowLim = static_cast<count_t> (J->M);
  colLim = static_cast<count_t> (J->N);
}
void arrayDataSundialsDense::clear ()
{
  memset (J->data, 0, sizeof(realtype) * J->ldata);
}

void arrayDataSundialsDense::assign (index_t X, index_t Y, double num)
{
  DENSE_ELEM (J, X, Y) += num;
}

void arrayDataSundialsDense::setMatrix (DlsMat mat)
{
  J = mat;
  rowLim = static_cast<count_t> (J->M);
  colLim = static_cast<count_t> (J->N);
}

count_t arrayDataSundialsDense::size () const
{
  return static_cast<count_t> (J->M * J->N);
}

count_t arrayDataSundialsDense::capacity () const
{
  return static_cast<count_t> (J->M * J->N);
}

index_t arrayDataSundialsDense::rowIndex (index_t N) const
{
  return static_cast<index_t> (N % J->N);
}

index_t arrayDataSundialsDense::colIndex (index_t N) const
{
  return static_cast<index_t> (N / J->N);
}

double arrayDataSundialsDense::val (index_t N) const
{
  return J->data[N];
}

double arrayDataSundialsDense::at (index_t rowN, index_t colN) const
{
  return DENSE_ELEM (J, rowN, colN);
}



arrayDataSundialsSparse::arrayDataSundialsSparse ()
{
}

arrayDataSundialsSparse::arrayDataSundialsSparse (SlsMat mat) : J (mat)
{
  rowLim = static_cast<count_t> (J->M);
  colLim = static_cast<count_t> (J->N);
}

void arrayDataSundialsSparse::clear ()
{
  memset (J->data, 0, sizeof(realtype) * J->NNZ);
}

void arrayDataSundialsSparse::assign (index_t X, index_t Y, double num)
{

  int sti = J->colptrs[Y];
  int stp = J->colptrs[Y + 1];
  auto st = J->rowvals + sti;
  while (sti < stp)
    {
      if (*st == static_cast<int> (X))
        {

          J->data[sti] += num;
          break;
        }
      ++st;
      ++sti;
    }
}

void arrayDataSundialsSparse::setMatrix (SlsMat mat)
{
  J = mat;
  rowLim = static_cast<count_t> (J->M);
  colLim = static_cast<count_t> (J->N);
}


count_t arrayDataSundialsSparse::size () const
{
  return static_cast<count_t> (J->colptrs[colLim]);
}

count_t arrayDataSundialsSparse::capacity () const
{
  return static_cast<count_t> (J->NNZ);
}

index_t arrayDataSundialsSparse::rowIndex (index_t N) const
{
  return static_cast<index_t> (J->rowvals[N]);
}

index_t arrayDataSundialsSparse::colIndex (index_t N) const
{
  auto res = std::lower_bound (J->colptrs, &(J->colptrs[colLim]), static_cast<int> (N));
  return *res - 1;
}

void arrayDataSundialsSparse::start()
{
	cur = 0;
	ccol = 0;
}

data_triple<double> arrayDataSundialsSparse::next()
{
	data_triple<double> ret{ static_cast<index_t>(J->rowvals[cur]), ccol, J->data[cur] };
	++cur;
	if (static_cast<int>(cur) >= J->colptrs[ccol+1])
	{
		++ccol;
		if (ccol > colLim)
		{
			--cur;
			--ccol;
		}
	}
	return ret;

}

double arrayDataSundialsSparse::val (index_t N) const
{
  return J->data[N];
}

double arrayDataSundialsSparse::at (index_t rowN, index_t colN) const
{
  if (static_cast<int> (colN) > J->M)
    {
      return 0;
    }
  int sti = J->colptrs[colN];
  int stp = J->colptrs[colN + 1];
  for (int kk = sti; kk < stp; ++kk)
    {
      if (J->rowvals[kk] == static_cast<int> (rowN))
        {
          return J->data[kk];
        }
    }
  return 0;
}
