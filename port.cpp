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
 * Revision 1.1  2008/03/26 08:36:47  vfrolov
 * Initial revision
 *
 *
 */

#include "precomp.h"
#include "port.h"
#include "comhub.h"

///////////////////////////////////////////////////////////////
Port::Port(ComHub &_hub, int _num, const PORT_ROUTINES_A *pPortRoutines, HPORT _hPort)
  : hub(_hub),
    num(_num),
    hPort(_hPort)    
{
#ifdef _DEBUG
  signature = PORT_SIGNATURE;
#endif

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

  pInit = ROUTINE_GET(pPortRoutines, pInit);
  pStart = ROUTINE_GET(pPortRoutines, pStart);
  pWrite = ROUTINE_GET(pPortRoutines, pWrite);

  pAddXoff = ROUTINE_GET(pPortRoutines, pAddXoff);
  pAddXon = ROUTINE_GET(pPortRoutines, pAddXon);
  pLostReport = ROUTINE_GET(pPortRoutines, pLostReport);
}

BOOL Port::Init()
{
  if (!pInit)
    return TRUE;

  return pInit(hPort, HMASTERPORT(this), HHUB(&hub));
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

BOOL Port::Write(HubMsg *pMsg)
{
  _ASSERTE(pMsg != NULL);

  if (!pWrite)
    return TRUE;

  return pWrite(hPort, (HUB_MSG *)pMsg);
}

void Port::AddXoff()
{
  if (pAddXoff)
    pAddXoff(hPort);
}

void Port::AddXon()
{
  if (pAddXon)
    pAddXon(hPort);
}

void Port::LostReport()
{
  if (pLostReport)
    pLostReport(hPort);
}
///////////////////////////////////////////////////////////////
