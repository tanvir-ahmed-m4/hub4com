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
 * Revision 1.1  2008/03/26 08:44:34  vfrolov
 * Redesigned for using plugins
 *
 * Revision 1.4  2007/04/16 07:33:38  vfrolov
 * Fixed LostReport()
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

#include "precomp.h"
#include "comport.h"
#include "comio.h"
#include "comparams.h"
#include "import.h"

///////////////////////////////////////////////////////////////
ComPort::ComPort(
    const ComParams &comParams,
    const char *pPath)
  : hMasterPort(NULL),
    hHub(NULL),
    countReadOverlapped(0),
    countWaitCommEventOverlapped(0),
    countXoff(0),
    filterX(FALSE),
    writeQueueLimit(256),
    writeQueued(0),
    writeLost(0),
    writeLostTotal(0),
    errors(0)
{
  filterX = comParams.InX();
  string path(pPath);
  name = path.substr(path.rfind('\\') + 1);
  handle = ::OpenComPort(pPath, comParams);
}

BOOL ComPort::Init(HMASTERPORT _hMasterPort, HHUB _hHub)
{
  if (handle == INVALID_HANDLE_VALUE) {
    cerr << "ComPort::Init(): Invalid handle" << endl;
    return FALSE;
  }

  hMasterPort = _hMasterPort;
  hHub = _hHub;

  return TRUE;
}

BOOL ComPort::Start()
{
  _ASSERTE(hMasterPort != NULL);
  _ASSERTE(hHub != NULL);
  _ASSERTE(handle != INVALID_HANDLE_VALUE);

  CheckComEvents(DWORD(-1));

  DWORD events;

  if (!::GetCommMask(handle, &events)) {
    DWORD err = ::GetLastError();

    cerr << "ComPort::Start(): GetCommMask() ERROR " << err << endl;
    return FALSE;
  }

  if (events && !StartWaitCommEvent())
    return FALSE;

  if (!StartRead())
    return FALSE;

  HUB_MSG msg;

  msg.type = HUB_MSG_TYPE_CONNECT;
  msg.u.val = TRUE;

  pOnRead(hHub, hMasterPort, &msg);

  return TRUE;
}

BOOL ComPort::StartRead()
{
  if (countReadOverlapped)
    return TRUE;

  if (handle == INVALID_HANDLE_VALUE)
    return FALSE;

  ReadOverlapped *pOverlapped;

  pOverlapped = new ReadOverlapped(*this);

  if (!pOverlapped)
    return FALSE;

  if (!pOverlapped->StartRead()) {
    delete pOverlapped;
    return FALSE;
  }

  countReadOverlapped++;

  //cout << "Started Read " << name << " " << countReadOverlapped << endl;

  return TRUE;
}

BOOL ComPort::Write(HUB_MSG *pMsg)
{
  _ASSERTE(pMsg != NULL);

  if (pMsg->type == HUB_MSG_TYPE_LINE_DATA) {
    BYTE *pBuf = pMsg->u.buf.pBuf;
    DWORD len = pMsg->u.buf.size;

    if (!len)
      return TRUE;

    if (!pBuf) {
      writeLost += len;
      return FALSE;
    }

    if (filterX) {
      BYTE *pSrc = pBuf;
      BYTE *pDst = pBuf;

      for (DWORD i = 0 ; i < len ; i++) {
        if (*pSrc == 0x11 || *pSrc == 0x13)
          pSrc++;
        else
          *pDst++ = *pSrc++;
      }

      len = DWORD(pDst - pBuf);
    }

    if (!len)
      return TRUE;

    if (handle == INVALID_HANDLE_VALUE) {
      writeLost += len;
      return FALSE;
    }

    WriteOverlapped *pOverlapped;

    pOverlapped = new WriteOverlapped(*this, pBuf, len);

    if (!pOverlapped) {
      writeLost += len;
      return FALSE;
    }

    pMsg->type = HUB_MSG_TYPE_EMPTY;

    if (writeQueued > writeQueueLimit)
      ::PurgeComm(handle, PURGE_TXABORT|PURGE_TXCLEAR);

    if (!pOverlapped->StartWrite()) {
      writeLost += len;
      delete pOverlapped;
      return FALSE;
    }

    if (writeQueued <= writeQueueLimit/2 && (writeQueued + len) > writeQueueLimit/2)
      pOnXoff(hHub, hMasterPort);

    writeQueued += len;

    //cout << "Started Write " << name << " " << len << " " << writeQueued << endl;
  }
  else
  if (pMsg->type == HUB_MSG_TYPE_COM_FUNCTION) {
    if (handle == INVALID_HANDLE_VALUE)
      return FALSE;

    if (!::EscapeCommFunction(handle, pMsg->u.val))
      return FALSE;
  }

  return TRUE;
}

BOOL ComPort::StartWaitCommEvent()
{
  if (countWaitCommEventOverlapped)
    return TRUE;

  if (handle == INVALID_HANDLE_VALUE)
    return FALSE;

  WaitCommEventOverlapped *pOverlapped;

  pOverlapped = new WaitCommEventOverlapped(*this);

  if (!pOverlapped)
    return FALSE;

  if (!pOverlapped->StartWaitCommEvent()) {
    delete pOverlapped;
    return FALSE;
  }

  countWaitCommEventOverlapped++;

  //cout << "Started WaitCommEvent " << name << " " << countReadOverlapped << endl;

  return TRUE;
}

void ComPort::OnWrite(WriteOverlapped *pOverlapped, DWORD len, DWORD done)
{
  delete pOverlapped;

  if (len > done)
    writeLost += len - done;

  if (writeQueued > writeQueueLimit/2 && (writeQueued - len) <= writeQueueLimit/2)
    pOnXon(hHub, hMasterPort);

  writeQueued -= len;
}

void ComPort::OnRead(ReadOverlapped *pOverlapped, BYTE *pBuf, DWORD done)
{
  HUB_MSG msg;

  msg.type = HUB_MSG_TYPE_LINE_DATA;
  msg.u.buf.pBuf = pBuf;
  msg.u.buf.size = done;

  pOnRead(hHub, hMasterPort, &msg);

  if (countXoff > 0 || !pOverlapped->StartRead()) {
    delete pOverlapped;
    countReadOverlapped--;

    //cout << "Stopped Read " << name << " " << countReadOverlapped << endl;
  }
}

void ComPort::OnCommEvent(WaitCommEventOverlapped *pOverlapped, DWORD eMask)
{
  CheckComEvents(eMask);

  if (!pOverlapped->StartWaitCommEvent()) {
    delete pOverlapped;
    countWaitCommEventOverlapped--;

    cout << "Stopped WaitCommEvent " << name << " " << countWaitCommEventOverlapped << endl;
  }
}

void ComPort::CheckComEvents(DWORD eMask)
{
  if ((eMask & (EV_CTS|EV_DSR|EV_RLSD|EV_RING)) != 0) {
    DWORD stat;

    if (::GetCommModemStatus(handle, &stat)) {
      HUB_MSG msg;

      msg.type = HUB_MSG_TYPE_MODEM_STATUS;
      msg.u.val = stat;

      pOnRead(hHub, hMasterPort, &msg);
    }
  }

  if ((eMask & (EV_BREAK|EV_ERR)) != 0) {
    DWORD errs;

    if (!::ClearCommError(handle, &errs, NULL))
      errs = 0;

    if (errs) {
      errors |= errs;

      HUB_MSG msg;

      msg.type = HUB_MSG_TYPE_COM_ERRORS;
      msg.u.val = errs;

      pOnRead(hHub, hMasterPort, &msg);
    }
  }
}

void ComPort::AddXoff(int count)
{
  _ASSERTE(handle != INVALID_HANDLE_VALUE);

  countXoff += count;

  if (countXoff <= 0)
    StartRead();
}

void ComPort::LostReport()
{
  _ASSERTE(handle != INVALID_HANDLE_VALUE);

  if (writeLost) {
    writeLostTotal += writeLost;
    cout << "Write lost " << name << ": " << writeLost << ", total " << writeLostTotal << endl;
    writeLost = 0;
  }

  CheckComEvents(EV_BREAK|EV_ERR);

  if (errors) {
    cout << "Error " << name << ":";

    if (errors & CE_RXOVER) { cout << " RXOVER"; errors &= ~CE_RXOVER; }
    if (errors & CE_OVERRUN) { cout << " OVERRUN"; errors &= ~CE_OVERRUN; }
    if (errors & CE_RXPARITY) { cout << " RXPARITY"; errors &= ~CE_RXPARITY; }
    if (errors & CE_FRAME) { cout << " FRAME"; errors &= ~CE_FRAME; }
    if (errors & CE_BREAK) { cout << " BREAK"; errors &= ~CE_BREAK; }
    if (errors & CE_TXFULL) { cout << " TXFULL"; errors &= ~CE_TXFULL; }
    if (errors) { cout << " 0x" << hex << errors << dec; errors = 0; }

    #define IOCTL_SERIAL_GET_STATS CTL_CODE(FILE_DEVICE_SERIAL_PORT,35,METHOD_BUFFERED,FILE_ANY_ACCESS)

    typedef struct _SERIALPERF_STATS {
      ULONG ReceivedCount;
      ULONG TransmittedCount;
      ULONG FrameErrorCount;
      ULONG SerialOverrunErrorCount;
      ULONG BufferOverrunErrorCount;
      ULONG ParityErrorCount;
    } SERIALPERF_STATS, *PSERIALPERF_STATS;

    SERIALPERF_STATS stats;
    DWORD size;

    if (DeviceIoControl(handle, IOCTL_SERIAL_GET_STATS, NULL, 0, &stats, sizeof(stats), &size, NULL)) {
      cout << ", total"
        << " RXOVER=" << stats.BufferOverrunErrorCount
        << " OVERRUN=" << stats.SerialOverrunErrorCount
        << " RXPARITY=" << stats.ParityErrorCount
        << " FRAME=" << stats.FrameErrorCount;
    }

    cout << endl;
  }
}
///////////////////////////////////////////////////////////////
