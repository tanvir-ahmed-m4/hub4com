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

#ifndef _TIMER_H
#define _TIMER_H

///////////////////////////////////////////////////////////////
class Port;
///////////////////////////////////////////////////////////////
class Timer
{
  public:
    Timer();
    ~Timer();

    BOOL Set(Port *_pPort, const LARGE_INTEGER *pDueTime, LONG period);
    void Cancel();

  protected:
    static VOID CALLBACK TimerAPCProc(
      LPVOID pArg,
      DWORD dwTimerLowValue,
      DWORD dwTimerHighValue);

    Port *pPort;
    HANDLE hTimer;
};
///////////////////////////////////////////////////////////////

#endif  // _TIMER_H
