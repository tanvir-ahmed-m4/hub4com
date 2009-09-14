/*
 * $Id$
 *
 * Copyright (c) 2006-2009 Vyacheslav Frolov
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
 * Revision 1.16  2009/09/14 08:52:27  vfrolov
 * Suppressed "IOCTL_SERIAL_GET_MODEM_CONTROL ERROR Unknown error (87)"
 *
 * Revision 1.15  2008/12/18 16:50:52  vfrolov
 * Extended the number of possible IN options
 *
 * Revision 1.14  2008/12/17 11:52:35  vfrolov
 * Replaced ComIo::dcb by serialBaudRate, serialLineControl,
 * serialHandFlow and serialChars
 * Replaced ComPort::filterX by ComIo::FilterX()
 * Replaced SetManual*() by PinStateControlMask()
 *
 * Revision 1.13  2008/12/01 17:06:29  vfrolov
 * Improved write buffering
 *
 * Revision 1.12  2008/11/13 07:35:10  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.11  2008/10/22 08:27:26  vfrolov
 * Added ability to set bytesize, parity and stopbits separately
 *
 * Revision 1.10  2008/10/07 09:26:00  vfrolov
 * Fixed reseting MCR by setting BR or LC
 *
 * Revision 1.9  2008/09/30 08:28:32  vfrolov
 * Added ability to control OUT1 and OUT2 pins
 * Added ability to get remote baud rate and line control settings
 * Added ability to set baud rate and line control
 * Added fallback to non escape mode
 *
 * Revision 1.8  2008/08/29 13:02:37  vfrolov
 * Added ESC_OPTS_MAP_EO2GO() and ESC_OPTS_MAP_GO2EO()
 *
 * Revision 1.7  2008/08/26 14:23:31  vfrolov
 * Added ability to SetEscMode() return LSR and MST for non com0com ports
 *
 * Revision 1.6  2008/08/22 16:57:11  vfrolov
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
 * Revision 1.3  2008/08/11 07:15:33  vfrolov
 * Replaced
 *   HUB_MSG_TYPE_COM_FUNCTION
 *   HUB_MSG_TYPE_INIT_LSR_MASK
 *   HUB_MSG_TYPE_INIT_MST_MASK
 * by
 *   HUB_MSG_TYPE_SET_PIN_STATE
 *   HUB_MSG_TYPE_GET_OPTIONS
 *   HUB_MSG_TYPE_SET_OPTIONS
 *
 * Revision 1.2  2008/04/07 12:28:02  vfrolov
 * Replaced --rt-events option by SET_RT_EVENTS message
 *
 * Revision 1.1  2008/03/26 08:43:50  vfrolov
 * Redesigned for using plugins
 *
 * Revision 1.5  2007/05/14 12:06:37  vfrolov
 * Added read interval timeout option
 *
 * Revision 1.4  2007/02/06 11:53:33  vfrolov
 * Added options --odsr, --ox, --ix and --idsr
 * Added communications error reporting
 *
 * Revision 1.3  2007/02/05 09:33:20  vfrolov
 * Implemented internal flow control
 *
 * Revision 1.2  2007/02/01 12:14:59  vfrolov
 * Redesigned COM port params
 *
 * Revision 1.1  2007/01/23 09:13:10  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "../plugins_api.h"
#include "../cncext.h"
///////////////////////////////////////////////////////////////
namespace PortSerial {
///////////////////////////////////////////////////////////////
#include "comio.h"
#include "comport.h"
#include "comparams.h"
#include "import.h"
///////////////////////////////////////////////////////////////
static void TraceError(DWORD err, const char *pFmt, ...)
{
  va_list va;
  va_start(va, pFmt);
  vfprintf(stderr, pFmt, va);
  va_end(va);

  fprintf(stderr, " ERROR %s (%lu)\n", strerror(err), (unsigned long)err);
}
///////////////////////////////////////////////////////////////
static BOOL myGetCommState(HANDLE handle, DCB *pDcb)
{
  DCB dcb;

  dcb.DCBlength = sizeof(dcb);

  if (!::GetCommState(handle, &dcb)) {
    TraceError(GetLastError(), "GetCommState()");
    return FALSE;
  }

  *pDcb = dcb;

  return TRUE;
}

static BOOL mySetCommState(HANDLE handle, DCB *pDcb)
{
  if (!::SetCommState(handle, pDcb)) {
    TraceError(GetLastError(), "SetCommState()");
    myGetCommState(handle, pDcb);
    return FALSE;
  }

  myGetCommState(handle, pDcb);

  return TRUE;
}
///////////////////////////////////////////////////////////////
static BOOL myGetCommTimeouts(HANDLE handle, COMMTIMEOUTS *timeouts)
{
  if (!::GetCommTimeouts(handle, timeouts)) {
    TraceError(GetLastError(), "GetCommTimeouts()");
    return FALSE;
  }
  return TRUE;
}

static BOOL mySetCommTimeouts(HANDLE handle, COMMTIMEOUTS *timeouts)
{
  if (!::SetCommTimeouts(handle, timeouts)) {
    TraceError(GetLastError(), "SetCommTimeouts()");
    myGetCommTimeouts(handle, timeouts);
    return FALSE;
  }
  return myGetCommTimeouts(handle, timeouts);
}
///////////////////////////////////////////////////////////////
static BOOL myGetCommMask(HANDLE handle, DWORD *events)
{
  if (!::GetCommMask(handle, events)) {
    TraceError(GetLastError(), "GetCommMask()");
    return FALSE;
  }
  return TRUE;
}
///////////////////////////////////////////////////////////////
static BOOL CommFunction(HANDLE handle, DWORD func)
{
  if (!::EscapeCommFunction(handle, func)) {
    TraceError(GetLastError(), "EscapeCommFunction(%lu)", (long)func);
    return FALSE;
  }
  return TRUE;
}
///////////////////////////////////////////////////////////////
#define IOCTL_SERIAL_GET_MODEM_CONTROL  CTL_CODE(FILE_DEVICE_SERIAL_PORT,37,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_SET_MODEM_CONTROL  CTL_CODE(FILE_DEVICE_SERIAL_PORT,38,METHOD_BUFFERED,FILE_ANY_ACCESS)

static BOOL HasExtendedModemControl(HANDLE handle)
{
  BYTE inBufIoctl[C0CE_SIGNATURE_SIZE];
  memcpy(inBufIoctl, C0CE_SIGNATURE, C0CE_SIGNATURE_SIZE);

  BYTE outBuf[sizeof(ULONG) + C0CE_SIGNATURE_SIZE];

  DWORD returned;

  if (!DeviceIoControl(handle,
                       IOCTL_SERIAL_GET_MODEM_CONTROL,
                       inBufIoctl, sizeof(inBufIoctl),
                       outBuf, sizeof(outBuf), &returned,
                       NULL))
  {
    DWORD err = GetLastError();

    if (err != ERROR_INVALID_PARAMETER) // some drivers expect sizeof(outBuf)==sizeof(ULONG)
      TraceError(err, "IOCTL_SERIAL_GET_MODEM_CONTROL");

    return FALSE;
  }

  if (returned < (sizeof(ULONG) + C0CE_SIGNATURE_SIZE) ||
      memcmp(outBuf + sizeof(ULONG), C0CE_SIGNATURE, C0CE_SIGNATURE_SIZE) != 0)
  {
    return FALSE;  // standard functionality
  }

  return TRUE;
}

static BOOL SetModemControl(HANDLE handle, BYTE control, BYTE mask)
{
  BYTE inBufIoctl[sizeof(ULONG) + sizeof(ULONG) + C0CE_SIGNATURE_SIZE];
  ((ULONG *)inBufIoctl)[0] = control;
  ((ULONG *)inBufIoctl)[1] = mask;
  memcpy(inBufIoctl + sizeof(ULONG) + sizeof(ULONG), C0CE_SIGNATURE, C0CE_SIGNATURE_SIZE);

  DWORD returned;

  if (!DeviceIoControl(handle,
                       IOCTL_SERIAL_SET_MODEM_CONTROL,
                       inBufIoctl, sizeof(inBufIoctl),
                       NULL, 0, &returned,
                       NULL))
  {
    TraceError(GetLastError(), "IOCTL_SERIAL_SET_MODEM_CONTROL");
    return FALSE;
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
#define IOCTL_SERIAL_GET_HANDFLOW       CTL_CODE(FILE_DEVICE_SERIAL_PORT,24,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_SET_HANDFLOW       CTL_CODE(FILE_DEVICE_SERIAL_PORT,25,METHOD_BUFFERED,FILE_ANY_ACCESS)

static BOOL GetHandFlow(HANDLE handle, SERIAL_HANDFLOW &serialHandFlow)
{
  DWORD returned;

  if (!DeviceIoControl(handle,
                       IOCTL_SERIAL_GET_HANDFLOW,
                       NULL, 0,
                       &serialHandFlow, sizeof(serialHandFlow), &returned,
                       NULL))
  {
    TraceError(GetLastError(), "IOCTL_SERIAL_GET_HANDFLOW");
    return FALSE;
  }

  return TRUE;
}

static void SetHandFlow(HANDLE handle, SERIAL_HANDFLOW &serialHandFlow)
{
  DWORD returned;

  if (!DeviceIoControl(handle,
                       IOCTL_SERIAL_SET_HANDFLOW,
                       &serialHandFlow, sizeof(serialHandFlow),
                       NULL, 0, &returned,
                       NULL))
  {
    TraceError(GetLastError(), "IOCTL_SERIAL_SET_HANDFLOW");
  }

  GetHandFlow(handle, serialHandFlow);
}
///////////////////////////////////////////////////////////////
static BOOL GetChars(HANDLE handle, SERIAL_CHARS &serialChars);
static BOOL GetBaudRate(HANDLE handle, SERIAL_BAUD_RATE &serialBaudRate);
static BOOL GetLineControl(HANDLE handle, SERIAL_LINE_CONTROL &serialLineControl);

BOOL ComIo::Open(const char *pPath, const ComParams &comParams)
{
  handle = CreateFile(
                    pPath,
                    GENERIC_READ|GENERIC_WRITE,
                    0,
                    NULL,
                    OPEN_EXISTING,
                    FILE_FLAG_OVERLAPPED,
                    NULL);

  if (handle == INVALID_HANDLE_VALUE) {
    TraceError(GetLastError(), "ComIo::Open(): CreateFile(\"%s\")", pPath);
    return FALSE;
  }

  DCB dcb;

  if (!myGetCommState(handle, &dcb)) {
    Close();
    return FALSE;
  }

  if (comParams.BaudRate() >= 0)
    dcb.BaudRate = (DWORD)comParams.BaudRate();

  if (comParams.ByteSize() >= 0)
    dcb.ByteSize = (BYTE)comParams.ByteSize();

  if (comParams.Parity() >= 0)
    dcb.Parity = (BYTE)comParams.Parity();

  if (comParams.StopBits() >= 0)
    dcb.StopBits = (BYTE)comParams.StopBits();

  if (comParams.OutCts() >= 0)
    dcb.fOutxCtsFlow = comParams.OutCts();

  if (comParams.OutDsr() >= 0)
    dcb.fOutxDsrFlow = comParams.OutDsr();

  if (comParams.OutX() >= 0)
    dcb.fOutX = comParams.OutX();

  if (comParams.InX() >= 0)
    dcb.fInX = comParams.InX();

  if (comParams.InDsr() >= 0)
    dcb.fDsrSensitivity = comParams.InDsr();

  dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
  dcb.fDtrControl = DTR_CONTROL_ENABLE;

  dcb.fParity = FALSE;
  dcb.fNull = FALSE;
  dcb.fAbortOnError = FALSE;
  dcb.fErrorChar = FALSE;

  if (!mySetCommState(handle, &dcb) ||
      !GetBaudRate(handle, serialBaudRate) ||
      !GetLineControl(handle, serialLineControl) ||
      !GetChars(handle, serialChars) ||
      !GetHandFlow(handle, serialHandFlow))
  {
    Close();
    return FALSE;
  }

  COMMTIMEOUTS timeouts;

  if (!myGetCommTimeouts(handle, &timeouts)) {
    Close();
    return FALSE;
  }

  if (comParams.IntervalTimeout() > 0) {
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.ReadIntervalTimeout = (DWORD)comParams.IntervalTimeout();
  } else {
    timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = MAXDWORD - 1;
    timeouts.ReadIntervalTimeout = MAXDWORD;
  }

  timeouts.WriteTotalTimeoutMultiplier = 0;
  timeouts.WriteTotalTimeoutConstant = 0;

  if (!mySetCommTimeouts(handle, &timeouts)) {
    Close();
    return FALSE;
  }

  cout
      << "Open("
      << "\"" << pPath << "\", baud=" << ComParams::BaudRateStr(serialBaudRate.BaudRate)
      << ", data=" << ComParams::ByteSizeStr(serialLineControl.WordLength)
      << ", parity=" << ComParams::ParityStr(serialLineControl.Parity)
      << ", stop=" << ComParams::StopBitsStr(serialLineControl.StopBits)
      << ", octs=" << ComParams::OutCtsStr((serialHandFlow.ControlHandShake & SERIAL_CTS_HANDSHAKE) != 0)
      << ", odsr=" << ComParams::OutDsrStr((serialHandFlow.ControlHandShake & SERIAL_DSR_HANDSHAKE) != 0)
      << ", ox=" << ComParams::OutXStr((serialHandFlow.FlowReplace & SERIAL_AUTO_TRANSMIT) != 0)
      << ", ix=" << ComParams::InXStr((serialHandFlow.FlowReplace & SERIAL_AUTO_RECEIVE) != 0)
      << ", idsr=" << ComParams::InDsrStr((serialHandFlow.ControlHandShake & SERIAL_DSR_SENSITIVITY) != 0)
      << ", ito=" << ComParams::IntervalTimeoutStr(timeouts.ReadIntervalTimeout)
      << ") - OK" << endl;

  hasExtendedModemControl = HasExtendedModemControl(handle);

  return TRUE;
}
///////////////////////////////////////////////////////////////
void ComIo::Close()
{
  if (handle != INVALID_HANDLE_VALUE) {
    CloseHandle(handle);
    handle = INVALID_HANDLE_VALUE;
  }
}
///////////////////////////////////////////////////////////////
BOOL ComIo::SetComEvents(DWORD *pEvents)
{
  if (!::SetCommMask(handle, *pEvents)) {
    TraceError(GetLastError(), "SetCommMask() %s", port.Name().c_str());
    myGetCommMask(handle, pEvents);
    return FALSE;
  }

  return myGetCommMask(handle, pEvents);
}
///////////////////////////////////////////////////////////////
void ComIo::SetPinState(WORD value, WORD mask)
{
  if (((mask & PIN_STATE_RTS) != 0 && !IsManualRts(serialHandFlow)) ||
      ((mask & PIN_STATE_DTR) != 0 && !IsManualDtr(serialHandFlow)))
  {
    if (mask & PIN_STATE_RTS)
      SetHandFlowRts(serialHandFlow, (value & PIN_STATE_RTS) ? SERIAL_RTS_CONTROL : 0);

    if (mask & PIN_STATE_DTR)
      SetHandFlowDtr(serialHandFlow, (value & PIN_STATE_DTR) ? SERIAL_DTR_CONTROL : 0);

    SetHandFlow(handle, serialHandFlow);

    mask &= ~(PIN_STATE_RTS|PIN_STATE_DTR);
  }

  if (mask & SPS_V2P_MCR(-1)) {
    if (hasExtendedModemControl) {
      if (!SetModemControl(handle, SPS_P2V_MCR(value), SPS_P2V_MCR(mask))) {
        cerr << port.Name() << " WARNING: can't change MCR state" << endl;
        mask &= ~SPS_V2P_MCR(-1);
      }
    } else {
      _ASSERTE((mask & SPS_V2P_MCR(-1) & ~(PIN_STATE_RTS|PIN_STATE_DTR)) == 0);

      if (mask & PIN_STATE_RTS) {
        if (!CommFunction(handle, (value & PIN_STATE_RTS) ? SETRTS : CLRRTS)) {
          cerr << port.Name() << " WARNING: can't change RTS state" << endl;
          mask &= ~PIN_STATE_RTS;
        }
      }
      if (mask & PIN_STATE_DTR) {
        if (!CommFunction(handle, (value & PIN_STATE_DTR) ? SETDTR : CLRDTR)) {
          cerr << port.Name() << " WARNING: can't change DTR state" << endl;
          mask &= ~PIN_STATE_DTR;
        }
      }
    }

    if (mask & PIN_STATE_RTS)
      SetHandFlowRts(serialHandFlow, (value & PIN_STATE_RTS) ? SERIAL_RTS_CONTROL : 0);

    if (mask & PIN_STATE_DTR)
      SetHandFlowDtr(serialHandFlow, (value & PIN_STATE_DTR) ? SERIAL_DTR_CONTROL : 0);
  }

  if (mask & PIN_STATE_BREAK) {
    if (!CommFunction(handle, (value & PIN_STATE_BREAK) ? SETBREAK : CLRBREAK))
      cerr << port.Name() << " WARNING: can't change BREAK state" << endl;
  }
}
///////////////////////////////////////////////////////////////
#define IOCTL_SERIAL_GET_CHARS          CTL_CODE(FILE_DEVICE_SERIAL_PORT,22,METHOD_BUFFERED,FILE_ANY_ACCESS)

static BOOL GetChars(HANDLE handle, SERIAL_CHARS &serialChars)
{
  DWORD returned;

  if (!DeviceIoControl(handle,
                       IOCTL_SERIAL_GET_CHARS,
                       NULL, 0,
                       &serialChars, sizeof(serialChars), &returned,
                       NULL))
  {
    TraceError(GetLastError(), "IOCTL_SERIAL_GET_CHARS");
    return FALSE;
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
#define IOCTL_SERIAL_SET_BAUD_RATE      CTL_CODE(FILE_DEVICE_SERIAL_PORT, 1,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_GET_BAUD_RATE      CTL_CODE(FILE_DEVICE_SERIAL_PORT,20,METHOD_BUFFERED,FILE_ANY_ACCESS)

static BOOL GetBaudRate(HANDLE handle, SERIAL_BAUD_RATE &serialBaudRate)
{
  DWORD returned;

  if (!DeviceIoControl(handle,
                       IOCTL_SERIAL_GET_BAUD_RATE,
                       NULL, 0,
                       &serialBaudRate, sizeof(serialBaudRate), &returned,
                       NULL))
  {
    TraceError(GetLastError(), "IOCTL_SERIAL_GET_BAUD_RATE");
    return FALSE;
  }

  return TRUE;
}

DWORD ComIo::SetBaudRate(DWORD baudRate)
{
  if (BaudRate() == baudRate)
    return baudRate;

  DWORD returned;
  SERIAL_BAUD_RATE newSerialBaudRate;

  newSerialBaudRate.BaudRate = baudRate;

  if (!DeviceIoControl(handle,
                       IOCTL_SERIAL_SET_BAUD_RATE,
                       &newSerialBaudRate, sizeof(newSerialBaudRate),
                       NULL, 0, &returned,
                       NULL))
  {
    TraceError(GetLastError(), "IOCTL_SERIAL_SET_BAUD_RATE");
  }

  if (GetBaudRate(handle, newSerialBaudRate))
    serialBaudRate = newSerialBaudRate;

  return BaudRate();
}
///////////////////////////////////////////////////////////////
#define IOCTL_SERIAL_SET_LINE_CONTROL   CTL_CODE(FILE_DEVICE_SERIAL_PORT, 3,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_SERIAL_GET_LINE_CONTROL   CTL_CODE(FILE_DEVICE_SERIAL_PORT,21,METHOD_BUFFERED,FILE_ANY_ACCESS)

static BOOL GetLineControl(HANDLE handle, SERIAL_LINE_CONTROL &serialLineControl)
{
  DWORD returned;

  if (!DeviceIoControl(handle,
                       IOCTL_SERIAL_GET_LINE_CONTROL,
                       NULL, 0,
                       &serialLineControl, sizeof(serialLineControl), &returned,
                       NULL))
  {
    TraceError(GetLastError(), "IOCTL_SERIAL_GET_LINE_CONTROL");
    return FALSE;
  }

  return TRUE;
}

DWORD ComIo::SetLineControl(DWORD lineControl)
{
  _ASSERTE((lineControl & ~(VAL2LC_BYTESIZE(-1)|LC_MASK_BYTESIZE
                           |VAL2LC_PARITY(-1)|LC_MASK_PARITY
                           |VAL2LC_STOPBITS(-1)|LC_MASK_STOPBITS)) == 0);

  if (LineControl() == lineControl)
    return lineControl;

  DWORD returned;
  SERIAL_LINE_CONTROL newSerialLineControl = serialLineControl;

  if (lineControl & LC_MASK_BYTESIZE)
    newSerialLineControl.WordLength = LC2VAL_BYTESIZE(lineControl);
  else
    newSerialLineControl.WordLength = serialLineControl.WordLength;

  if (lineControl & LC_MASK_PARITY)
    newSerialLineControl.Parity = LC2VAL_PARITY(lineControl);
  else
    newSerialLineControl.Parity = serialLineControl.Parity;

  if (lineControl & LC_MASK_STOPBITS)
    newSerialLineControl.StopBits = LC2VAL_STOPBITS(lineControl);
  else
    newSerialLineControl.StopBits = serialLineControl.StopBits;

  if (!DeviceIoControl(handle,
                       IOCTL_SERIAL_SET_LINE_CONTROL,
                       &newSerialLineControl, sizeof(newSerialLineControl),
                       NULL, 0, &returned,
                       NULL))
  {
    TraceError(GetLastError(), "IOCTL_SERIAL_SET_LINE_CONTROL");
  }

  if (GetLineControl(handle, newSerialLineControl))
    serialLineControl = newSerialLineControl;

  return LineControl();
}
///////////////////////////////////////////////////////////////
static ULONG GetEscCaps(HANDLE handle)
{
  BYTE inBufIoctl[sizeof(UCHAR) + C0CE_SIGNATURE_SIZE + sizeof(ULONG)];
  inBufIoctl[0] = 0;  // disable inserting
  memcpy(inBufIoctl + sizeof(UCHAR), C0CE_SIGNATURE, C0CE_SIGNATURE_SIZE);
  *(ULONG *)(inBufIoctl + sizeof(UCHAR) + C0CE_SIGNATURE_SIZE) = C0CE_INSERT_IOCTL_CAPS;

  BYTE outBuf[C0CE_SIGNATURE_SIZE + sizeof(ULONG)];

  DWORD returned;

  if (!DeviceIoControl(handle,
                       IOCTL_SERIAL_LSRMST_INSERT,
                       inBufIoctl, sizeof(inBufIoctl),
                       outBuf, sizeof(outBuf), &returned,
                       NULL))
  {
    TraceError(GetLastError(), "IOCTL_SERIAL_LSRMST_INSERT");
    return 0;
  }

  if (returned < (C0CE_SIGNATURE_SIZE + sizeof(ULONG)) ||
      memcmp(outBuf, C0CE_SIGNATURE, C0CE_SIGNATURE_SIZE) != 0)
  {
    return C0CE_INSERT_ENABLE_LSR|C0CE_INSERT_ENABLE_MST;  // standard functionality
  }

  return *(ULONG *)(outBuf + C0CE_SIGNATURE_SIZE);
}

DWORD ComIo::SetEscMode(DWORD escOptions, BYTE **ppBuf, DWORD *pDone)
{
  _ASSERTE(ppBuf != NULL);
  _ASSERTE(*ppBuf == NULL);
  _ASSERTE(pDone != NULL);
  _ASSERTE(*pDone == 0);

  BYTE escapeChar = ESC_OPTS_O2V_ESCCHAR(escOptions);

  if (!escapeChar)
    return escOptions | ESC_OPTS_V2O_ESCCHAR(-1);

  ULONG opts = (C0CE_INSERT_IOCTL_GET|C0CE_INSERT_IOCTL_RXCLEAR);

#define MODEM_STATUS_BITS (MODEM_STATUS_CTS|MODEM_STATUS_DSR|MODEM_STATUS_RI|MODEM_STATUS_DCD)
#define LINE_STATUS_BITS  (LINE_STATUS_OE|LINE_STATUS_PE|LINE_STATUS_FE|LINE_STATUS_BI|LINE_STATUS_FIFOERR)

  if (escOptions & ESC_OPTS_MAP_GO1_2_EO(GO1_V2O_MODEM_STATUS(MODEM_STATUS_BITS)))
    opts |= C0CE_INSERT_ENABLE_MST;

  if (escOptions & ESC_OPTS_MAP_GO1_2_EO(GO1_BREAK_STATUS))
    opts |= C0CE_INSERT_ENABLE_LSR_BI;

  if (escOptions & ESC_OPTS_MAP_GO1_2_EO(GO1_V2O_LINE_STATUS(LINE_STATUS_BITS)))
    opts |= C0CE_INSERT_ENABLE_LSR;

  if (escOptions & ESC_OPTS_MAP_GO1_2_EO(GO1_RBR_STATUS))
    opts |= C0CE_INSERT_ENABLE_RBR;

  if (escOptions & ESC_OPTS_MAP_GO1_2_EO(GO1_RLC_STATUS))
    opts |= C0CE_INSERT_ENABLE_RLC;

  opts &= GetEscCaps(handle);

  if (!opts)
    return escOptions | ESC_OPTS_V2O_ESCCHAR(-1);

  BYTE inBufIoctl[sizeof(UCHAR) + C0CE_SIGNATURE_SIZE + sizeof(ULONG)];
  inBufIoctl[0] = escapeChar;
  memcpy(inBufIoctl + sizeof(UCHAR), C0CE_SIGNATURE, C0CE_SIGNATURE_SIZE);
  *(ULONG *)(inBufIoctl + sizeof(UCHAR) + C0CE_SIGNATURE_SIZE) = opts;

  DWORD lenOutBufIoctl = 0;

  if (opts & (C0CE_INSERT_ENABLE_LSR|C0CE_INSERT_ENABLE_LSR_BI))
    lenOutBufIoctl += sizeof(UCHAR)*2 + sizeof(UCHAR);

  if (opts & C0CE_INSERT_ENABLE_MST)
    lenOutBufIoctl += sizeof(UCHAR)*2 + sizeof(UCHAR);

  if (opts & C0CE_INSERT_ENABLE_RBR)
    lenOutBufIoctl += sizeof(UCHAR)*2 + sizeof(ULONG);

  if (opts & C0CE_INSERT_ENABLE_RLC)
    lenOutBufIoctl += sizeof(UCHAR)*2 + sizeof(UCHAR)*3;

  if (lenOutBufIoctl) {
    *ppBuf = pBufAlloc(lenOutBufIoctl);

    if (!*ppBuf)
      lenOutBufIoctl = 0;
  }

  if (!DeviceIoControl(handle,
                       IOCTL_SERIAL_LSRMST_INSERT,
                       inBufIoctl, sizeof(inBufIoctl),
                       *ppBuf, lenOutBufIoctl, pDone,
                       NULL))
  {
    TraceError(GetLastError(), "IOCTL_SERIAL_LSRMST_INSERT");

    *pDone = 0;
    return escOptions | ESC_OPTS_V2O_ESCCHAR(-1);
  }

  if (lenOutBufIoctl && (opts & C0CE_INSERT_IOCTL_GET) == 0) {
    BYTE *pBuf = *ppBuf;

    if (opts & (C0CE_INSERT_ENABLE_LSR|C0CE_INSERT_ENABLE_LSR_BI)) {
      *pBuf++ = escapeChar;
      *pBuf++ = SERIAL_LSRMST_LSR_NODATA;
      *pBuf++ = (LINE_STATUS_THRE | LINE_STATUS_TEMT);
    }

    if (opts & C0CE_INSERT_ENABLE_MST) {
      DWORD stat;

      if (::GetCommModemStatus(handle, &stat)) {
        *pBuf++ = escapeChar;
        *pBuf++ = SERIAL_LSRMST_MST;
        *pBuf++ = (BYTE)stat;
      }
    }

    *pDone = (DWORD)(pBuf - *ppBuf);
  }

  if (opts & C0CE_INSERT_ENABLE_MST)
    escOptions &= ~ESC_OPTS_MAP_GO1_2_EO(GO1_V2O_MODEM_STATUS(MODEM_STATUS_BITS));

  if (opts & C0CE_INSERT_ENABLE_LSR_BI)
    escOptions &= ~ESC_OPTS_MAP_GO1_2_EO(GO1_BREAK_STATUS);

  if (opts & C0CE_INSERT_ENABLE_LSR)
    escOptions &= ~ESC_OPTS_MAP_GO1_2_EO(GO1_V2O_LINE_STATUS(LINE_STATUS_BITS));

  if (opts & C0CE_INSERT_ENABLE_RBR)
    escOptions &= ~ESC_OPTS_MAP_GO1_2_EO(GO1_RBR_STATUS);

  if (opts & C0CE_INSERT_ENABLE_RLC)
    escOptions &= ~ESC_OPTS_MAP_GO1_2_EO(GO1_RLC_STATUS);

  return escOptions & ~ESC_OPTS_V2O_ESCCHAR(-1);
}
///////////////////////////////////////////////////////////////
VOID CALLBACK WriteOverlapped::OnWrite(
    DWORD err,
    DWORD done,
    LPOVERLAPPED pOverlapped)
{
  WriteOverlapped *pOver = (WriteOverlapped *)pOverlapped;

  pOver->BufFree();

  if (err != ERROR_SUCCESS && err != ERROR_OPERATION_ABORTED)
    TraceError(err, "WriteOverlapped::OnWrite: %s", pOver->pComIo->port.Name().c_str());

  pOver->pComIo->port.OnWrite(pOver, pOver->len, done);
}

void WriteOverlapped::BufFree()
{
  _ASSERTE(pBuf != NULL);

  pBufFree(pBuf);

#ifdef _DEBUG
  pBuf = NULL;
#endif
}

BOOL WriteOverlapped::StartWrite(ComIo *_pComIo, BYTE *_pBuf, DWORD _len)
{
  _ASSERTE(pBuf == NULL);

  ::memset((OVERLAPPED *)this, 0, sizeof(OVERLAPPED));

  _ASSERTE(_pComIo != NULL);
  _ASSERTE(_pBuf != NULL);
  _ASSERTE(_len != 0);

  if (!::WriteFileEx(_pComIo->Handle(), _pBuf, _len, this, OnWrite)) {
    TraceError(GetLastError(), "WriteOverlapped::StartWrite(): WriteFileEx(%x) %s", _pComIo->Handle(), _pComIo->port.Name().c_str());
    return FALSE;
  }

  pComIo = _pComIo;
  pBuf = _pBuf;
  len = _len;

  return TRUE;
}
///////////////////////////////////////////////////////////////
ReadOverlapped::ReadOverlapped(ComIo &_comIo)
  : comIo(_comIo),
    pBuf(NULL)
{
}

ReadOverlapped::~ReadOverlapped()
{
  pBufFree(pBuf);
}

VOID CALLBACK ReadOverlapped::OnRead(
    DWORD err,
    DWORD done,
    LPOVERLAPPED pOverlapped)
{
  ReadOverlapped *pOver = (ReadOverlapped *)pOverlapped;

  if (err != ERROR_SUCCESS) {
    TraceError(err, "ReadOverlapped::OnRead(): %s", pOver->comIo.port.Name().c_str());
    done = 0;
  }

  BYTE *pInBuf = pOver->pBuf;
  pOver->pBuf = NULL;

  pOver->comIo.port.OnRead(pOver, pInBuf, done);
}

BOOL ReadOverlapped::StartRead()
{
  ::memset((OVERLAPPED *)this, 0, sizeof(OVERLAPPED));

  #define readBufSize 64

  pBuf = pBufAlloc(readBufSize);

  if (!pBuf)
    return FALSE;

  if (::ReadFileEx(comIo.Handle(), pBuf, readBufSize, this, OnRead))
    return TRUE;

  TraceError(GetLastError(), "ReadOverlapped::StartRead(): ReadFileEx() %s", comIo.port.Name().c_str());
  return FALSE;
}
///////////////////////////////////////////////////////////////
static HANDLE hThread = INVALID_HANDLE_VALUE;

WaitCommEventOverlapped::WaitCommEventOverlapped(ComIo &_comIo)
  : comIo(_comIo),
    hWait(INVALID_HANDLE_VALUE)
{
#ifdef _DEBUG
  static DWORD idThread;

  if (hThread == INVALID_HANDLE_VALUE) {
    idThread = ::GetCurrentThreadId();
  } else {
    _ASSERTE(idThread == ::GetCurrentThreadId());
  }
#endif  /* _DEBUG */

  if (hThread == INVALID_HANDLE_VALUE) {
    if (!::DuplicateHandle(::GetCurrentProcess(),
                           ::GetCurrentThread(),
                           ::GetCurrentProcess(),
                           &hThread,
                           0,
                           FALSE,
                           DUPLICATE_SAME_ACCESS))
    {
      hThread = INVALID_HANDLE_VALUE;

      TraceError(
          GetLastError(),
          "WaitCommEventOverlapped::WaitCommEventOverlapped(): DuplicateHandle() %s",
          comIo.port.Name().c_str());

      return;
    }
  }

  ::memset((OVERLAPPED *)this, 0, sizeof(OVERLAPPED));

  hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

  if (!hEvent) {
    TraceError(
        GetLastError(),
        "WaitCommEventOverlapped::WaitCommEventOverlapped(): CreateEvent() %s",
        comIo.port.Name().c_str());

    return;
  }

  if (!::RegisterWaitForSingleObject(&hWait, hEvent, OnCommEvent, this, INFINITE, WT_EXECUTEINWAITTHREAD)) {
    TraceError(
        GetLastError(),
        "WaitCommEventOverlapped::StartWaitCommEvent(): RegisterWaitForSingleObject() %s",
        comIo.port.Name().c_str());

    hWait = INVALID_HANDLE_VALUE;

    return;
  }
}

WaitCommEventOverlapped::~WaitCommEventOverlapped()
{
  if (hWait != INVALID_HANDLE_VALUE) {
    if (!::UnregisterWait(hWait)) {
      TraceError(
          GetLastError(),
          "WaitCommEventOverlapped::~WaitCommEventOverlapped(): UnregisterWait() %s",
          comIo.port.Name().c_str());
    }
  }

  if (hEvent) {
    if (!::CloseHandle(hEvent)) {
      TraceError(
          GetLastError(),
          "WaitCommEventOverlapped::~WaitCommEventOverlapped(): CloseHandle(hEvent) %s",
          comIo.port.Name().c_str());
    }
  }
}

VOID CALLBACK WaitCommEventOverlapped::OnCommEvent(
    PVOID pOverlapped,
    BOOLEAN /*timerOrWaitFired*/)
{
  ::QueueUserAPC(OnCommEvent, hThread, (ULONG_PTR)pOverlapped);
}

VOID CALLBACK WaitCommEventOverlapped::OnCommEvent(ULONG_PTR pOverlapped)
{
  WaitCommEventOverlapped *pOver = (WaitCommEventOverlapped *)pOverlapped;

  DWORD done;

  if (!::GetOverlappedResult(pOver->comIo.Handle(), pOver, &done, FALSE)) {
    TraceError(
        GetLastError(),
        "WaitCommEventOverlapped::OnCommEvent(): GetOverlappedResult() %s",
        pOver->comIo.port.Name().c_str());

    pOver->eMask = 0;
  }

  pOver->comIo.port.OnCommEvent(pOver, pOver->eMask);
}

BOOL WaitCommEventOverlapped::StartWaitCommEvent()
{
  if (!hEvent)
    return FALSE;

  if (!::WaitCommEvent(comIo.Handle(), &eMask, this)) {
    DWORD err = ::GetLastError();

    if (err != ERROR_IO_PENDING) {
      TraceError(
          err,
          "WaitCommEventOverlapped::StartWaitCommEvent(): WaitCommEvent() %s",
          comIo.port.Name().c_str());
      return FALSE;
    }
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
