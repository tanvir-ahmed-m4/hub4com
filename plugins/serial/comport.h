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
 * Revision 1.4  2008/08/11 07:15:34  vfrolov
 * Replaced
 *   HUB_MSG_TYPE_COM_FUNCTION
 *   HUB_MSG_TYPE_INIT_LSR_MASK
 *   HUB_MSG_TYPE_INIT_MST_MASK
 * by
 *   HUB_MSG_TYPE_SET_PIN_STATE
 *   HUB_MSG_TYPE_GET_OPTIONS
 *   HUB_MSG_TYPE_SET_OPTIONS
 *
 * Revision 1.3  2008/04/11 14:48:42  vfrolov
 * Replaced SET_RT_EVENTS by INIT_LSR_MASK and INIT_MST_MASK
 * Replaced COM_ERRORS by LINE_STATUS
 *
 * Revision 1.2  2008/04/07 12:28:03  vfrolov
 * Replaced --rt-events option by SET_RT_EVENTS message
 *
 * Revision 1.1  2008/03/26 08:44:34  vfrolov
 * Redesigned for using plugins
 *
 * Revision 1.3  2007/02/06 11:53:33  vfrolov
 * Added options --odsr, --ox, --ix and --idsr
 * Added communications error reporting
 *
 * Revision 1.2  2007/02/05 09:33:20  vfrolov
 * Implemented internal flow control
 *
 * Revision 1.1  2007/01/23 09:13:10  vfrolov
 * Initial revision
 *
 */

#ifndef _COMPORT_H
#define _COMPORT_H

#include "../plugins_api.h"

///////////////////////////////////////////////////////////////
class ComParams;
class WriteOverlapped;
class ReadOverlapped;
class WaitCommEventOverlapped;
///////////////////////////////////////////////////////////////
class ComPort
{
  public:
    ComPort(
      const ComParams &comParams,
      const char *pPath);

    BOOL Init(HMASTERPORT _hMasterPort, HHUB _hHub);
    BOOL Start();
    BOOL Write(HUB_MSG *pMsg);
    void OnWrite(WriteOverlapped *pOverlapped, DWORD len, DWORD done);
    void OnRead(ReadOverlapped *pOverlapped, BYTE *pBuf, DWORD done);
    void OnCommEvent(WaitCommEventOverlapped *pOverlapped, DWORD eMask);
    void AddXoff(int count);
    void LostReport();

    const string &Name() const { return name; }
    void Name(const char *pName) { name = pName; }
    HANDLE Handle() const { return handle; }

  private:
    BOOL StartRead();
    BOOL StartWaitCommEvent();
    void CheckComEvents(DWORD eMask);

    string name;
    HANDLE handle;
    HMASTERPORT hMasterPort;
    HHUB hHub;
    int countReadOverlapped;
    int countWaitCommEventOverlapped;
    int countXoff;
    BOOL filterX;
    DWORD events;
    BYTE maskOutPins;
    BYTE optsLsr;
    BYTE optsMst;

    DWORD writeQueueLimit;
    DWORD writeQueued;
    DWORD writeLost;
    DWORD writeLostTotal;
    DWORD errors;
};
///////////////////////////////////////////////////////////////

#endif  // _COMPORT_H
