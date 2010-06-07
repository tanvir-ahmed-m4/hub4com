/*
 * $Id$
 *
 * Copyright (c) 2008-2010 Vyacheslav Frolov
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
 * Revision 1.20  2010/06/07 14:54:48  vfrolov
 * Added "Connected" and "Disconnected" messages (feature request #3010158)
 *
 * Revision 1.19  2009/09/14 09:08:48  vfrolov
 * Added discarding owned tick (for optimization)
 *
 * Revision 1.18  2009/08/11 06:12:45  vfrolov
 * Added missing initialization of isValid
 *
 * Revision 1.17  2009/08/04 11:36:49  vfrolov
 * Implemented priority and reject modifiers for <listen port>
 *
 * Revision 1.16  2009/07/31 11:40:04  vfrolov
 * Fixed pending acception of incoming connections
 *
 * Revision 1.15  2009/02/17 14:17:37  vfrolov
 * Redesigned timer's API
 *
 * Revision 1.14  2009/01/23 16:55:05  vfrolov
 * Utilized timer routines
 *
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

void Listener::Push(ComPort *pPort)
{
  _ASSERTE(pPort != NULL);

  ports.push(pPort);
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
    if (!ports.empty()) {
      ComPort *pPort = ports.top().Ptr();
      _ASSERTE(pPort != NULL);

      if (pPort->Accept())
        ports.pop();
    } else {
      PortTcp::Accept("Listener", hSockListen, CF_DEFER);
    }
  }

  return TRUE;
}

void Listener::OnDisconnect(ComPort *pPort)
{
  Push(pPort);

  _ASSERTE(!ports.empty());

  for (;;) {
    ComPort *pPort = ports.top().Ptr();
    _ASSERTE(pPort != NULL);

    if (!pPort->Accept())
      break;

    ports.pop();

    if (ports.empty())
      break;
  }
}

SOCKET Listener::Accept(const ComPort &port, int cmd)
{
  return PortTcp::Accept(port.Name().c_str(), hSockListen, cmd);
}
///////////////////////////////////////////////////////////////
ComPort::ComPort(
    vector<Listener *> &listeners,
    const ComParams &comParams,
    const char *pPath)
  : pListener(NULL),
    rejectZeroConnectionCounter(FALSE),
    priority(0),
    hSock(INVALID_SOCKET),
    isValid(TRUE),
    isConnected(FALSE),
    isDisconnected(FALSE),
    connectionCounter(0),
    permanent(FALSE),
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

  string path(pPath);

  for ( ;; path = path.substr(1)) {
    switch (path[0]) {
      case '*':
        permanent = TRUE;
        continue;
      case '!':
        rejectZeroConnectionCounter = TRUE;
        continue;
    }
    break;
  }

  string::size_type iDelim = path.find(':');

  if (iDelim != path.npos) {
    string addrName = path.substr(0, iDelim);
    string portName = path.substr(iDelim + 1);

    if (!SetAddr(snLocal, comParams.GetIF(), NULL) ||
        !SetAddr(snRemote, addrName.c_str(), portName.c_str()))
    {
      isValid = FALSE;
      return;
    }

    if (comParams.GetReconnectTime() == comParams.rtDefault) {
      if (permanent)
        reconnectTime = 0;
    }
    else
    if (comParams.GetReconnectTime() != comParams.rtDisable) {
      reconnectTime = comParams.GetReconnectTime();
    }
  } else {
    iDelim = path.find('/');

    if (iDelim != path.npos) {
      istringstream buf(path.substr(iDelim + 1));
      path = path.substr(0, iDelim);

      buf >> priority;

      if (buf.fail())
        isValid = FALSE;

      string rest;
      buf >> rest;

      if (!rest.empty())
        isValid = FALSE;

      if (!isValid) {
        cerr << "ERROR: Invalid priority in " << pPath << endl;
        return;
      }
    }

    if (!SetAddr(snLocal, comParams.GetIF(), path.c_str())) {
      isValid = FALSE;
      return;
    }

    for (vector<Listener *>::iterator i = listeners.begin() ; i != listeners.end() ; i++) {
      if ((*i)->IsEqual(snLocal)) {
        pListener = *i;
        break;
      }
    }

    if (!pListener) {
      pListener = new Listener(snLocal);

      if (!pListener) {
        cerr << "No enough memory." << endl;
        exit(2);
      }

      listeners.push_back(pListener);
    }

    pListener->Push(this);
  }

  for (int i = 0 ; i < 3 ; i++) {
    WriteOverlapped *pOverlapped = new WriteOverlapped(*this);

    if (!pOverlapped) {
      cerr << "No enough memory." << endl;
      exit(2);
    }

    writeOverlappedBuf.push(pOverlapped);
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

BOOL ComPort::FakeReadFilter(HUB_MSG *pInMsg)
{
  _ASSERTE(pInMsg != NULL);

  switch (HUB_MSG_T2N(pInMsg->type)) {
    case HUB_MSG_T2N(HUB_MSG_TYPE_TICK): {
      if (pInMsg->u.hv2.hVal0 != this)
        break;

      if (pInMsg->u.hv2.hVal1 == hReconnectTimer) {
        if (CanConnect())
          StartConnect();
      }

      // discard owned tick
      if (!pMsgReplaceNone(pInMsg, HUB_MSG_TYPE_EMPTY))
        return FALSE;

      break;
    }
  }

  return pInMsg != NULL;
}

void ComPort::StartConnect()
{
  _ASSERTE(!pListener);

  if (hSock != INVALID_SOCKET)
    return;

  hSock = Socket(snLocal);

  if (hSock == INVALID_SOCKET)
    return;

  if (!StartWaitEvent(hSock) || !Connect(name.c_str(), hSock, snRemote)) {
    Close(name.c_str(), hSock);
    hSock = INVALID_SOCKET;
  }
}

BOOL ComPort::Accept()
{
  _ASSERTE(pListener);
  _ASSERTE(hSock == INVALID_SOCKET);

  for (;;) {
    hSock = pListener->Accept(*this,
        (rejectZeroConnectionCounter && connectionCounter <= 0) ? CF_REJECT : CF_ACCEPT);

    if (hSock == INVALID_SOCKET)
      break;

    if (StartWaitEvent(hSock)) {
      OnConnect();
      return TRUE;
    }

    Close(name.c_str(), hSock);
  }

  return FALSE;
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
        PortTcp::Disconnect(name.c_str(), hSock);
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

void ComPort::OnDisconnect()
{
  cout << name << ": Disconnected" << endl;

  Close(name.c_str(), hSock);
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
    pListener->OnDisconnect(this);
  }
  else
  if (CanConnect()) {
    if (reconnectTime == 0) {
      StartConnect();
    }
    else
    if (reconnectTime > 0) {
      if (!hReconnectTimer)
        hReconnectTimer = pTimerCreate((HTIMEROWNER)this);

      if (hReconnectTimer) {
        LARGE_INTEGER firstReportTime;

        firstReportTime.QuadPart = -10000LL * reconnectTime;

        pTimerSet(
            hReconnectTimer,
            hMasterPort,
            &firstReportTime, 0,
            (HTIMERPARAM)hReconnectTimer);
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

  cout << name << ": Connected" << endl;

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
