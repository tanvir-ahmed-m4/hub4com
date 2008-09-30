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
 * Revision 1.7  2008/09/30 08:28:32  vfrolov
 * Added ability to control OUT1 and OUT2 pins
 * Added ability to get remote baud rate and line control settings
 * Added ability to set baud rate and line control
 * Added fallback to non escape mode
 *
 * Revision 1.6  2008/08/22 16:57:12  vfrolov
 * Added
 *   HUB_MSG_TYPE_GET_ESC_OPTS
 *   HUB_MSG_TYPE_FAIL_ESC_OPTS
 *   HUB_MSG_TYPE_BREAK_STATUS
 *
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

#include "../plugins_api.h"

///////////////////////////////////////////////////////////////
class ComPort;
class ComParams;
///////////////////////////////////////////////////////////////
class ComIo
{
  public:
    ComIo(ComPort &_port) : port(_port), handle(INVALID_HANDLE_VALUE) {}
    ~ComIo() { Close(); }

    BOOL Open(const char *pPath, const ComParams &comParams);
    void Close();

    BOOL SetManualRtsControl();
    BOOL SetManualDtrControl();
    BOOL SetManualOut1Control() { return hasExtendedModemControl; }
    BOOL SetManualOut2Control() { return hasExtendedModemControl; }
    BOOL SetComEvents(DWORD *pEvents);
    void SetPinState(WORD value, WORD mask);
    DWORD SetBaudRate(DWORD baudRate);
    DWORD SetLineControl(DWORD lineControl);
    DWORD SetEscMode(DWORD escOptions, BYTE **ppBuf, DWORD *pDone);
    void PurgeWrite() { ::PurgeComm(handle, PURGE_TXABORT|PURGE_TXCLEAR); }

    HANDLE Handle() const { return handle; }
    DWORD GetBaudRate() const { return dcb.BaudRate; }
    DWORD GetLineControl() const {
      return (VAL2LC_BYTESIZE(dcb.ByteSize)
             |VAL2LC_PARITY(dcb.Parity)
             |VAL2LC_STOPBITS(dcb.StopBits));
    }

  public:
    ComPort &port;

  private:
    HANDLE handle;
    BOOL hasExtendedModemControl;
    DCB dcb;
};
///////////////////////////////////////////////////////////////
class ReadOverlapped : private OVERLAPPED
{
  public:
    ReadOverlapped(ComIo &_comIo);
    ~ReadOverlapped();
    BOOL StartRead();

  private:
    static VOID CALLBACK OnRead(
        DWORD err,
        DWORD done,
        LPOVERLAPPED pOverlapped);

    ComIo &comIo;
    BYTE *pBuf;
};
///////////////////////////////////////////////////////////////
class WriteOverlapped : private OVERLAPPED
{
  public:
    WriteOverlapped(ComIo &_comIo, BYTE *_pBuf, DWORD _len);
    ~WriteOverlapped();
    BOOL StartWrite();

  private:
    static VOID CALLBACK OnWrite(
      DWORD err,
      DWORD done,
      LPOVERLAPPED pOverlapped);

    ComIo &comIo;
    BYTE *pBuf;
    DWORD len;
};
///////////////////////////////////////////////////////////////
class WaitCommEventOverlapped : private OVERLAPPED
{
  public:
    WaitCommEventOverlapped(ComIo &_comIo);
    ~WaitCommEventOverlapped();
    BOOL StartWaitCommEvent();

  private:
    static VOID CALLBACK OnCommEvent(
      PVOID pParameter,
      BOOLEAN timerOrWaitFired);
    static VOID CALLBACK OnCommEvent(ULONG_PTR pOverlapped);

    ComIo &comIo;
    HANDLE hWait;
    DWORD eMask;
};
///////////////////////////////////////////////////////////////

#endif  // _COMIO_H
