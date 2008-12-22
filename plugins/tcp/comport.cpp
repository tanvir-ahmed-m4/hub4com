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
 * Revision 1.13  2008/12/22 09:40:46  vfrolov
 * Optimized message switching
 *
 * Revision 1.12  2008/12/01 17:09:34  vfrolov
 * Improved write buffering
 *
 * Revision 1.11  2008/11/27 13:46:29  vfrolov
 * Added --write-limit option
 *
 * Revision 1.10  2008/11/24 16:30:56  vfrolov
 * Removed pOnXoffXon
 *
 * Revision 1.9  2008/11/24 12:37:00  vfrolov
 * Changed plugin API
 *
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
    countReadOverlapped(0),
    countXoff(0),
    writeQueueLimit(comParams.WriteQueueLimit()),
    writeQueued(0),
    writeSuspended(FALSE),
    writeLost(0),
    writeLostTotal(0),
    pWriteBuf(NULL),
    lenWriteBuf(0)
{
  writeQueueLimitSendXoff = (writeQueueLimit*2)/3;
  writeQueueLimitSendXon = writeQueueLimit/3;

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

  for (int i = 0 ; i < 3 ; i++) {
    WriteOverlapped *pOverlapped = new WriteOverlapped(*this);

    if (pOverlapped)
      writeOverlappedBuf.push(pOverlapped);
    else
      isValid = FALSE;
  }
}

BOOL ComPort::Init(HMASTERPORT _hMasterPort)
{
  hMasterPort = _hMasterPort;

  return isValid;
}

BOOL ComPort::Start()
{
  _ASSERTE(hMasterPort != NULL);

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

void ComPort::FlowControlUpdate()
{
  if (writeSuspended) {
    if (writeQueued <= writeQueueLimitSendXon) {
      writeSuspended = FALSE;

      HUB_MSG msg;

      msg.type = HUB_MSG_TYPE_ADD_XOFF_XON;
      msg.u.val = FALSE;

      pOnRead(hMasterPort, &msg);
    }
  } else {
    if (writeQueued > writeQueueLimitSendXoff) {
      writeSuspended = TRUE;

      HUB_MSG msg;

      msg.type = HUB_MSG_TYPE_ADD_XOFF_XON;
      msg.u.val = TRUE;

      pOnRead(hMasterPort, &msg);
    }
  }
}

BOOL ComPort::Write(HUB_MSG *pMsg)
{
  _ASSERTE(pMsg != NULL);

  switch (HUB_MSG_T2N(pMsg->type)) {
  case HUB_MSG_T2N(HUB_MSG_TYPE_LINE_DATA): {
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

    if (writeQueued > writeQueueLimit) {
      if (lenWriteBuf) {
        _ASSERTE(pWriteBuf != NULL);

        writeLost += lenWriteBuf;
        writeQueued -= lenWriteBuf;
        lenWriteBuf = 0;
        pBufFree(pWriteBuf);
        pWriteBuf = NULL;
      }
    }

    if (isConnected && writeOverlappedBuf.size()) {
      _ASSERTE(pWriteBuf == NULL);
      _ASSERTE(lenWriteBuf == 0);

      WriteOverlapped *pOverlapped = writeOverlappedBuf.front();

      _ASSERTE(pOverlapped != NULL);

      if (!pOverlapped->StartWrite(pBuf, len)) {
        writeLost += len;
        FlowControlUpdate();
        return FALSE;
      }

      writeOverlappedBuf.pop();
      pMsg->type = HUB_MSG_TYPE_EMPTY;  // detach pBuf
    } else {
      _ASSERTE((pWriteBuf == NULL && lenWriteBuf == 0) || (pWriteBuf != NULL && lenWriteBuf != 0));

      pBufAppend(&pWriteBuf, lenWriteBuf, pBuf, len);
      lenWriteBuf += len;
    }

    writeQueued += len;
    FlowControlUpdate();

    //cout << "Started Write " << name << " " << len << " " << writeQueued << endl;
    break;
  }
  case HUB_MSG_T2N(HUB_MSG_TYPE_CONNECT): {
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
    break;
  }
  case HUB_MSG_T2N(HUB_MSG_TYPE_SET_OUT_OPTS):
    if (pMsg->u.val) {
      cerr << name << " WARNING: Requested output option(s) [0x"
           << hex << pMsg->u.val << dec
           << "] will be ignored by driver" << endl;
    }
    break;
  case HUB_MSG_T2N(HUB_MSG_TYPE_ADD_XOFF_XON):
    if (pMsg->u.val) {
      countXoff++;
    } else {
      if (--countXoff == 0 && isConnected)
        StartRead();
    }
    break;
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
  //cout << name << " OnWrite " << ::GetCurrentThreadId() << " len=" << len << " done=" << done << " queued=" << writeQueued << endl;

  if (len > done)
    writeLost += len - done;

  writeQueued -= len;

  _ASSERTE(pWriteBuf != NULL || lenWriteBuf == 0);
  _ASSERTE(pWriteBuf == NULL || lenWriteBuf != 0);

  if (lenWriteBuf &&
      isConnected &&
      !isDisconnected &&
      hSock != INVALID_SOCKET)
  {
    if (!pOverlapped->StartWrite(pWriteBuf, lenWriteBuf)) {
      writeOverlappedBuf.push(pOverlapped);

      writeLost += lenWriteBuf;
      writeQueued -= lenWriteBuf;
      pBufFree(pWriteBuf);
    }

    lenWriteBuf = 0;
    pWriteBuf = NULL;
  } else {
    writeOverlappedBuf.push(pOverlapped);
  }

  FlowControlUpdate();
}

void ComPort::OnRead(ReadOverlapped *pOverlapped, BYTE *pBuf, DWORD done)
{
  HUB_MSG msg;

  msg.type = HUB_MSG_TYPE_LINE_DATA;
  msg.u.buf.pBuf = pBuf;
  msg.u.buf.size = done;

  pOnRead(hMasterPort, &msg);

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
    if (lenWriteBuf) {
      _ASSERTE(pWriteBuf != NULL);

      writeLost += lenWriteBuf;
      writeQueued -= lenWriteBuf;
      lenWriteBuf = 0;
      pBufFree(pWriteBuf);
      pWriteBuf = NULL;

      FlowControlUpdate();
    }

    isConnected = FALSE;

    HUB_MSG msg;

    msg.type = HUB_MSG_TYPE_CONNECT;
    msg.u.val = FALSE;

    pOnRead(hMasterPort, &msg);
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
  _ASSERTE(hSock != INVALID_SOCKET);

  isConnected = TRUE;

  if (countXoff <= 0)
    StartRead();

  if (lenWriteBuf && writeOverlappedBuf.size()) {
    WriteOverlapped *pOverlapped = writeOverlappedBuf.front();

    _ASSERTE(pOverlapped != NULL);

    if (pOverlapped->StartWrite(pWriteBuf, lenWriteBuf)) {
      writeOverlappedBuf.pop();
    } else {
      writeLost += lenWriteBuf;
      writeQueued -= lenWriteBuf;
      pBufFree(pWriteBuf);

      FlowControlUpdate();
    }

    lenWriteBuf = 0;
    pWriteBuf = NULL;
  }

  HUB_MSG msg;

  msg.type = HUB_MSG_TYPE_CONNECT;
  msg.u.val = TRUE;

  pOnRead(hMasterPort, &msg);
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
