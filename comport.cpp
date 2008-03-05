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
 * Revision 1.5  2008/03/05 13:24:19  vfrolov
 * Added restoring base 10 notation
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
 *
 */

#include "precomp.h"
#include "comport.h"
#include "comhub.h"
#include "comio.h"
#include "comparams.h"

///////////////////////////////////////////////////////////////
ComPort::ComPort(ComHub &_hub)
  : handle(INVALID_HANDLE_VALUE),
    hub(_hub),
    countReadOverlapped(0),
    countXoff(0),
    filterX(FALSE),
    writeQueueLimit(256),
    writeQueued(0),
    writeLostTotal(0)
{
}

BOOL ComPort::Open(const char *pPath, const ComParams &comParams)
{
  if (handle != INVALID_HANDLE_VALUE)
    return FALSE;

  while (writeQueued) {
    if (SleepEx(5000, TRUE) != WAIT_IO_COMPLETION)
      return FALSE;
  }

  writeLost = 0;
  filterX = comParams.InX();

  string path(pPath);

  name = path.substr(path.rfind('\\') + 1);

  handle = ::OpenComPort(pPath, comParams);

  return handle != INVALID_HANDLE_VALUE;
}

BOOL ComPort::Start()
{
  if (!StartRead())
    return FALSE;

  cout << "Started " << name << endl;

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

BOOL ComPort::Write(LPCVOID pData, DWORD len)
{
  if (handle == INVALID_HANDLE_VALUE) {
    writeLost += len;
    return FALSE;
  }

  WriteOverlapped *pOverlapped;

  pOverlapped = new WriteOverlapped(*this, pData, len);

  if (!pOverlapped)
    return FALSE;

  if (filterX) {
    len = pOverlapped->FilterX();

    if (!len) {
      delete pOverlapped;
      return TRUE;
    }
  }

  if (writeQueued > writeQueueLimit)
    PurgeComm(handle, PURGE_TXABORT|PURGE_TXCLEAR);

  if (!pOverlapped->StartWrite()) {
    writeLost += len;
    delete pOverlapped;
    return FALSE;
  }

  if (writeQueued <= writeQueueLimit/2 && (writeQueued + len) > writeQueueLimit/2)
    hub.AddXoff(this, 1);

  writeQueued += len;

  //cout << "Started Write " << name << " " << len << " " << writeQueued << endl;

  return TRUE;
}

void ComPort::OnWrite(WriteOverlapped *pOverlapped, DWORD len, DWORD done)
{
  delete pOverlapped;

  if (len > done)
    writeLost += len - done;

  if (writeQueued > writeQueueLimit/2 && (writeQueued - len) <= writeQueueLimit/2)
    hub.AddXoff(this, -1);

  writeQueued -= len;
}

void ComPort::OnRead(ReadOverlapped *pOverlapped, LPCVOID pBuf, DWORD done)
{
  if (done)
    hub.Write(this, pBuf, done);

  if (countXoff > 0 || !pOverlapped->StartRead()) {
    delete pOverlapped;
    countReadOverlapped--;

    //cout << "Stopped Read " << name << " " << countReadOverlapped << endl;
  }
}

void ComPort::AddXoff(int count)
{
  countXoff += count;

  if (countXoff <= 0)
    StartRead();
}

void ComPort::LostReport()
{
  if (writeLost) {
    writeLostTotal += writeLost;
    cout << "Write lost " << name << ": " << writeLost << ", total " << writeLostTotal << endl;
    writeLost = 0;
  }

  DWORD errors;

  if (ClearCommError(handle, &errors, NULL) && errors) {
    cout << "Error " << name << ":";

    if (errors & CE_RXOVER) { cout << " RXOVER"; errors &= ~CE_RXOVER; }
    if (errors & CE_OVERRUN) { cout << " OVERRUN"; errors &= ~CE_OVERRUN; }
    if (errors & CE_RXPARITY) { cout << " RXPARITY"; errors &= ~CE_RXPARITY; }
    if (errors & CE_FRAME) { cout << " FRAME"; errors &= ~CE_FRAME; }
    if (errors & CE_BREAK) { cout << " BREAK"; errors &= ~CE_BREAK; }
    if (errors & CE_TXFULL) { cout << " TXFULL"; errors &= ~CE_TXFULL; }
    if (errors) { cout << " 0x" << hex << errors << dec; }

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
