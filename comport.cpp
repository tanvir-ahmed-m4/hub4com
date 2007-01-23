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
 * Revision 1.1  2007/01/23 09:13:10  vfrolov
 * Initial revision
 *
 *
 */

#include "precomp.h"
#include "comport.h"
#include "comhub.h"
#include "comio.h"

///////////////////////////////////////////////////////////////
ComPort::ComPort(ComHub &_hub)
  : handle(INVALID_HANDLE_VALUE),
    hub(_hub),
    writeQueued(0)
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

  string path(pPath);

  name = path.substr(path.rfind('\\') + 1);

  handle = ::OpenComPort(pPath, comParams);

  return handle != INVALID_HANDLE_VALUE;
}

BOOL ComPort::Start()
{
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

  cout << "Started " << name << endl;

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

  if (writeQueued > 256)
    PurgeComm(handle, PURGE_TXABORT|PURGE_TXCLEAR);

  if (!pOverlapped->StartWrite()) {
    delete pOverlapped;
    return FALSE;
  }

  //cout << "Started Write " << pOverlapped->i << " " << path << " " << len << " " << writeQueued << endl;

  return TRUE;
}

void ComPort::OnRead(LPCVOID pBuf, DWORD done)
{
  hub.Write(this, pBuf, done);
}

void ComPort::LostReport()
{
  if (writeLost) {
    writeLostTotal += writeLost;
    cout << "Write lost: " << name << " " << writeLost << " total " << writeLostTotal << endl;
    writeLost = 0;
  }
}
///////////////////////////////////////////////////////////////
