/*
 * $Id$
 *
 * Copyright (c) 2008-2011 Vyacheslav Frolov
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
 * Revision 1.13  2011/12/08 09:38:23  vfrolov
 * Fixed compile warning
 *
 * Revision 1.12  2011/07/26 11:59:14  vfrolov
 * Replaced strerror() by FormatMessage()
 *
 * Revision 1.11  2010/09/14 18:34:30  vfrolov
 * Fixed rejected connections handling
 *
 * Revision 1.10  2009/09/14 09:05:28  vfrolov
 * Fixed data loss on disconnect.
 *
 * Revision 1.9  2009/08/04 11:36:49  vfrolov
 * Implemented priority and reject modifiers for <listen port>
 *
 * Revision 1.8  2008/12/05 14:20:55  vfrolov
 * Fixed race conditions
 *
 * Revision 1.7  2008/12/01 17:09:34  vfrolov
 * Improved write buffering
 *
 * Revision 1.6  2008/11/17 16:44:57  vfrolov
 * Fixed race conditions
 *
 * Revision 1.5  2008/11/13 07:41:09  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.4  2008/10/22 15:31:38  vfrolov
 * Fixed race condition
 *
 * Revision 1.3  2008/10/06 12:12:29  vfrolov
 * Duplicated code moved to SetThread()
 *
 * Revision 1.2  2008/08/26 14:07:01  vfrolov
 * Execute OnEvent() in main thread context
 *
 * Revision 1.1  2008/03/27 17:17:27  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "../plugins_api.h"
///////////////////////////////////////////////////////////////
namespace PortTcp {
///////////////////////////////////////////////////////////////
#include "comio.h"
#include "comport.h"
#include "import.h"
///////////////////////////////////////////////////////////////
static void TraceError(DWORD err, const char *pFmt, ...)
{
  va_list va;
  va_start(va, pFmt);
  vfprintf(stderr, pFmt, va);
  va_end(va);

  LPVOID pMsgBuf;

  FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      err,
      MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
      (LPTSTR) &pMsgBuf,
      0,
      NULL);

  if ((err & 0xFFFF0000) == 0)
    fprintf(stderr, " ERROR %lu - %s\n", (unsigned long)err, pMsgBuf);
  else
    fprintf(stderr, " ERROR 0x%08lX - %s\n", (unsigned long)err, pMsgBuf);

  fflush(stderr);

  LocalFree(pMsgBuf);
}
///////////////////////////////////////////////////////////////
BOOL SetAddr(struct sockaddr_in &sn, const char *pAddr, const char *pPort)
{
  memset(&sn, 0, sizeof(sn));
  sn.sin_family = AF_INET;

  if (pPort) {
    struct servent *pServEnt;

    pServEnt = getservbyname(pPort, "tcp");

    sn.sin_port = pServEnt ? pServEnt->s_port : htons((u_short)atoi(pPort));
  }

  sn.sin_addr.s_addr = pAddr ? inet_addr(pAddr) : INADDR_ANY;

  if (sn.sin_addr.s_addr == INADDR_NONE) {
    const struct hostent *pHostEnt = gethostbyname(pAddr);

    if (!pHostEnt) {
      TraceError(GetLastError(), "SetAddr(): gethostbyname(\"%s\")", pAddr);
      return FALSE;
    }

    memcpy(&sn.sin_addr, pHostEnt->h_addr, pHostEnt->h_length);
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
SOCKET Socket(const struct sockaddr_in &sn)
{
  SOCKET hSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  if (hSock == INVALID_SOCKET) {
    TraceError(GetLastError(), "Socket(): socket()");
    return INVALID_SOCKET;
  }

  if (bind(hSock, (struct sockaddr *)&sn, sizeof(sn)) == SOCKET_ERROR) {
    TraceError(GetLastError(), "Socket(): bind()");
    closesocket(hSock);
    return INVALID_SOCKET;
  }

  u_long addr = ntohl(sn.sin_addr.s_addr);
  u_short port  = ntohs(sn.sin_port);

  cout << "Socket("
       << ((addr >> 24) & 0xFF) << '.'
       << ((addr >> 16) & 0xFF) << '.'
       << ((addr >>  8) & 0xFF) << '.'
       << ( addr        & 0xFF) << ':'
       << port
       << ") = " << hex << hSock << dec << endl;

  return hSock;
}
///////////////////////////////////////////////////////////////
BOOL Connect(const char *pName, SOCKET hSock, const struct sockaddr_in &snRemote)
{
  if (connect(hSock, (struct sockaddr *)&snRemote, sizeof(snRemote)) == SOCKET_ERROR) {
    DWORD err = GetLastError();

    if (err != WSAEWOULDBLOCK) {
      TraceError(err, "Connect(%x): connect() %s", hSock, pName);
      return FALSE;
    }
  }

  u_long addr = ntohl(snRemote.sin_addr.s_addr);
  u_short port  = ntohs(snRemote.sin_port);

  cout << pName << ": Connect(" << hex << hSock << dec << ", "
       << ((addr >> 24) & 0xFF) << '.'
       << ((addr >> 16) & 0xFF) << '.'
       << ((addr >>  8) & 0xFF) << '.'
       << ( addr        & 0xFF) << ':'
       << port
       << ") ..." << endl;

  return TRUE;
}
///////////////////////////////////////////////////////////////
BOOL Listen(SOCKET hSock)
{
  if (listen(hSock, SOMAXCONN) == SOCKET_ERROR) {
    TraceError(GetLastError(), "Listen(%x): listen()", hSock);
    closesocket(hSock);
    return FALSE;
  }

  cout << "Listen(" << hex << hSock << dec << ") - OK" << endl;

  return TRUE;
}
///////////////////////////////////////////////////////////////
struct ConditionProcData {
  ConditionProcData(int _cmd) : cmd(_cmd) { ::memset(&sn, 0, sizeof(sn)); }

  void SetSN(char FAR* pBuf, u_long len) {
    if (pBuf != NULL && len != 0) {
      if (len > sizeof(sn))
        len = sizeof(sn);

      ::memcpy(&sn, pBuf, len);
    }
  }

  int cmd;
  struct sockaddr_in sn;
};

static int CALLBACK ConditionProc(
    IN LPWSABUF lpCallerId,
    IN LPWSABUF /*lpCallerData*/,
    IN OUT LPQOS /*lpSQOS*/,
    IN OUT LPQOS /*lpGQOS*/,
    IN LPWSABUF /*lpCalleeId*/,
    IN LPWSABUF /*lpCalleeData*/,
    OUT GROUP FAR * /*g*/,
    IN DWORD_PTR dwCallbackData)
{
  _ASSERTE((ConditionProcData *)dwCallbackData != NULL);

  if (lpCallerId)
    ((ConditionProcData *)dwCallbackData)->SetSN(lpCallerId->buf, lpCallerId->len);

  return ((ConditionProcData *)dwCallbackData)->cmd;
}

SOCKET Accept(const char *pName, SOCKET hSockListen, int cmd)
{
  BOOL defered = FALSE;

  for (;;) {
    ConditionProcData cpd(cmd);

    SOCKET hSock = WSAAccept(hSockListen, NULL, NULL, ConditionProc, (DWORD_PTR)&cpd);

    stringstream buf;

    if (hSock != INVALID_SOCKET) {
      buf << hex << hSock << dec;
    } else {
      DWORD err = GetLastError();

      switch (err) {
        case WSAEWOULDBLOCK:
          return INVALID_SOCKET;
        case WSAECONNREFUSED:
          buf << "rejected";
          break;
        case WSATRY_AGAIN:
          defered = TRUE;
          buf << "defered";
          break;
        case WSAECONNRESET:
          buf << "forcibly closed by the remote host";
          break;
        default:
          TraceError(err, "Accept(%x): accept() %s", hSockListen, pName);
          return INVALID_SOCKET;
      }
    }

    string result = buf.str();
    u_long addr = ntohl(cpd.sn.sin_addr.s_addr);
    u_short port  = ntohs(cpd.sn.sin_port);

    cout << pName << ": Accept(" << hex << hSockListen << dec << ") = " << result
         << " from "
         << ((addr >> 24) & 0xFF) << '.'
         << ((addr >> 16) & 0xFF) << '.'
         << ((addr >>  8) & 0xFF) << '.'
         << ( addr        & 0xFF) << ':'
         << port
         << endl;

    if (hSock != INVALID_SOCKET || defered)
      return hSock;
  }
}
///////////////////////////////////////////////////////////////
void Disconnect(const char *pName, SOCKET hSock)
{
  if (shutdown(hSock, SD_SEND) != 0)
    TraceError(GetLastError(), "Disconnect(%x): shutdown() %s", hSock, pName);
  else
    cout << pName << ": Disconnect(" << hex << hSock << dec << ") - OK" << endl;
}
///////////////////////////////////////////////////////////////
void Close(const char *pName, SOCKET hSock)
{
  if (hSock == INVALID_SOCKET)
    return;

  if (closesocket(hSock) != 0)
    TraceError(GetLastError(), "Close(): closesocket(%x) %s", hSock, pName);
  else
    cout << pName << ": Close(" << hex << hSock << dec << ") - OK" << endl;
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
    TraceError(err, "WriteOverlapped::OnWrite: %s", pOver->port.Name().c_str());

  pOver->port.OnWrite(pOver, pOver->len, done);
}

void WriteOverlapped::BufFree()
{
  _ASSERTE(pBuf != NULL);

  pBufFree(pBuf);

#ifdef _DEBUG
  pBuf = NULL;
#endif
}

BOOL WriteOverlapped::StartWrite(BYTE *_pBuf, DWORD _len)
{
  _ASSERTE(pBuf == NULL);

  ::memset((OVERLAPPED *)this, 0, sizeof(OVERLAPPED));

  _ASSERTE(_pBuf != NULL);
  _ASSERTE(_len != 0);

  if (!::WriteFileEx(port.Handle(), _pBuf, _len, this, OnWrite)) {
    TraceError(GetLastError(), "WriteOverlapped::StartWrite(): WriteFileEx(%x) %s", port.Handle(), port.Name().c_str());
    return FALSE;
  }

  pBuf = _pBuf;
  len = _len;

  return TRUE;
}
///////////////////////////////////////////////////////////////
ReadOverlapped::ReadOverlapped(ComPort &_port)
  : port(_port),
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
    TraceError(err, "ReadOverlapped::OnRead(): %s", pOver->port.Name().c_str());
    done = 0;
  }

  BYTE *pInBuf = pOver->pBuf;
  pOver->pBuf = NULL;

  pOver->port.OnRead(pOver, pInBuf, done);
}

BOOL ReadOverlapped::StartRead()
{
  ::memset((OVERLAPPED *)this, 0, sizeof(OVERLAPPED));

  #define readBufSize 64

  pBuf = pBufAlloc(readBufSize);

  if (!pBuf)
    return FALSE;

  if (!::ReadFileEx(port.Handle(), pBuf, readBufSize, this, OnRead)) {
    TraceError(GetLastError(), "ReadOverlapped::StartRead(): ReadFileEx(%x) %s", port.Handle(), port.Name().c_str());
    return FALSE;
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
static HANDLE hThread = INVALID_HANDLE_VALUE;
#ifdef _DEBUG
static DWORD idThread;
#endif  /* _DEBUG */

static BOOL SetThread()
{
#ifdef _DEBUG
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
          "SetThread(): DuplicateHandle()");

      return FALSE;
    }
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
WaitEventOverlapped::WaitEventOverlapped(ComPort &_port, SOCKET hSockWait)
  : port(_port),
    hSock(hSockWait),
    hWait(INVALID_HANDLE_VALUE)
{
  if (!SetThread())
      return;

  hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

  if (!hEvent) {
    TraceError(
        GetLastError(),
        "WaitEventOverlapped::WaitEventOverlapped(): CreateEvent() %s",
        port.Name().c_str());

    return;
  }

  if (!::RegisterWaitForSingleObject(&hWait, hEvent, OnEvent, this, INFINITE, WT_EXECUTEINWAITTHREAD)) {
    TraceError(
        GetLastError(),
        "WaitEventOverlapped::StartWaitEvent(): RegisterWaitForSingleObject() %s",
        port.Name().c_str());

    hWait = INVALID_HANDLE_VALUE;

    return;
  }
}

void WaitEventOverlapped::Delete()
{
  if (hWait != INVALID_HANDLE_VALUE) {
    if (!::UnregisterWait(hWait)) {
      DWORD err = GetLastError();

      if (err != ERROR_IO_PENDING) {
        TraceError(
            err,
            "WaitEventOverlapped::Delete(): UnregisterWait() %s",
            port.Name().c_str());
      }
    }
  }

  if (hEvent) {
    if (!::WSACloseEvent(hEvent)) {
      TraceError(
          GetLastError(),
          "WaitEventOverlapped::Delete(): CloseHandle(hEvent) %s",
          port.Name().c_str());
    }
  }

  SafeDelete::Delete();
}

VOID CALLBACK WaitEventOverlapped::OnEvent(
    PVOID pOverlapped,
    BOOLEAN /*timerOrWaitFired*/)
{
  ((WaitEventOverlapped *)pOverlapped)->LockDelete();
  if (!::QueueUserAPC(OnEvent, hThread, (ULONG_PTR)pOverlapped))
    ((WaitEventOverlapped *)pOverlapped)->UnockDelete();
}

VOID CALLBACK WaitEventOverlapped::OnEvent(ULONG_PTR pOverlapped)
{
  WaitEventOverlapped *pOver = (WaitEventOverlapped *)pOverlapped;

  if (!pOver->UnockDelete())
    return;

  WSANETWORKEVENTS events;

  if (::WSAEnumNetworkEvents(pOver->hSock, NULL, &events) != 0) {
    TraceError(
        GetLastError(),
        "WaitEventOverlapped::OnEvent: WSAEnumNetworkEvents() %s",
        pOver->port.Name().c_str());
  }

  if ((events.lNetworkEvents & FD_CONNECT) != 0) {
    if (events.iErrorCode[FD_CONNECT_BIT] != ERROR_SUCCESS) {
      TraceError(
          events.iErrorCode[FD_CONNECT_BIT],
          "Connect(%lx) %s",
          (long)pOver->hSock,
          pOver->port.Name().c_str());

      if (!pOver->port.OnEvent(pOver, FD_CLOSE))
        return;
    }
    else
    if (!pOver->port.OnEvent(pOver, FD_CONNECT))
      return;
  }

  if ((events.lNetworkEvents & FD_CLOSE) != 0) {
    if (!pOver->port.OnEvent(pOver, FD_CLOSE))
      return;
  }
}

BOOL WaitEventOverlapped::StartWaitEvent()
{
  if (!hEvent || hSock == INVALID_SOCKET || hWait == INVALID_HANDLE_VALUE)
    return FALSE;

  if (::WSAEventSelect(hSock, hEvent, FD_ACCEPT|FD_CONNECT|FD_CLOSE) != 0) {
    TraceError(
        ::GetLastError(),
        "WaitEventOverlapped::StartWaitEvent(): WSAEventSelect() %s",
        port.Name().c_str());
    return FALSE;
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
ListenOverlapped::ListenOverlapped(Listener &_listener, SOCKET hSockWait)
  : listener(_listener),
    hSock(hSockWait),
    hWait(INVALID_HANDLE_VALUE)
{
  if (!SetThread())
      return;

  hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

  if (!hEvent) {
    TraceError(
        GetLastError(),
        "ListenOverlapped::ListenOverlapped(): CreateEvent() %s");

    return;
  }

  if (!::RegisterWaitForSingleObject(&hWait, hEvent, OnEvent, this, INFINITE, WT_EXECUTEINWAITTHREAD)) {
    TraceError(
        GetLastError(),
        "ListenOverlapped::StartWaitEvent(): RegisterWaitForSingleObject() %s");

    hWait = INVALID_HANDLE_VALUE;

    return;
  }
}

void ListenOverlapped::Delete()
{
  if (hSock != INVALID_SOCKET) {
    if (closesocket(hSock) != 0) {
      TraceError(
          GetLastError(),
          "ListenOverlapped::~ListenOverlapped(): closesocket(%x) %s",
          hSock);
    }
    else
      cout << "Close(" << hex << hSock << dec << ") - OK" << endl;
  }

  if (hWait != INVALID_HANDLE_VALUE) {
    if (!::UnregisterWait(hWait)) {
      DWORD err = GetLastError();

      if (err != ERROR_IO_PENDING) {
        TraceError(
            err,
            "ListenOverlapped::~ListenOverlapped(): UnregisterWait() %s");
      }
    }
  }

  if (hEvent) {
    if (!::WSACloseEvent(hEvent)) {
      TraceError(
          GetLastError(),
          "ListenOverlapped::~ListenOverlapped(): CloseHandle(hEvent) %s");
    }
  }

  SafeDelete::Delete();
}

VOID CALLBACK ListenOverlapped::OnEvent(
    PVOID pOverlapped,
    BOOLEAN /*timerOrWaitFired*/)
{
  ((ListenOverlapped *)pOverlapped)->LockDelete();
  if (!::QueueUserAPC(OnEvent, hThread, (ULONG_PTR)pOverlapped))
    ((ListenOverlapped *)pOverlapped)->UnockDelete();
}

VOID CALLBACK ListenOverlapped::OnEvent(ULONG_PTR pOverlapped)
{
  ListenOverlapped *pOver = (ListenOverlapped *)pOverlapped;

  if (!pOver->UnockDelete())
    return;

  WSANETWORKEVENTS events;

  if (::WSAEnumNetworkEvents(pOver->hSock, NULL, &events) != 0) {
    TraceError(
        GetLastError(),
        "ListenOverlapped::OnEvent: WSAEnumNetworkEvents() %s");
  }

  if ((events.lNetworkEvents & FD_ACCEPT) != 0) {
    if (!pOver->listener.OnEvent(pOver, FD_ACCEPT, events.iErrorCode[FD_ACCEPT_BIT]))
      return;
  }
}

BOOL ListenOverlapped::StartWaitEvent()
{
  if (!hEvent || hSock == INVALID_SOCKET || hWait == INVALID_HANDLE_VALUE)
    return FALSE;

  if (::WSAEventSelect(hSock, hEvent, FD_ACCEPT) != 0) {
    TraceError(
        ::GetLastError(),
        "ListenOverlapped::StartWaitEvent(): WSAEventSelect() %s");
    return FALSE;
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
