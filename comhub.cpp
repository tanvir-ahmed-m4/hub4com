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
 * Revision 1.14  2008/12/18 16:50:51  vfrolov
 * Extended the number of possible IN options
 *
 * Revision 1.13  2008/11/24 16:30:56  vfrolov
 * Removed pOnXoffXon
 *
 * Revision 1.12  2008/11/24 12:36:59  vfrolov
 * Changed plugin API
 *
 * Revision 1.11  2008/11/21 08:16:56  vfrolov
 * Added HUB_MSG_TYPE_LOOP_TEST
 *
 * Revision 1.10  2008/11/13 08:07:40  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.9  2008/08/20 14:30:18  vfrolov
 * Redesigned serial port options
 *
 * Revision 1.8  2008/08/20 08:46:06  vfrolov
 * Implemented ComHub::FilterName()
 *
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
#include "plugins/plugins_api.h"

#include "comhub.h"
#include "port.h"
#include "filters.h"
#include "hubmsg.h"

///////////////////////////////////////////////////////////////
void ComHub::Add()
{
  Port *pPort = new Port(*this, NumPorts());

  if (!pPort) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  ports.push_back(pPort);
}

BOOL ComHub::InitPort(
    int n,
    const PORT_ROUTINES_A *pPortRoutines,
    HCONFIG hConfig,
    const char *pPath)
{
  return ports[n]->Init(pPortRoutines, hConfig, pPath);
}

BOOL ComHub::StartAll() const
{
  for (Ports::const_iterator i = ports.begin() ; i != ports.end() ; i++) {
    HubMsg msg;

    msg.type = HUB_MSG_TYPE_LOOP_TEST;

    if (!OnFakeRead(*i, &msg))
      return FALSE;
  }

  for (Ports::const_iterator i = ports.begin() ; i != ports.end() ; i++) {
    HubMsg msg;

    msg.type = HUB_MSG_TYPE_SET_OUT_OPTS;

    if (!OnFakeRead(*i, &msg))
      return FALSE;
  }

  for (Ports::const_iterator i = ports.begin() ; i != ports.end() ; i++) {
    DWORD fail_options[GO_O2I(GO_I2O(-1)) + 1];

    for (int iGo = 0 ; iGo < sizeof(fail_options)/sizeof(fail_options[0]) ; iGo++)
      fail_options[iGo] = 0;

    DWORD repeats = 0;

    {
      HubMsg msg;

      msg.type = HUB_MSG_TYPE_COUNT_REPEATS;
      msg.u.pv.pVal = &repeats;
      msg.u.pv.val = HUB_MSG_TYPE_GET_IN_OPTS;

      if (!OnFakeRead(*i, &msg))
        return FALSE;
    }

    do {
      for (int iGo = 0 ; iGo < sizeof(fail_options)/sizeof(fail_options[0]) ; iGo++) {
        HubMsg msg;

        msg.type = HUB_MSG_TYPE_GET_IN_OPTS;
        msg.u.pv.pVal = &fail_options[iGo];
        msg.u.pv.val = ~GO_I2O(-1) | GO_I2O(iGo);

        if (!OnFakeRead(*i, &msg))
          return FALSE;
      }
    } while (repeats--);

    for (int iGo = 0 ; iGo < sizeof(fail_options)/sizeof(fail_options[0]) ; iGo++) {
      _ASSERTE((fail_options[iGo] & GO_I2O(-1)) == 0);

      if (fail_options[iGo]) {
        cerr << (*i)->Name() << " WARNING: Requested option(s) GO" << iGo << "_0x"
             << hex << fail_options << dec << " not supported" << endl;
      }

      HubMsg msg;

      msg.type = HUB_MSG_TYPE_FAIL_IN_OPTS;
      msg.u.val = (fail_options[iGo] & ~GO_I2O(-1)) | GO_I2O(iGo);

      if (!OnFakeRead(*i, &msg))
        return FALSE;
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

    if (!pFilters->InMethod(pFromPort, pMsg, &pEchoMsg)) {
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

  const PortMap &routeMap = (pMsg->type & HUB_MSG_ROUTE_FLOW_CONTROL) ? routeFlowControlMap : routeDataMap;

  for (PortMap::const_iterator i = routeMap.find(pFromPort) ; i != routeMap.end() ; i++) {
    if (i->first != pFromPort)
      break;

    HubMsg *pOutMsg = pMsg->Clone();

    if (pFilters && pOutMsg) {
      if (!pFilters->OutMethod(pFromPort, i->second, pOutMsg)) {
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

void ComHub::LostReport() const
{
  for (Ports::const_iterator i = ports.begin() ; i != ports.end() ; i++)
    (*i)->LostReport();
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
