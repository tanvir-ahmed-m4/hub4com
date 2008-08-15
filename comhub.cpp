/*
 * $Id$
 *
 * Copyright (c) 2006-2008 Vyacheslav Frolov
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
 * Revision 1.7  2008/08/15 12:44:59  vfrolov
 * Added fake read filter method to ports
 *
 * Revision 1.6  2008/08/11 07:15:33  vfrolov
 * Replaced
 *   HUB_MSG_TYPE_COM_FUNCTION
 *   HUB_MSG_TYPE_INIT_LSR_MASK
 *   HUB_MSG_TYPE_INIT_MST_MASK
 * by
 *   HUB_MSG_TYPE_SET_PIN_STATE
 *   HUB_MSG_TYPE_GET_OPTIONS
 *   HUB_MSG_TYPE_SET_OPTIONS
 *
 * Revision 1.5  2008/03/26 08:48:18  vfrolov
 * Initial revision
 *
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
#include "port.h"
#include "filters.h"
#include "hubmsg.h"

///////////////////////////////////////////////////////////////
BOOL ComHub::CreatePort(
    const PORT_ROUTINES_A *pPortRoutines,
    int n,
    HCONFIG hConfig,
    const char *pPath)
{
  if (!ROUTINE_IS_VALID(pPortRoutines, pCreate)) {
    cerr << "No create routine for port " << pPath << endl;
    return FALSE;
  }

  HPORT hPort = pPortRoutines->pCreate(hConfig, pPath);

  if (!hPort) {
    cerr << "Can't create port " << pPath << endl;
    return FALSE;
  }

  ports[n] = new Port(*this, n, pPortRoutines, hPort);

  if (!ports[n]) {
    cerr << "Can't create master port " << pPath << endl;
    return FALSE;
  }

  return TRUE;
}

BOOL ComHub::StartAll() const
{
  if (pFilters && !pFilters->Init())
    return FALSE;

  for (Ports::const_iterator i = ports.begin() ; i != ports.end() ; i++) {
    if (!(*i)->Init())
      return FALSE;
  }

  for (Ports::const_iterator i = ports.begin() ; i != ports.end() ; i++) {
    HubMsg msg;

    msg.type = HUB_MSG_TYPE_SET_OPTIONS;

    if (!OnFakeRead(*i, &msg))
      return FALSE;
  }

  for (Ports::const_iterator i = ports.begin() ; i != ports.end() ; i++) {
    HubMsg msg;
    DWORD options = 0;

    msg.type = HUB_MSG_TYPE_GET_OPTIONS;
    msg.u.pv.pVal = &options;
    msg.u.pv.val = DWORD(-1);

    if (!OnFakeRead(*i, &msg))
      return FALSE;

    if (options) {
      cout << (*i)->Name() << " WARNING: Requested option(s) 0x"
           << hex << options << dec << " not supported" << endl;
    }
  }

  for (Ports::const_iterator i = ports.begin() ; i != ports.end() ; i++) {
    if (!(*i)->Start())
      return FALSE;
  }

  return TRUE;
}

BOOL ComHub::OnFakeRead(Port *pFromPort, HubMsg *pMsg) const
{
  if (!pFromPort->FakeReadFilter(pMsg))
    return FALSE;

  OnRead(pFromPort, pMsg);

  return TRUE;
}

void ComHub::OnRead(Port *pFromPort, HubMsg *pMsg) const
{
  _ASSERTE(pFromPort != NULL);
  _ASSERTE(pMsg != NULL);

  if (pFilters) {
    HubMsg *pEchoMsg = NULL;

    if (!pFilters->InMethod(pFromPort->Num(), pMsg, &pEchoMsg)) {
      if (pEchoMsg) {
        delete pEchoMsg;
        pEchoMsg = NULL;
      }
    }

    for (HubMsg *pCurMsg = pEchoMsg ; pCurMsg ; pCurMsg = pCurMsg->Next())
      pFromPort->Write(pCurMsg);

    if (pEchoMsg)
      delete pEchoMsg;
  }

  for (PortMap::const_iterator i = routeDataMap.find(pFromPort) ; i != routeDataMap.end() ; i++) {
    if (i->first != pFromPort)
      break;

    HubMsg *pOutMsg = pMsg->Clone();

    if (pFilters && pOutMsg) {
      if (!pFilters->OutMethod(pFromPort->Num(), i->second->Num(), pOutMsg)) {
        if (pOutMsg) {
          delete pOutMsg;
          pOutMsg = NULL;
        }
      }
    }

    for (HubMsg *pCurMsg = pOutMsg ; pCurMsg ; pCurMsg = pCurMsg->Next())
      i->second->Write(pCurMsg);

    if (pOutMsg)
      delete pOutMsg;
  }
}

void ComHub::AddXoff(Port *pFromPort) const
{
  for (PortMap::const_iterator i = routeFlowControlMap.find(pFromPort) ; i != routeFlowControlMap.end() ; i++) {
    if (i->first != pFromPort)
      break;

    i->second->AddXoff();
  }
}

void ComHub::AddXon(Port *pFromPort) const
{
  for (PortMap::const_iterator i = routeFlowControlMap.find(pFromPort) ; i != routeFlowControlMap.end() ; i++) {
    if (i->first != pFromPort)
      break;

    i->second->AddXon();
  }
}

void ComHub::LostReport() const
{
  for (Ports::const_iterator i = ports.begin() ; i != ports.end() ; i++)
    (*i)->LostReport();
}

void ComHub::SetDataRoute(const PortNumMap &map)
{
  routeDataMap.clear();

  for (PortNumMap::const_iterator i = map.begin() ; i != map.end() ; i++)
    routeDataMap.insert(pair<Port*, Port*>(ports.at(i->first), ports.at(i->second)));
}

void ComHub::SetFlowControlRoute(const PortNumMap &map)
{
  routeFlowControlMap.clear();

  for (PortNumMap::const_iterator i = map.begin() ; i != map.end() ; i++)
    routeFlowControlMap.insert(pair<Port*, Port*>(ports.at(i->first), ports.at(i->second)));
}

static void RouteReport(const PortMap &map, const char *pMapName)
{
  if (!map.size()) {
    cout << "No route for " << pMapName << endl;
    return;
  }

  Port *pLastPort = NULL;

  for (PortMap::const_iterator i = map.begin() ; i != map.end() ; i++) {
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
