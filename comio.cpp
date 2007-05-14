/*
 * $Id$
 *
 * Copyright (c) 2006-2007 Vyacheslav Frolov
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
 *
 */

#include "precomp.h"
#include "comio.h"
#include "comport.h"
#include "comparams.h"

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
static BOOL myGetCommState(HANDLE handle, DCB *dcb)
{
  dcb->DCBlength = sizeof(*dcb);

  if (!GetCommState(handle, dcb)) {
    TraceError(GetLastError(), "GetCommState()");
    return FALSE;
  }
  return TRUE;
}

static BOOL mySetCommState(HANDLE handle, DCB *dcb)
{
  if (!SetCommState(handle, dcb)) {
    TraceError(GetLastError(), "SetCommState()");
    return FALSE;
  }
  return TRUE;
}
///////////////////////////////////////////////////////////////
HANDLE OpenComPort(const char *pPath, const ComParams &comParams)
{
  HANDLE handle = CreateFile(pPath,
                    GENERIC_READ|GENERIC_WRITE,
                    0,
                    NULL,
                    OPEN_EXISTING,
                    FILE_FLAG_OVERLAPPED,
                    NULL);

  if (handle == INVALID_HANDLE_VALUE) {
    TraceError(GetLastError(), "OpenComPort(): CreateFile(\"%s\")", pPath);
    return INVALID_HANDLE_VALUE;
  }

  DCB dcb;

  if (!myGetCommState(handle, &dcb)) {
    CloseHandle(handle);
    return INVALID_HANDLE_VALUE;
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

  if (!mySetCommState(handle, &dcb)) {
    CloseHandle(handle);
    return INVALID_HANDLE_VALUE;
  }

  COMMTIMEOUTS timeouts;

  if (!GetCommTimeouts(handle, &timeouts)) {
    TraceError(GetLastError(), "OpenComPort(): GetCommTimeouts()");
    CloseHandle(handle);
    return INVALID_HANDLE_VALUE;
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

  if (!SetCommTimeouts(handle, &timeouts)) {
    TraceError(GetLastError(), "OpenComPort(): SetCommTimeouts()");
    CloseHandle(handle);
    return INVALID_HANDLE_VALUE;
  }

  cout
      << "Open("
      << "\"" << pPath << "\", baud=" << ComParams::BaudRateStr(dcb.BaudRate)
      << ", data=" << ComParams::ByteSizeStr(dcb.ByteSize)
      << ", parity=" << ComParams::ParityStr(dcb.Parity)
      << ", stop=" << ComParams::StopBitsStr(dcb.StopBits)
      << ", octs=" << ComParams::OutCtsStr(dcb.fOutxCtsFlow)
      << ", odsr=" << ComParams::OutDsrStr(dcb.fOutxDsrFlow)
      << ", ox=" << ComParams::OutXStr(dcb.fOutX)
      << ", ix=" << ComParams::InXStr(dcb.fInX)
      << ", idsr=" << ComParams::InDsrStr(dcb.fDsrSensitivity)
      << ", ito=" << comParams.IntervalTimeoutStr()
      << ") - OK" << endl;
  return handle;
}
///////////////////////////////////////////////////////////////
WriteOverlapped::WriteOverlapped(ComPort &_port, const void *_pBuf, DWORD _len)
  : port(_port),
    len(_len)
{
  pBuf = new BYTE[len];

  for (DWORD i = 0 ; i < len ; i++) {
    pBuf[i] = ((const BYTE *)_pBuf)[i];
  }
}

WriteOverlapped::~WriteOverlapped()
{
  if (pBuf)
    delete [] pBuf;
}

DWORD WriteOverlapped::FilterX()
{
  BYTE *pSrc = pBuf;
  BYTE *pDst = pBuf;

  for (DWORD i = 0 ; i < len ; i++) {
    if (*pSrc == 0x11 || *pSrc == 0x13)
      pSrc++;
    else
      *pDst++ = *pSrc++;
  }

  return len = DWORD(pDst - pBuf);
}

VOID CALLBACK WriteOverlapped::OnWrite(
    DWORD err,
    DWORD done,
    LPOVERLAPPED pOverlapped)
{
  WriteOverlapped *pOver = (WriteOverlapped *)pOverlapped;

  if (err != ERROR_SUCCESS && err != ERROR_OPERATION_ABORTED)
    TraceError(err, "WriteOverlapped::OnWrite: %s", pOver->port.Name().c_str());

  pOver->port.OnWrite(pOver, pOver->len, done);
}

BOOL WriteOverlapped::StartWrite()
{
  ::memset((OVERLAPPED *)this, 0, sizeof(OVERLAPPED));

  if (!pBuf)
    return FALSE;

  if (!::WriteFileEx(port.Handle(), pBuf, len, this, OnWrite))
    return FALSE;

  return TRUE;
}
///////////////////////////////////////////////////////////////
VOID CALLBACK ReadOverlapped::OnRead(
    DWORD err,
    DWORD done,
    LPOVERLAPPED pOverlapped)
{
  ReadOverlapped *pOver = (ReadOverlapped *)pOverlapped;

  if (err != ERROR_SUCCESS) {
    TraceError(err, "ReadOverlapped::OnRead(): %s", pOver->port.Name().c_str());
    done = 0;
  }

  pOver->port.OnRead(pOver, pOver->buf, done);
}

BOOL ReadOverlapped::StartRead()
{
  ::memset((OVERLAPPED *)this, 0, sizeof(OVERLAPPED));

  if (::ReadFileEx(port.Handle(), buf, sizeof(buf), this, OnRead)) {
    return TRUE;
  }

  TraceError(GetLastError(), "ReadOverlapped::StartRead(): ReadFileEx() %s", port.Name().c_str());
  return FALSE;
}
///////////////////////////////////////////////////////////////
