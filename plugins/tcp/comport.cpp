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
 * Revision 1.8  2008/11/17 16:44:57  vfrolov
 * Fixed race conditions
 *
 * Revision 1.7  2008/11/13 07:41:09  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.6  2008/10/22 15:31:38  vfrolov
 * Fixed race condition
 *
 * Revision 1.5  2008/10/06 12:15:14  vfrolov
 * Added --reconnect option
 *
 * Revision 1.4  2008/10/02 08:07:02  vfrolov
 * Fixed sending not paired CONNECT(FALSE)
 *
 * Revision 1.3  2008/09/30 06:44:50  vfrolov
 * Added warning about ignored output options
 *
 * Revision 1.2  2008/03/28 16:00:19  vfrolov
 * Added connectionCounter
 *
 * Revision 1.1  2008/03/27 17:18:27  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "../plugins_api.h"
///////////////////////////////////////////////////////////////
namespace PortTcp {
///////////////////////////////////////////////////////////////
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
    pOverlapped->Delete();
    return FALSE;
  }

  if (!Listen(hSockListen))
    return FALSE;

  return TRUE;
}

BOOL Listener::OnEvent(ListenOverlapped * /*pOverlapped*/, long e, int /*err*/)
{
  //cout << "Listener::OnEvent " << hex << e << dec << " " << err << endl;

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
  return PortTcp::Accept(hSockListen);
}
///////////////////////////////////////////////////////////////
ComPort::ComPort(
    vector<Listener *> &listeners,
    const ComParams &comParams,
    const char *pPath)
  : pListener(NULL),
    hSock(INVALID_SOCKET),
    isConnected(FALSE),
    isDisconnected(FALSE),
    connectionCounter(0),
    reconnectTime(-1),
    hReconnectTimer(NULL),
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

    if (comParams.GetReconnectTime() == comParams.rtDefault) {
      if (permanent)
        reconnectTime = 0;
    }
    else
    if (comParams.GetReconnectTime() != comParams.rtDisable) {
      reconnectTime = comParams.GetReconnectTime();
    }
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
    if (CanConnect())
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

  if (!StartWaitEvent(hSock) || !Connect(hSock, snRemote)) {
    Close(hSock);
    hSock = INVALID_SOCKET;
  }
}

void ComPort::Accept()
{
  _ASSERTE(pListener);
  _ASSERTE(hSock == INVALID_SOCKET);

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

    if (hSock == INVALID_SOCKET || isDisconnected) {
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
      connectionCounter++;

      _ASSERTE(connectionCounter > 0);

      if (!pListener) {
        if (hReconnectTimer)
          ::CancelWaitableTimer(hReconnectTimer);

        StartConnect();
      }
    } else {
      _ASSERTE(connectionCounter > 0);

      connectionCounter--;

      if (hSock != INVALID_SOCKET && !permanent && connectionCounter <= 0)
        PortTcp::Disconnect(hSock);
    }
  }
  else
  if (pMsg->type == HUB_MSG_TYPE_SET_OUT_OPTS) {
    if (pMsg->u.val) {
      cerr << name << " WARNING: Requested output option(s) [0x"
           << hex << pMsg->u.val << dec
           << "] will be ignored by driver" << endl;
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
    pOverlapped->Delete();
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

  if (done == 0 && isDisconnected) {
    isDisconnected = FALSE;
    OnDisconnect();
  }

  if (countXoff > 0 || !isConnected || !pOverlapped->StartRead()) {
    _ASSERTE(countReadOverlapped > 0);

    delete pOverlapped;
    countReadOverlapped--;
  }
}

static VOID CALLBACK TimerAPCProc(
  LPVOID pArg,
  DWORD /*dwTimerLowValue*/,
  DWORD /*dwTimerHighValue*/)
{
  if (((ComPort *)pArg)->CanConnect())
    ((ComPort *)pArg)->StartConnect();
}

void ComPort::OnDisconnect()
{
  Close(hSock);
  hSock = INVALID_SOCKET;

  if (isConnected) {
    isConnected = FALSE;

    HUB_MSG msg;

    msg.type = HUB_MSG_TYPE_CONNECT;
    msg.u.val = FALSE;

    pOnRead(hHub, hMasterPort, &msg);
  }

  if (pListener) {
    pListener->push(this);
  }
  else
  if (CanConnect()) {
    if (reconnectTime == 0) {
      StartConnect();
    }
    else
    if (reconnectTime > 0) {
      if (!hReconnectTimer)
        hReconnectTimer = ::CreateWaitableTimer(NULL, FALSE, NULL);

      if (hReconnectTimer) {
        LARGE_INTEGER firstReportTime;

        firstReportTime.QuadPart = -10000LL * reconnectTime;

        if (!::SetWaitableTimer(hReconnectTimer, &firstReportTime, 0, TimerAPCProc, this, FALSE)) {
          DWORD err = GetLastError();

          cerr << "WARNING: SetWaitableTimer() - error=" << err << endl;
        }
      } else {
        DWORD err = GetLastError();

        cerr << "WARNING: CreateWaitableTimer() - error=" << err << endl;
      }
    }
  }
}

BOOL ComPort::OnEvent(WaitEventOverlapped *pOverlapped, long e)
{
  //cout << "ComPort::OnEvent " << name << " " << hex << e << dec << endl;

  _ASSERTE(hSock == pOverlapped->Sock());

  if (e == FD_CLOSE) {
    if (countReadOverlapped > 0)
      isDisconnected = TRUE;
    else
      OnDisconnect();

    pOverlapped->Delete();
    return FALSE;
  }
  else
  if (e == FD_CONNECT) {
    OnConnect();
  }

  return TRUE;
}

void ComPort::OnConnect()
{
  _ASSERTE(isConnected == FALSE);
  _ASSERTE(isDisconnected == FALSE);

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
} // end namespace
///////////////////////////////////////////////////////////////
