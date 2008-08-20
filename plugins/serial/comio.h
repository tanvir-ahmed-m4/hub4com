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
 * Revision 1.5  2008/08/20 14:30:19  vfrolov
 * Redesigned serial port options
 *
 * Revision 1.4  2008/08/14 15:19:07  vfrolov
 * Execute OnCommEvent() in main thread context
 *
 * Revision 1.3  2008/08/11 07:15:34  vfrolov
 * Replaced
 *   HUB_MSG_TYPE_COM_FUNCTION
 *   HUB_MSG_TYPE_INIT_LSR_MASK
 *   HUB_MSG_TYPE_INIT_MST_MASK
 * by
 *   HUB_MSG_TYPE_SET_PIN_STATE
 *   HUB_MSG_TYPE_GET_OPTIONS
 *   HUB_MSG_TYPE_SET_OPTIONS
 *
 * Revision 1.2  2008/04/07 12:28:03  vfrolov
 * Replaced --rt-events option by SET_RT_EVENTS message
 *
 * Revision 1.1  2008/03/26 08:43:50  vfrolov
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

#ifndef _COMIO_H
#define _COMIO_H

///////////////////////////////////////////////////////////////
class ComPort;
class ComParams;
///////////////////////////////////////////////////////////////
extern HANDLE OpenComPort(const char *pPath, const ComParams &comParams);
extern BOOL SetManualRtsControl(HANDLE handle);
extern BOOL SetManualDtrControl(HANDLE handle);
extern BOOL SetComEvents(HANDLE handle, DWORD *events);
extern BOOL CommFunction(HANDLE handle, DWORD func);
extern DWORD SetEscMode(HANDLE handle, DWORD options, BYTE escapeChar, BYTE **ppBuf, DWORD *pDone);
///////////////////////////////////////////////////////////////
class ReadOverlapped : private OVERLAPPED
{
  public:
    ReadOverlapped(ComPort &_port);
    ~ReadOverlapped();
    BOOL StartRead();

  private:
    static VOID CALLBACK OnRead(
        DWORD err,
        DWORD done,
        LPOVERLAPPED pOverlapped);

    ComPort &port;
    BYTE *pBuf;
};
///////////////////////////////////////////////////////////////
class WriteOverlapped : private OVERLAPPED
{
  public:
    WriteOverlapped(ComPort &_port, BYTE *_pBuf, DWORD _len);
    ~WriteOverlapped();
    BOOL StartWrite();

  private:
    static VOID CALLBACK OnWrite(
      DWORD err,
      DWORD done,
      LPOVERLAPPED pOverlapped);

    ComPort &port;
    BYTE *pBuf;
    DWORD len;
};
///////////////////////////////////////////////////////////////
class WaitCommEventOverlapped : private OVERLAPPED
{
  public:
    WaitCommEventOverlapped(ComPort &_port);
    ~WaitCommEventOverlapped();
    BOOL StartWaitCommEvent();

  private:
    static VOID CALLBACK OnCommEvent(
      PVOID pParameter,
      BOOLEAN timerOrWaitFired);
    static VOID CALLBACK OnCommEvent(ULONG_PTR pOverlapped);

    ComPort &port;
    HANDLE hWait;
    DWORD eMask;
};
///////////////////////////////////////////////////////////////

#endif  // _COMIO_H
