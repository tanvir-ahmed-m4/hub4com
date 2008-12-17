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
 * Revision 1.11  2008/12/17 11:52:35  vfrolov
 * Replaced ComIo::dcb by serialBaudRate, serialLineControl,
 * serialHandFlow and serialChars
 * Replaced ComPort::filterX by ComIo::FilterX()
 * Replaced SetManual*() by PinStateControlMask()
 *
 * Revision 1.10  2008/12/01 17:06:29  vfrolov
 * Improved write buffering
 *
 * Revision 1.9  2008/11/13 07:35:10  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.8  2008/10/22 08:27:26  vfrolov
 * Added ability to set bytesize, parity and stopbits separately
 *
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

///////////////////////////////////////////////////////////////
class ComPort;
class ComParams;
///////////////////////////////////////////////////////////////
struct SERIAL_BAUD_RATE {
  ULONG BaudRate;
};
///////////////////////////////////////////////////////////////
struct SERIAL_LINE_CONTROL {
  UCHAR StopBits;
  UCHAR Parity;
  UCHAR WordLength;
};
///////////////////////////////////////////////////////////////
struct SERIAL_HANDFLOW {
  ULONG ControlHandShake;

#define SERIAL_DTR_MASK           ((ULONG)0x03)
#define SERIAL_DTR_CONTROL        ((ULONG)0x01)
#define SERIAL_DTR_HANDSHAKE      ((ULONG)0x02)

#define SERIAL_CTS_HANDSHAKE      ((ULONG)0x08)
#define SERIAL_DSR_HANDSHAKE      ((ULONG)0x10)
#define SERIAL_DCD_HANDSHAKE      ((ULONG)0x20)
#define SERIAL_DSR_SENSITIVITY    ((ULONG)0x40)
#define SERIAL_ERROR_ABORT        ((ULONG)0x80000000)

  ULONG FlowReplace;

#define SERIAL_AUTO_TRANSMIT      ((ULONG)0x01)
#define SERIAL_AUTO_RECEIVE       ((ULONG)0x02)
#define SERIAL_ERROR_CHAR         ((ULONG)0x04)
#define SERIAL_NULL_STRIPPING     ((ULONG)0x08)
#define SERIAL_BREAK_CHAR         ((ULONG)0x10)

#define SERIAL_RTS_MASK           ((ULONG)0xc0)
#define SERIAL_RTS_CONTROL        ((ULONG)0x40)
#define SERIAL_RTS_HANDSHAKE      ((ULONG)0x80)
#define SERIAL_TRANSMIT_TOGGLE    ((ULONG)0xc0)

#define SERIAL_XOFF_CONTINUE      ((ULONG)0x80000000)

  LONG XonLimit;
  LONG XoffLimit;
};

inline BOOL IsManualDtr(const SERIAL_HANDFLOW &serialHandFlow)
{
  ULONG val = serialHandFlow.ControlHandShake & SERIAL_DTR_MASK;

  return (val == 0 || val == SERIAL_DTR_CONTROL);
}

inline void SetHandFlowDtr(SERIAL_HANDFLOW &serialHandFlow, ULONG val)
{
  _ASSERTE((val & ~SERIAL_DTR_MASK) == 0);

  serialHandFlow.ControlHandShake &= ~SERIAL_DTR_MASK;
  serialHandFlow.ControlHandShake |= val;
}

inline BOOL IsManualRts(const SERIAL_HANDFLOW &serialHandFlow)
{
  ULONG val = serialHandFlow.FlowReplace & SERIAL_RTS_MASK;

  return (val == 0 || val == SERIAL_RTS_CONTROL);
}

inline void SetHandFlowRts(SERIAL_HANDFLOW &serialHandFlow, ULONG val)
{
  _ASSERTE((val & ~SERIAL_RTS_MASK) == 0);

  serialHandFlow.FlowReplace &= ~SERIAL_RTS_MASK;
  serialHandFlow.FlowReplace |= val;
}
///////////////////////////////////////////////////////////////
struct SERIAL_CHARS {
  UCHAR EofChar;
  UCHAR ErrorChar;
  UCHAR BreakChar;
  UCHAR EventChar;
  UCHAR XonChar;
  UCHAR XoffChar;
};
///////////////////////////////////////////////////////////////
class ComIo
{
  public:
    ComIo(ComPort &_port) : port(_port), handle(INVALID_HANDLE_VALUE) {}
    ~ComIo() { Close(); }

    BOOL Open(const char *pPath, const ComParams &comParams);
    void Close();

    WORD PinStateControlMask() const {
      return PIN_STATE_RTS|
             PIN_STATE_DTR|
             (hasExtendedModemControl ? PIN_STATE_OUT1|PIN_STATE_OUT2 : 0);
    }

    BOOL FilterX(BYTE &xOn, BYTE &xOff) const {
      if ((serialHandFlow.FlowReplace & SERIAL_AUTO_RECEIVE) == 0)
        return FALSE;

      xOn = serialChars.XonChar;
      xOff = serialChars.XoffChar;

      return TRUE;
    }

    BOOL SetComEvents(DWORD *pEvents);
    void SetPinState(WORD value, WORD mask);
    DWORD SetBaudRate(DWORD baudRate);
    DWORD SetLineControl(DWORD lineControl);
    DWORD SetEscMode(DWORD escOptions, BYTE **ppBuf, DWORD *pDone);
    void PurgeWrite() { ::PurgeComm(handle, PURGE_TXABORT|PURGE_TXCLEAR); }

    HANDLE Handle() const { return handle; }
    DWORD BaudRate() const { return serialBaudRate.BaudRate; }
    DWORD LineControl() const {
      return (VAL2LC_BYTESIZE(serialLineControl.WordLength)
             |LC_MASK_BYTESIZE
             |VAL2LC_PARITY(serialLineControl.Parity)
             |LC_MASK_PARITY
             |VAL2LC_STOPBITS(serialLineControl.StopBits)
             |LC_MASK_STOPBITS);
    }

  public:
    ComPort &port;

  private:
    HANDLE handle;
    BOOL hasExtendedModemControl;
    SERIAL_BAUD_RATE serialBaudRate;
    SERIAL_LINE_CONTROL serialLineControl;
    SERIAL_HANDFLOW serialHandFlow;
    SERIAL_CHARS serialChars;
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
#ifdef _DEBUG
    WriteOverlapped() : pBuf(NULL) {}
    ~WriteOverlapped() {
      _ASSERTE(pBuf == NULL);
    }
#endif

    BOOL StartWrite(ComIo *_pComIo, BYTE *_pBuf, DWORD _len);

  private:
    static VOID CALLBACK OnWrite(
      DWORD err,
      DWORD done,
      LPOVERLAPPED pOverlapped);
    void BufFree();

    ComIo *pComIo;
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
