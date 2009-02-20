/*
 * $Id$
 *
 * Copyright (c) 2008-2009 Vyacheslav Frolov
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
 * Revision 1.10  2009/02/20 18:32:35  vfrolov
 * Added info about location of options
 *
 * Revision 1.9  2009/02/04 12:26:54  vfrolov
 * Implemented --load option for filters
 *
 * Revision 1.8  2009/02/02 15:21:42  vfrolov
 * Optimized filter's API
 *
 * Revision 1.7  2008/11/24 12:36:59  vfrolov
 * Changed plugin API
 *
 * Revision 1.6  2008/11/13 08:07:40  vfrolov
 * Changed for staticaly linking
 *
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
#include "plugins/plugins_api.h"

#include "port.h"
#include "comhub.h"
#include "filters.h"
#include "filter.h"
#include "bufutils.h"
#include "hubmsg.h"
#include "utils.h"

///////////////////////////////////////////////////////////////
typedef pair<Port *, FilterInstanceArray*> PortFilters;
///////////////////////////////////////////////////////////////
Filters::~Filters()
{
  for (PortFiltersMap::const_iterator iPort = portFilters.begin() ; iPort != portFilters.end() ; iPort++) {
    if (iPort->second) {
      for (FilterInstanceArray::const_iterator i = iPort->second->begin() ; i != iPort->second->end() ; i++) {
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
  Filter *pFilter = new Filter(
      pFilterGroup,
      pFilterName,
      ROUTINE_GET(pFltRoutines, pCreateInstance),
      ROUTINE_GET(pFltRoutines, pInMethod),
      ROUTINE_GET(pFltRoutines, pOutMethod));

  if (!pFilter) {
    cerr << "No enough memory." << endl;
    return FALSE;
  }

  if (ROUTINE_IS_VALID(pFltRoutines, pCreate)) {
    int argc;
    const char **argv;
    Args args;

    CreateArgsVector(pFilterName, pArgs, args, &argc, &argv);

    HFILTER hFilter = pFltRoutines->pCreate((HMASTERFILTER)pFilter, hConfig, argc, argv);

    FreeArgsVector(argv);

    if (!hFilter) {
      delete pFilter;
      return FALSE;
    }

    pFilter->hFilter = hFilter;
  }

  allFilters.push_back(pFilter);

  return TRUE;
}
///////////////////////////////////////////////////////////////
BOOL Filters::AddFilter(
    Port *pPort,
    const char *pGroup,
    BOOL addInMethod,
    BOOL addOutMethod,
    const set<Port *> *pOutMethodSrcPorts)
{
  _ASSERTE(pPort != NULL);
  _ASSERTE(pGroup != NULL);

  PortFiltersMap::iterator iPair = portFilters.find(pPort);

  if (iPair == portFilters.end()) {
    portFilters.insert(PortFilters(pPort, NULL));

    iPair = portFilters.find(pPort);

    if (iPair == portFilters.end()) {
      cerr << "Can't add filters for port " << pPort->Name() << endl;
      return FALSE;
    }
  }

  if (!iPair->second) {
    iPair->second = new FilterInstanceArray;

    if (!iPair->second) {
      cerr << "No enough memory." << endl;
      return FALSE;
    }
  }

  BOOL found = FALSE;

  for (FilterArray::const_iterator i = allFilters.begin() ; i != allFilters.end() ; i++) {
    if (*i && (*i)->group == pGroup) {
      if ((addInMethod && (*i)->pInMethod) || (addOutMethod && (*i)->pOutMethod)) {
        const set<Port *> *pSrcPorts;

        if (pOutMethodSrcPorts) {
          pSrcPorts = new set<Port *>(*pOutMethodSrcPorts);

          if (!pSrcPorts) {
            cerr << "No enough memory." << endl;
            return FALSE;
          }
        } else {
          pSrcPorts = NULL;
        }

        FilterInstance *pFilterInstance = new FilterInstance(*(*i), *pPort, addInMethod, addOutMethod, pSrcPorts);

        if (!pFilterInstance) {
          cerr << "No enough memory." << endl;

          if (pSrcPorts)
            delete pSrcPorts;

          return FALSE;
        }

        if ((*i)->pCreateInstance) {
          HFILTERINSTANCE hFilterInstance = (*i)->pCreateInstance((HMASTERFILTERINSTANCE)pFilterInstance);

          if (!hFilterInstance) {
            cerr << "Can't create instance of filter " << (*i)->name << " for port " << pPort->Name() << endl;
            delete pFilterInstance;
            return FALSE;
          }

          pFilterInstance->hFilterInstance = hFilterInstance;
        }

        iPair->second->push_back(pFilterInstance);
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

    Port *pPort = iPort->first;

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

    FilterInstanceArray *pFilters = iPort->second;

    if (pFilters) {
      for (FilterInstanceArray::const_iterator i = pFilters->begin() ; i != pFilters->end() ; i++) {
        if ((*i)->pInMethod) {
          bufs[0] << ">{" << (*i)->filter.name << ".IN" << "}-";
          string::size_type len = (*i)->filter.name.length();

          for (int i = 1 ; i < 3 ; i++) {
            string::size_type diff = bufs[0].str().length() - bufs[i].str().length();

            for (string::size_type j = len/2 + 6 ; j < diff ; j++)
              bufs[i] << (i == 2 ? "-" : " ");
          }

          bufs[1] << "/";
          bufs[2] << "-";
        }

        if ((*i)->pOutMethod) {
          bufs[2] << "{" << (*i)->filter.name << ".OUT";
          if ((*i)->pSrcPorts) {
            bufs[2] << "(";
            for (set<Port *>::const_iterator iSrc = (*i)->pSrcPorts->begin() ;
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
BOOL Filters::InMethod(
    Port *pFromPort,
    const FilterInstanceArray::const_iterator &i,
    const FilterInstanceArray::const_iterator &iEnd,
    HubMsg *pInMsg,
    HubMsg **ppEchoMsg)
{
  _ASSERTE(*ppEchoMsg == NULL);

  HubMsg *pEchoMsg = NULL;
  HFILTER hFilter = (*i)->filter.hFilter;
  HFILTERINSTANCE hFilterInstance = (*i)->hFilterInstance;

  if ((*i)->pInMethod) {
    FILTER_IN_METHOD *pInMethod = (*i)->pInMethod;
    HubMsg *pNextMsg = pInMsg;

    for (HubMsg *pCurMsg = pNextMsg ; pCurMsg ; pCurMsg = pNextMsg) {
      pNextMsg = pNextMsg->Next();

      HUB_MSG *pEchoMsgPart = NULL;

      if (!pInMethod(hFilter, hFilterInstance, pCurMsg, &pEchoMsgPart)) {
        if (pEchoMsgPart)
          delete (HubMsg *)pEchoMsgPart;

        if (pEchoMsg)
          delete pEchoMsg;

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

  const FilterInstanceArray::const_iterator iNext = i + 1;

  if (iNext != iEnd) {
    if (!InMethod(pFromPort, iNext, iEnd, pInMsg, ppEchoMsg))
      return FALSE;
  }

  if ((*i)->pOutMethod) {
    FILTER_OUT_METHOD *pOutMethod = (*i)->pOutMethod;
    HubMsg *pNextMsg = *ppEchoMsg;

    for (HubMsg *pCurMsg = pNextMsg ; pCurMsg ; pCurMsg = pNextMsg) {
      pNextMsg = pNextMsg->Next();

      if (!pOutMethod(hFilter, hFilterInstance, (HMASTERPORT)pFromPort, pCurMsg)) {
        if (pEchoMsg)
          delete pEchoMsg;

        return FALSE;
      }
    }
  }

  if (pEchoMsg) {
    if (*ppEchoMsg)
      pEchoMsg->Merge(*ppEchoMsg);

    *ppEchoMsg = pEchoMsg;
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
BOOL Filters::InMethod(
    Port *pFromPort,
    HubMsg *pInMsg,
    HubMsg **ppEchoMsg) const
{
  _ASSERTE(*ppEchoMsg == NULL);

  PortFiltersMap::const_iterator iPair = portFilters.find(pFromPort);

  if (iPair == portFilters.end())
    return TRUE;

  FilterInstanceArray *pFilters = iPair->second;

  if (!pFilters)
    return TRUE;

  const FilterInstanceArray::const_iterator i = pFilters->begin();

  if (i != pFilters->end()) {
    if (!InMethod(pFromPort, i, pFilters->end(), pInMsg, ppEchoMsg))
      return FALSE;
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
BOOL Filters::OutMethod(
    Port *pFromPort,
    Port *pToPort,
    HubMsg *pOutMsg) const
{
  PortFiltersMap::const_iterator iPair = portFilters.find(pToPort);

  if (iPair == portFilters.end())
    return TRUE;

  FilterInstanceArray *pFilters = iPair->second;

  if (!pFilters)
    return TRUE;

  for (FilterInstanceArray::const_reverse_iterator i = pFilters->rbegin() ; i != pFilters->rend() ; i++) {
    if ((*i)->pOutMethod && (!(*i)->pSrcPorts ||
        (*i)->pSrcPorts->find(pFromPort) != (*i)->pSrcPorts->end()))
    {
      FILTER_OUT_METHOD *pOutMethod = (*i)->pOutMethod;
      HFILTER hFilter = (*i)->filter.hFilter;
      HFILTERINSTANCE hFilterInstance = (*i)->hFilterInstance;

      HubMsg *pNextMsg = pOutMsg;

      for (HubMsg *pCurMsg = pNextMsg ; pCurMsg ; pCurMsg = pNextMsg) {
        pNextMsg = pNextMsg->Next();

        if (!pOutMethod(hFilter, hFilterInstance, (HMASTERPORT)pFromPort, pCurMsg))
          return FALSE;
      }
    }
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
