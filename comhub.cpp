/*
 * $Id$
 *
 * Copyright (c) 2006-2007 Vyacheslav Frolov
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
 * Revision 1.1  2007/01/23 09:13:10  vfrolov
 * Initial revision
 *
 *
 */

#include "precomp.h"
#include "comhub.h"
#include "comport.h"

///////////////////////////////////////////////////////////////
typedef pair <ComPort*, ComPort*> ComPortPair;
///////////////////////////////////////////////////////////////
static ComPortMap::iterator FindPair(ComPortMap &map, ComPortPair &pair)
{
  ComPortMap::iterator i;

  for (i = map.find(pair.first) ; i != map.end() ; i++) {
    if (i->first != pair.first)
      return map.end();

    if (i->second == pair.second)
      break;
  }

  return i;
}
///////////////////////////////////////////////////////////////
ComHub::ComHub(int num)
{
  for (int i = 0 ; i < num ; i++) {
    ComPort *pPort = new ComPort(*this);

    if (!pPort) {
      cerr << "Can't create ComPort " << i << endl;
      break;
    }

    ports.push_back(pPort);
  }
}

BOOL ComHub::PlugIn(int n, const char *pPath, const ComParams &comParams)
{
  ComPort *pPort = ports.at(n);

  if (!pPort->Open(pPath, comParams))
    return FALSE;

  return TRUE;
}

BOOL ComHub::StartAll()
{
  BOOL res = TRUE;

  for (ComPorts::const_iterator i = ports.begin() ; i != ports.end() ; i++) {
    if (!(*i)->Start())
      res = FALSE;
  }

  return res;
}

void ComHub::Write(ComPort *pFromPort, LPCVOID pData, DWORD len)
{
  ComPortMap::const_iterator i;

  for (i = routeDataMap.find(pFromPort) ; i != routeDataMap.end() ; i++) {
    if (i->first != pFromPort)
      break;

    i->second->Write(pData, len);
  }
}

void ComHub::LostReport()
{
  for (ComPorts::const_iterator i = ports.begin() ; i != ports.end() ; i++)
    (*i)->LostReport();
}

void ComHub::RouteData(int iFrom, int iTo, BOOL noRoute)
{
  if (iFrom < 0) {
    for (int iF = 0 ; iF < (int)ports.size() ; iF++) {
      if(iTo < 0) {
        for (int iT = 0 ; iT < (int)ports.size() ; iT++)
          RouteData(iF, iT, noRoute);
      } else {
        RouteData(iF, iTo, noRoute);
      }
    }
  } else {
    if(iTo < 0) {
      for (int iT = 0 ; iT < (int)ports.size() ; iT++)
        RouteData(iFrom, iT, noRoute);
    }
    else
    if (iFrom != iTo || noRoute) {
      ComPortPair pair(ports.at(iFrom), ports.at(iTo));

      for (;;) {
        ComPortMap::iterator i = FindPair(routeDataMap, pair);

        if (i == routeDataMap.end()) {
          if (!noRoute)
            routeDataMap.insert(pair);
          break;
        } else if (noRoute) {
          routeDataMap.erase(i);
        } else {
          break;
        }
      }
    }
  }
}

void ComHub::RouteDataReport()
{
  if (!routeDataMap.size()) {
    cout << "No route for data" << endl;
    return;
  }

  ComPort *pLastPort = NULL;

  for (ComPortMap::const_iterator i = routeDataMap.begin() ; i != routeDataMap.end() ; i++) {
    if (pLastPort != i->first) {
      if (pLastPort)
        cout << endl;

      cout << "Route data " << i->first->Name() << " -->";

      pLastPort = i->first;
    }

    cout << " " << i->second->Name();
  }

  if (pLastPort)
    cout << endl;
}
///////////////////////////////////////////////////////////////
