/*
 * $Id$
 *
 * Copyright (c) 2009 Vyacheslav Frolov
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
 * Revision 1.1  2009/01/23 16:46:32  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "plugins/plugins_api.h"

#include "timer.h"
#include "comhub.h"
#include "port.h"
#include "hubmsg.h"
///////////////////////////////////////////////////////////////
Timer::Timer()
{
  hTimer = ::CreateWaitableTimer(NULL, FALSE, NULL);

  if (!hTimer) {
    DWORD err = GetLastError();

    cerr << "WARNING: CreateWaitableTimer() - error=" << err << endl;
  }
}
///////////////////////////////////////////////////////////////
Timer::~Timer()
{
  if (hTimer) {
    Cancel();
    CloseHandle(hTimer);
  }
}
///////////////////////////////////////////////////////////////
VOID CALLBACK Timer::TimerAPCProc(
  LPVOID pArg,
  DWORD /*dwTimerLowValue*/,
  DWORD /*dwTimerHighValue*/)
{
  HubMsg msg;

  msg.type = HUB_MSG_TYPE_TICK;
  msg.u.hVal = pArg;

  ((Timer *)pArg)->pPort->hub.OnFakeRead(((Timer *)pArg)->pPort, &msg);
}
///////////////////////////////////////////////////////////////
BOOL Timer::Set(Port *_pPort, const LARGE_INTEGER *pDueTime, LONG period)
{
  pPort = _pPort;

  _ASSERTE(pPort != NULL);

  if (!::SetWaitableTimer(hTimer, pDueTime, period, TimerAPCProc, this, FALSE)) {
    DWORD err = GetLastError();

    cerr << "WARNING: SetWaitableTimer() - error=" << err << endl;

    return FALSE;
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
void Timer::Cancel()
{
  if (!::CancelWaitableTimer(hTimer)) {
    DWORD err = GetLastError();

    cerr << "WARNING: CancelWaitableTimer() - error=" << err << endl;
  }
}
///////////////////////////////////////////////////////////////
