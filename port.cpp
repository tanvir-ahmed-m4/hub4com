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
 * Revision 1.4  2008/11/24 12:36:59  vfrolov
 * Changed plugin API
 *
 * Revision 1.3  2008/11/13 08:07:40  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.2  2008/08/15 12:44:59  vfrolov
 * Added fake read filter method to ports
 *
 * Revision 1.1  2008/03/26 08:36:47  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "plugins/plugins_api.h"

#include "port.h"
#include "comhub.h"

///////////////////////////////////////////////////////////////
Port::Port(ComHub &_hub, int _num)
  : hub(_hub),
    num(_num),
    hPort(NULL)
{
  stringstream buf;

  buf << "P(" << num << ")";

  name = buf.str();

#ifdef _DEBUG
  signature = PORT_SIGNATURE;
#endif
}

BOOL Port::Init(
    const PORT_ROUTINES_A *pPortRoutines,
    HCONFIG hConfig,
    const char *pPath)
{
  if (!ROUTINE_IS_VALID(pPortRoutines, pCreate)) {
    cerr << "No create routine for port " << pPath << endl;
    return FALSE;
  }

  hPort = pPortRoutines->pCreate(hConfig, pPath);

  if (!hPort) {
    cerr << "Can't create port " << pPath << endl;
    return FALSE;
  }

  pStart = ROUTINE_GET(pPortRoutines, pStart);
  pFakeReadFilter = ROUTINE_GET(pPortRoutines, pFakeReadFilter);
  pWrite = ROUTINE_GET(pPortRoutines, pWrite);
  pLostReport = ROUTINE_GET(pPortRoutines, pLostReport);

  const char *pName = ROUTINE_IS_VALID(pPortRoutines, pGetPortName)
                     ? pPortRoutines->pGetPortName(hPort)
                     : NULL;

  stringstream buf;

  if (pName && *pName) {
    buf << pName;
  } else {
    buf << "P";
  }

  buf << "(" << num << ")";

  name = buf.str();

  if (ROUTINE_IS_VALID(pPortRoutines, pSetPortName))
    pPortRoutines->pSetPortName(hPort, name.c_str());

  if (ROUTINE_IS_VALID(pPortRoutines, pInit)) {
    if (!ROUTINE_GET(pPortRoutines, pInit)(hPort, HMASTERPORT(this)))
      return FALSE;
  }

  return TRUE;
}

BOOL Port::Start()
{
  if (!pStart)
    return TRUE;

  if (!pStart(hPort)) {
    cerr << "Can't start " << name << endl;
    return FALSE;
  }

  cout << "Started " << name << endl;

  return TRUE;
}

BOOL Port::FakeReadFilter(HubMsg *pMsg)
{
  _ASSERTE(pMsg != NULL);

  if (!pFakeReadFilter)
    return TRUE;

  return pFakeReadFilter(hPort, (HUB_MSG *)pMsg);
}

BOOL Port::Write(HubMsg *pMsg)
{
  _ASSERTE(pMsg != NULL);

  if (!pWrite)
    return TRUE;

  return pWrite(hPort, (HUB_MSG *)pMsg);
}

void Port::LostReport()
{
  if (pLostReport)
    pLostReport(hPort);
}
///////////////////////////////////////////////////////////////
