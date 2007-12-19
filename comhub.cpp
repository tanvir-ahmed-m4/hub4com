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
 * Revision 1.4  2007/12/19 13:46:36  vfrolov
 * Added ability to send data received from port to the same port
 *
 * Revision 1.3  2007/02/05 09:33:20  vfrolov
 * Implemented internal flow control
 *
 * Revision 1.2  2007/02/01 12:14:58  vfrolov
 * Redesigned COM port params
 *
 * Revision 1.1  2007/01/23 09:13:10  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "comhub.h"
#include "comport.h"

///////////////////////////////////////////////////////////////
typedef pair <ComPort*, ComPort*> ComPortPair;
///////////////////////////////////////////////////////////////
static ComPortMap::iterator FindPair(ComPortMap &map, const ComPortPair &pair)
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
BOOL ComHub::Add(const char * /*pPath*/)
{
  ComPort *pPort = new ComPort(*this);

  if (!pPort) {
    cerr << "Can't create ComPort " << ports.size() << endl;
    return FALSE;
  }

  ports.push_back(pPort);

  return TRUE;
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

void ComHub::AddXoff(ComPort *pFromPort, int count)
{
  ComPortMap::const_iterator i;

  for (i = routeFlowControlMap.find(pFromPort) ; i != routeFlowControlMap.end() ; i++) {
    if (i->first != pFromPort)
      break;

    i->second->AddXoff(count);
  }
}

void ComHub::LostReport() const
{
  for (ComPorts::const_iterator i = ports.begin() ; i != ports.end() ; i++)
    (*i)->LostReport();
}

void ComHub::RouteFlowControl(BOOL fromAnyDataReceiver)
{
  for (ComPortMap::const_iterator i = routeDataMap.begin() ; i != routeDataMap.end() ; i++) {
    ComPortPair pair(i->second, i->first);

    if (FindPair(routeFlowControlMap, pair) == routeFlowControlMap.end()) {
      if (fromAnyDataReceiver || FindPair(routeDataMap, pair) != routeDataMap.end())
        routeFlowControlMap.insert(pair);
    }
  }
}

void ComHub::Route(ComPortMap &map, int iFrom, int iTo, BOOL noRoute, BOOL noEcho) const
{
  if (iFrom < 0) {
    for (int iF = 0 ; iF < (int)ports.size() ; iF++) {
      if(iTo < 0) {
        for (int iT = 0 ; iT < (int)ports.size() ; iT++)
          Route(map, iF, iT, noRoute, noEcho);
      } else {
        Route(map, iF, iTo, noRoute, noEcho);
      }
    }
  } else {
    if(iTo < 0) {
      for (int iT = 0 ; iT < (int)ports.size() ; iT++)
        Route(map, iFrom, iT, noRoute, noEcho);
    }
    else
    if (iFrom != iTo || !noEcho || noRoute) {
      ComPortPair pair(ports.at(iFrom), ports.at(iTo));

      for (;;) {
        ComPortMap::iterator i = FindPair(map, pair);

        if (i == map.end()) {
          if (!noRoute)
            map.insert(pair);
          break;
        } else if (noRoute) {
          map.erase(i);
        } else {
          break;
        }
      }
    }
  }
}

static void RouteReport(const ComPortMap &map, const char *pMapName)
{
  if (!map.size()) {
    cout << "No route for " << pMapName << endl;
    return;
  }

  ComPort *pLastPort = NULL;

  for (ComPortMap::const_iterator i = map.begin() ; i != map.end() ; i++) {
    if (pLastPort != i->first) {
      if (pLastPort)
        cout << endl;

      cout << "Route " << pMapName << " " << i->first->Name() << " -->";

      pLastPort = i->first;
    }

    cout << " " << i->second->Name();
  }

  if (pLastPort)
    cout << endl;
}

void ComHub::RouteReport() const
{
  ::RouteReport(routeDataMap, "data");
  ::RouteReport(routeFlowControlMap, "flow control");
}
///////////////////////////////////////////////////////////////
