/*
 * $Id$
 *
 * Copyright (c) 2008 Vyacheslav Frolov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * $Log$
 * Revision 1.5  2008/10/16 06:19:12  vfrolov
 * Divided filter ID to filter group ID and filter name
 *
 * Revision 1.4  2008/09/26 15:34:50  vfrolov
 * Fixed adding order for filters with the same FID
 *
 * Revision 1.3  2008/08/20 08:32:35  vfrolov
 * Implemented Filters::FilterName()
 *
 * Revision 1.2  2008/04/16 14:13:59  vfrolov
 * Added ability to specify source posts for OUT method
 *
 * Revision 1.1  2008/03/26 08:35:32  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "port.h"
#include "comhub.h"
#include "filters.h"
#include "bufutils.h"
#include "hubmsg.h"
#include "utils.h"

///////////////////////////////////////////////////////////////
class Filter {
  public:
    Filter(
        const char *pGroup,
        const char *pName,
        HFILTER _hFilter,
        FILTER_INIT *_pInit,
        FILTER_IN_METHOD *_pInMethod,
        FILTER_OUT_METHOD *_pOutMethod)
      : group(pGroup),
        name(pName),
        hFilter(_hFilter),
        pInit(_pInit),
        pInMethod(_pInMethod),
        pOutMethod(_pOutMethod) {}

    const string group;
    const string name;
    const HFILTER hFilter;
    FILTER_INIT *const pInit;
    FILTER_IN_METHOD *const pInMethod;
    FILTER_OUT_METHOD *const pOutMethod;
};
///////////////////////////////////////////////////////////////
class FilterMethod {
  public:
    FilterMethod(const Filter &_filter, BOOL _isInMethod, const set<int> *_pSrcPorts)
      : filter(_filter),
        isInMethod(_isInMethod),
        pSrcPorts(_pSrcPorts) {}

    ~FilterMethod() {
      if (pSrcPorts)
        delete pSrcPorts;
    }

    const Filter &filter;
    const BOOL isInMethod;
    const set<int> *const pSrcPorts;
};
///////////////////////////////////////////////////////////////
typedef pair<int, FilterMethodArray*> PortFilters;
///////////////////////////////////////////////////////////////
Filters::~Filters()
{
  for (PortFiltersMap::const_iterator iPort = portFilters.begin() ; iPort != portFilters.end() ; iPort++) {
    if (iPort->second) {
      for (FilterMethodArray::const_iterator i = iPort->second->begin() ; i != iPort->second->end() ; i++) {
        if (*i)
          delete *i;
      }
      delete iPort->second;
    }
  }

  for (FilterArray::const_iterator i = allFilters.begin() ; i != allFilters.end() ; i++) {
    if (*i)
      delete *i;
  }
}
///////////////////////////////////////////////////////////////
BOOL Filters::CreateFilter(
    const FILTER_ROUTINES_A *pFltRoutines,
    const char *pFilterGroup,
    const char *pFilterName,
    HCONFIG hConfig,
    const char *pArgs)
{
  if (!ROUTINE_IS_VALID(pFltRoutines, pCreate)) {
    cerr << "No create routine for filter " << pFilterName << endl;
    return FALSE;
  }

  int argc;
  const char **argv;
  void *pTmpArgs;

  CreateArgsVector(pFilterName, pArgs, &argc, &argv, &pTmpArgs);

  HFILTER hFilter = pFltRoutines->pCreate(hConfig, argc, argv);

  FreeArgsVector(argv, pTmpArgs);

  if (!hFilter) {
    cerr << "Can't create filter " << pFilterName << endl;
    return FALSE;
  }

  Filter *pFilter = new Filter(
      pFilterGroup,
      pFilterName,
      hFilter,
      ROUTINE_GET(pFltRoutines, pInit),
      ROUTINE_GET(pFltRoutines, pInMethod),
      ROUTINE_GET(pFltRoutines, pOutMethod));

  if (!pFilter) {
    cerr << "No enough memory." << endl;
    return FALSE;
  }

  allFilters.push_back(pFilter);

  return TRUE;
}
///////////////////////////////////////////////////////////////
BOOL Filters::AddFilter(
    int iPort,
    const char *pGroup,
    BOOL addInMethod,
    BOOL addOutMethod,
    const set<int> *pOutMethodSrcPorts)
{
  PortFiltersMap::iterator iPair = portFilters.find(iPort);

  if (iPair == portFilters.end()) {
    portFilters.insert(PortFilters(iPort, NULL));

    iPair = portFilters.find(iPort);

    if (iPair == portFilters.end()) {
      cerr << "Can't add filters for port " << iPort << endl;
      return FALSE;
    }
  }

  if (!iPair->second) {
    iPair->second = new FilterMethodArray;

    if (!iPair->second) {
      cerr << "No enough memory." << endl;
      return FALSE;
    }
  }

  BOOL found = FALSE;

  for (FilterArray::const_iterator i = allFilters.begin() ; i != allFilters.end() ; i++) {
    if (*i && (*i)->group == pGroup) {
      if (addInMethod && (*i)->pInMethod) {
        FilterMethod *pFilterMethod = new FilterMethod(*(*i), TRUE, NULL);

        if (!pFilterMethod) {
          cerr << "No enough memory." << endl;
          return FALSE;
        }

        iPair->second->push_back(pFilterMethod);
      }

      if (addOutMethod && (*i)->pOutMethod) {
        const set<int> *pSrcPorts;

        if (pOutMethodSrcPorts) {
          pSrcPorts = new set<int>(*pOutMethodSrcPorts);

          if (!pSrcPorts) {
            cerr << "No enough memory." << endl;
            return FALSE;
          }
        } else {
          pSrcPorts = NULL;
        }

        FilterMethod *pFilterMethod = new FilterMethod(*(*i), FALSE, pSrcPorts);

        if (!pFilterMethod) {
          cerr << "No enough memory." << endl;
          return FALSE;
        }

        iPair->second->push_back(pFilterMethod);
      }

      found = TRUE;
    }
  }

  if (!found) {
    cerr << "Can't find any filter for group " << pGroup << endl;
    return FALSE;
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
void Filters::Report() const
{
  if (!portFilters.size())
    return;

  cout << "Filters:" << endl;

  for (PortFiltersMap::const_iterator iPort = portFilters.begin() ; iPort != portFilters.end() ; iPort++) {
    stringstream bufs[3];

    Port *pPort = hub.GetPort(iPort->first);

    if (pPort)
      bufs[1] << pPort->Name() << " ";
    else
      bufs[1] << iPort->first << " ";

    for (int i = 0 ; i < 3 ; i++) {
      string::size_type diff = bufs[1].str().length() - bufs[i].str().length();

      for (string::size_type j = 0 ; j < diff ; j++) {
        bufs[i] << (i == 0 ? " " : "_");
      }
    }

    cout << bufs[2].str() << endl;

    bufs[0] << "\\-";
    bufs[1] << "|  ";
    bufs[2] << "/<-";

    FilterMethodArray *pFilters = iPort->second;

    if (pFilters) {
      for (FilterMethodArray::const_iterator i = pFilters->begin() ; i != pFilters->end() ; i++) {
        if ((*i)->isInMethod) {
          bufs[0] << ">{" << (*i)->filter.name << ".IN" << "}-";
          string::size_type len = (*i)->filter.name.length();

          for (int i = 1 ; i < 3 ; i++) {
            string::size_type diff = bufs[0].str().length() - bufs[i].str().length();

            for (string::size_type j = len/2 + 6 ; j < diff ; j++)
              bufs[i] << (i == 2 ? "-" : " ");
          }

          bufs[1] << "/";
          bufs[2] << "-";
        } else {
          bufs[2] << "{" << (*i)->filter.name << ".OUT";
          if ((*i)->pSrcPorts) {
            bufs[2] << "(";
            for (set<int>::const_iterator iSrc = (*i)->pSrcPorts->begin() ;
                 iSrc != (*i)->pSrcPorts->end() ; iSrc++)
            {
              if (iSrc != (*i)->pSrcPorts->begin())
                bufs[2] << ",";

              bufs[2] << *iSrc;
            }
            bufs[2] << ")";
          }
          bufs[2] << "}<-";

          string::size_type len = (*i)->filter.name.length();

          for (int i = 0 ; i < 2 ; i++) {
            string::size_type diff = bufs[2].str().length() - bufs[i].str().length();

            for (string::size_type j = len/2 + 7 ; j < diff ; j++)
              bufs[i] << (i == 0 ? "-" : " ");
          }
        }
      }
    }

    if (bufs[2].str().length() > bufs[0].str().length()) {
      string::size_type diff = bufs[2].str().length() - (bufs[0].str().length() + 1);

      for (string::size_type j = 0 ; j < diff ; j++)
        bufs[0] << "-";
    } else {
      string::size_type diff = bufs[0].str().length() + 1 - bufs[2].str().length();

      for (string::size_type j = 0 ; j < diff ; j++)
        bufs[2] << "-";
    }

    bufs[0] << ">";

    for (int i = 0 ; i < 3 ; i++) {
      cout << bufs[i].str() << endl;
    }

    cout << endl;
  }
}
///////////////////////////////////////////////////////////////
BOOL Filters::Init() const
{
  BOOL res = TRUE;

  for (FilterArray::const_iterator i = allFilters.begin() ; i != allFilters.end() ; i++) {
    if (*i && (*i)->pInit) {
      if (!(*i)->pInit((*i)->hFilter, (HHUB)&hub))
        res = FALSE;
    }
  }

  return res;
}
///////////////////////////////////////////////////////////////
const char *Filters::FilterName(HFILTER hFilter) const
{
  for (FilterArray::const_iterator i = allFilters.begin() ; i != allFilters.end() ; i++) {
    if (*i && (*i)->hFilter == hFilter)
      return (*i)->name.c_str();
  }

  return NULL;
}
///////////////////////////////////////////////////////////////
static BOOL InMethod(
    int nFromPort,
    const FilterMethodArray::const_iterator &i,
    const FilterMethodArray::const_iterator &iEnd,
    HubMsg *pInMsg,
    HubMsg **ppEchoMsg)
{
  _ASSERTE(*ppEchoMsg == NULL);

  HubMsg *pEchoMsg = NULL;

  if ((*i)->isInMethod) {
    FILTER_IN_METHOD *pInMethod = (*i)->filter.pInMethod;
    HFILTER hFilter = (*i)->filter.hFilter;

    HubMsg *pNextMsg = pInMsg;

    for (HubMsg *pCurMsg = pNextMsg ; pCurMsg ; pCurMsg = pNextMsg) {
      pNextMsg = pNextMsg->Next();

      HUB_MSG *pEchoMsgPart = NULL;

      if (!pInMethod(hFilter, nFromPort, pCurMsg, &pEchoMsgPart)) {
        if (pEchoMsgPart)
          delete (HubMsg *)pEchoMsgPart;
        return FALSE;
      }

      if (pEchoMsgPart) {
        if (pEchoMsg) {
          pEchoMsg->Merge((HubMsg *)pEchoMsgPart);
        } else {
          pEchoMsg = (HubMsg *)pEchoMsgPart;
        }
      }
    }
  }

  const FilterMethodArray::const_iterator iNext = i + 1;

  if (iNext != iEnd) {
    if (!InMethod(nFromPort, iNext, iEnd, pInMsg, ppEchoMsg))
      return FALSE;
  }

  if (pEchoMsg) {
    if (*ppEchoMsg)
      pEchoMsg->Merge(*ppEchoMsg);

    *ppEchoMsg = pEchoMsg;
  }

  if (!(*i)->isInMethod) {
    FILTER_OUT_METHOD *pOutMethod = (*i)->filter.pOutMethod;
    HFILTER hFilter = (*i)->filter.hFilter;

    HubMsg *pNextMsg = *ppEchoMsg;

    for (HubMsg *pCurMsg = pNextMsg ; pCurMsg ; pCurMsg = pNextMsg) {
      pNextMsg = pNextMsg->Next();

      if (!pOutMethod(hFilter, nFromPort, nFromPort, pCurMsg))
        return FALSE;
    }
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
BOOL Filters::InMethod(
    int nFromPort,
    HubMsg *pInMsg,
    HubMsg **ppEchoMsg) const
{
  _ASSERTE(*ppEchoMsg == NULL);

  PortFiltersMap::const_iterator iPair = portFilters.find(nFromPort);

  if (iPair == portFilters.end())
    return TRUE;

  FilterMethodArray *pFilters = iPair->second;

  if (!pFilters)
    return TRUE;

  const FilterMethodArray::const_iterator i = pFilters->begin();

  if (i != pFilters->end()) {
    if (!::InMethod(nFromPort, i, pFilters->end(), pInMsg, ppEchoMsg))
      return FALSE;
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
BOOL Filters::OutMethod(
    int nFromPort,
    int nToPort,
    HubMsg *pOutMsg) const
{
  PortFiltersMap::const_iterator iPair = portFilters.find(nToPort);

  if (iPair == portFilters.end())
    return TRUE;

  FilterMethodArray *pFilters = iPair->second;

  if (!pFilters)
    return TRUE;

  for (FilterMethodArray::const_reverse_iterator i = pFilters->rbegin() ; i != pFilters->rend() ; i++) {
    if (!(*i)->isInMethod && (!(*i)->pSrcPorts ||
        (*i)->pSrcPorts->find(nFromPort) != (*i)->pSrcPorts->end()))
    {
      FILTER_OUT_METHOD *pOutMethod = (*i)->filter.pOutMethod;
      HFILTER hFilter = (*i)->filter.hFilter;

      HubMsg *pNextMsg = pOutMsg;

      for (HubMsg *pCurMsg = pNextMsg ; pCurMsg ; pCurMsg = pNextMsg) {
        pNextMsg = pNextMsg->Next();

        if (!pOutMethod(hFilter, nFromPort, nToPort, pCurMsg))
          return FALSE;
      }
    }
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
