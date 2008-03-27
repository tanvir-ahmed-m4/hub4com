/*
 * $Id$
 *
 * Copyright (c) 2008 Vyacheslav Frolov
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
 * Revision 1.1  2008/03/27 17:18:27  vfrolov
 * Initial revision
 *
 *
 */

#include "precomp.h"
#include "comport.h"
#include "comio.h"
#include "comparams.h"
#include "import.h"

///////////////////////////////////////////////////////////////
Listener::Listener(const struct sockaddr_in &_snLocal)
  : snLocal(_snLocal),
    hSockListen(INVALID_SOCKET)
{
}

BOOL Listener::Start()
{
  if (hSockListen != INVALID_SOCKET)
    return TRUE;

  hSockListen = Socket(snLocal);

  if (hSockListen == INVALID_SOCKET)
    return FALSE;

  ListenOverlapped *pOverlapped;

  pOverlapped = new ListenOverlapped(*this, hSockListen);

  if (!pOverlapped)
    return FALSE;

  if (!pOverlapped->StartWaitEvent()) {
    delete pOverlapped;
    return FALSE;
  }

  if (!Listen(hSockListen))
    return FALSE;

  return TRUE;
}

BOOL Listener::OnEvent(ListenOverlapped * /*pOverlapped*/, long e, int /*err*/)
{
  //cout << "OnEvent " << hex << e << dec << " " << err << endl;

  if (e == FD_ACCEPT) {
    if (!empty()) {
      ComPort *pPort = front();
      _ASSERTE(pPort != NULL);
      pop();

      pPort->Accept();
    }
  }

  return TRUE;
}

SOCKET Listener::Accept()
{
  return ::Accept(hSockListen);
}
///////////////////////////////////////////////////////////////
ComPort::ComPort(
    vector<Listener *> &listeners,
    const ComParams &comParams,
    const char *pPath)
  : pListener(NULL),
    hSock(INVALID_SOCKET),
    isConnected(FALSE),
    name("TCP"),
    hMasterPort(NULL),
    hHub(NULL),
    countReadOverlapped(0),
    countXoff(0),
    writeQueueLimit(256),
    writeQueued(0),
    writeLost(0),
    writeLostTotal(0)
{
  string path;

  if (*pPath == '*') {
    path = pPath + 1;
    permanent = TRUE;
  } else {
    path = pPath;
    permanent = FALSE;
  }

  string::size_type iDelim = path.find(':');

  if (iDelim != path.npos) {
    string addrName = path.substr(0, iDelim);
    string portName = path.substr(iDelim + 1);

    isValid =
      SetAddr(snLocal, comParams.GetIF(), NULL) &&
      SetAddr(snRemote, addrName.c_str(), portName.c_str());
  } else {
    isValid = SetAddr(snLocal, comParams.GetIF(), path.c_str());

    if (isValid) {
      for (vector<Listener *>::iterator i = listeners.begin() ; i != listeners.end() ; i++) {
        if ((*i)->IsEqual(snLocal)) {
          pListener = *i;
          break;
        }
      }

      if (!pListener) {
        pListener = new Listener(snLocal);

        if (pListener)
          listeners.push_back(pListener);
      }

      if (pListener)
        pListener->push(this);
      else
        isValid = FALSE;
    }
  }
}

BOOL ComPort::Init(HMASTERPORT _hMasterPort, HHUB _hHub)
{
  hMasterPort = _hMasterPort;
  hHub = _hHub;

  return isValid;
}

BOOL ComPort::Start()
{
  _ASSERTE(hMasterPort != NULL);
  _ASSERTE(hHub != NULL);

  if (pListener) {
    return pListener->Start();
  } else {
    if (permanent)
      StartConnect();
  }

  return TRUE;
}

BOOL ComPort::StartRead()
{
  if (countReadOverlapped)
    return TRUE;

  if (hSock == INVALID_SOCKET)
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


void ComPort::StartConnect()
{
  _ASSERTE(!pListener);

  if (hSock != INVALID_SOCKET)
    return;

  hSock = Socket(snLocal);

  if (hSock == INVALID_SOCKET)
    return;

  if (!StartWaitEvent(hSock) || !Connect(hSock, snRemote))
    hSock = INVALID_SOCKET;
}

void ComPort::Accept()
{
  _ASSERTE(pListener);

  if (hSock != INVALID_SOCKET)
    return;

  hSock = pListener->Accept();

  if (hSock != INVALID_SOCKET) {
    if (StartWaitEvent(hSock))
      OnConnect();
    else
      hSock = INVALID_SOCKET;
  }

  if (hSock == INVALID_SOCKET)
    pListener->push(this);
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

    if (hSock == INVALID_SOCKET) {
      writeLost += len;
      return FALSE;
    }

    if (isConnected) {
      WriteOverlapped *pOverlapped;

      pOverlapped = new WriteOverlapped(*this, pBuf, len);

      if (!pOverlapped) {
        writeLost += len;
        return FALSE;
      }

      pMsg->type = HUB_MSG_TYPE_EMPTY;

      //if (writeQueued > writeQueueLimit) {
      //}

      if (!pOverlapped->StartWrite()) {
        writeLost += len;
        delete pOverlapped;
        return FALSE;
      }
    } else {
      if (writeQueued > writeQueueLimit) {
        for (Bufs::const_iterator i = bufs.begin() ; i != bufs.end() ; i++) {
          pBufFree(i->pBuf);
          writeLost += i->len;
        }
        bufs.clear();
      }

      bufs.push_back(Buf(pBuf, len));
      pMsg->type = HUB_MSG_TYPE_EMPTY;
    }

    if (writeQueued <= writeQueueLimit/2 && (writeQueued + len) > writeQueueLimit/2)
      pOnXoff(hHub, hMasterPort);

    writeQueued += len;

    //cout << "Started Write " << name << " " << len << " " << writeQueued << endl;
  }
  else
  if (pMsg->type == HUB_MSG_TYPE_CONNECT) {
    if (pMsg->u.val) {
      if (!pListener)
        StartConnect();
    } else {
      if (hSock != INVALID_SOCKET && !permanent) {
        Disconnect(hSock);

        isConnected = FALSE;
        hSock = INVALID_SOCKET;

        HUB_MSG msg;

        msg.type = HUB_MSG_TYPE_CONNECT;
        msg.u.val = FALSE;

        pOnRead(hHub, hMasterPort, &msg);
      }
    }
  }

  return TRUE;
}

BOOL ComPort::StartWaitEvent(SOCKET hSockWait)
{
  if (hSockWait == INVALID_SOCKET)
    return FALSE;

  WaitEventOverlapped *pOverlapped;

  pOverlapped = new WaitEventOverlapped(*this, hSockWait);

  if (!pOverlapped)
    return FALSE;

  if (!pOverlapped->StartWaitEvent()) {
    delete pOverlapped;
    return FALSE;
  }

  //cout << "Started WaitEvent " << name << endl;

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

  if (countXoff > 0 || !isConnected || !pOverlapped->StartRead()) {
    delete pOverlapped;
    countReadOverlapped--;

    //cout << "Stopped Read " << name << " " << countReadOverlapped << endl;
  }
}

BOOL ComPort::OnEvent(WaitEventOverlapped *pOverlapped, long e, int err)
{
  //cout << "OnEvent " << name << " " << hex << e << dec << " " << err << endl;

  if (e == FD_CLOSE || (e == FD_CONNECT && err != ERROR_SUCCESS)) {
    if (hSock == pOverlapped->Sock()) {
      isConnected = FALSE;
      hSock = INVALID_SOCKET;

      HUB_MSG msg;

      msg.type = HUB_MSG_TYPE_CONNECT;
      msg.u.val = FALSE;

      pOnRead(hHub, hMasterPort, &msg);

      if (pListener)
        Accept();
      else
      if (permanent)
        StartConnect();
    }

    delete pOverlapped;
    return FALSE;
  }
  else
  if (e == FD_CONNECT) {
    if (hSock == pOverlapped->Sock()) {
      OnConnect();
    } else {
      delete pOverlapped;
      return FALSE;
    }
  }

  return TRUE;
}

void ComPort::OnConnect()
{
  isConnected = TRUE;

  if (countXoff <= 0)
    StartRead();

  for (Bufs::const_iterator i = bufs.begin() ; i != bufs.end() ; i++) {
    WriteOverlapped *pWriteOverlapped;

    pWriteOverlapped = new WriteOverlapped(*this, i->pBuf, i->len);

    if (!pWriteOverlapped) {
      pBufFree(i->pBuf);
      writeLost += i->len;
      continue;
    }

    if (!pWriteOverlapped->StartWrite()) {
      writeLost += i->len;
      delete pWriteOverlapped;
      continue;
    }
  }
  bufs.clear();

  HUB_MSG msg;

  msg.type = HUB_MSG_TYPE_CONNECT;
  msg.u.val = TRUE;

  pOnRead(hHub, hMasterPort, &msg);
}

void ComPort::AddXoff(int count)
{
  countXoff += count;

  if (countXoff <= 0 && isConnected)
    StartRead();
}

void ComPort::LostReport()
{
  if (writeLost) {
    writeLostTotal += writeLost;
    cout << "Write lost " << name << ": " << writeLost << ", total " << writeLostTotal << endl;
    writeLost = 0;
  }
}
///////////////////////////////////////////////////////////////
