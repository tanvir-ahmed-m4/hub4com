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

  fprintf(stderr, " ERROR %s (%lu)\n", strerror(err), (unsigned long)err);
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
BOOL Connect(SOCKET hSock, const struct sockaddr_in &snRemote)
{
  if (connect(hSock, (struct sockaddr *)&snRemote, sizeof(snRemote)) == SOCKET_ERROR) {
    DWORD err = GetLastError();

    if (err != WSAEWOULDBLOCK) {
      TraceError(err, "Connect(%x): connect()", hSock);
      closesocket(hSock);
      return FALSE;
    }
  }

  u_long addr = ntohl(snRemote.sin_addr.s_addr);
  u_short port  = ntohs(snRemote.sin_port);

  cout << "Connect(" << hex << hSock << dec << ", "
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
SOCKET Accept(SOCKET hSockListen)
{
  struct sockaddr_in sn;
  int snlen = sizeof(sn);

  SOCKET hSock = accept(hSockListen, (struct sockaddr *)&sn, &snlen);

  if (hSock == INVALID_SOCKET) {
    DWORD err = GetLastError();

    if (err != WSAEWOULDBLOCK)
      TraceError(err, "Accept(%x): accept()", hSockListen);

    return INVALID_SOCKET;
  }

  u_long addr = ntohl(sn.sin_addr.s_addr);
  u_short port  = ntohs(sn.sin_port);

  cout << "Accept(" << hex << hSockListen << dec << ") = " << hex << hSock << dec
       << " from "
       << ((addr >> 24) & 0xFF) << '.'
       << ((addr >> 16) & 0xFF) << '.'
       << ((addr >>  8) & 0xFF) << '.'
       << ( addr        & 0xFF) << ':'
       << port
       << endl;

  return hSock;
}
///////////////////////////////////////////////////////////////
void Disconnect(SOCKET hSock)
{
  if (shutdown(hSock, SD_BOTH) != 0)
    TraceError(GetLastError(), "Disconnect(%x): shutdown()", hSock);
  else
    cout << "Disconnect(" << hex << hSock << dec << ") - OK" << endl;
}
///////////////////////////////////////////////////////////////
WriteOverlapped::WriteOverlapped(ComPort &_port, BYTE *_pBuf, DWORD _len)
  : port(_port),
    pBuf(_pBuf),
    len(_len)
{
}

WriteOverlapped::~WriteOverlapped()
{
  pBufFree(pBuf);
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

  if (!::WriteFileEx(port.Handle(), pBuf, len, this, OnWrite)) {
    TraceError(GetLastError(), "WriteOverlapped::StartWrite(): WriteFileEx(%x) %s", port.Handle(), port.Name().c_str());
    return FALSE;
  }

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
  if (hSock != INVALID_SOCKET) {
    if (closesocket(hSock) != 0) {
      TraceError(
          GetLastError(),
          "WaitEventOverlapped::~WaitEventOverlapped(): closesocket(%x) %s",
          hSock,
          port.Name().c_str());
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
            "WaitEventOverlapped::~WaitEventOverlapped(): UnregisterWait() %s",
            port.Name().c_str());
      }
    }
  }

  if (hEvent) {
    if (!::WSACloseEvent(hEvent)) {
      TraceError(
          GetLastError(),
          "WaitEventOverlapped::~WaitEventOverlapped(): CloseHandle(hEvent) %s",
          port.Name().c_str());
    }
  }

  SafeDelete::Delete();
}

VOID CALLBACK WaitEventOverlapped::OnEvent(
    PVOID pOverlapped,
    BOOLEAN /*timerOrWaitFired*/)
{
  if (::QueueUserAPC(OnEvent, hThread, (ULONG_PTR)pOverlapped))
    ((WaitEventOverlapped *)pOverlapped)->LockDelete();
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
    }

    if (!pOver->port.OnEvent(pOver, FD_CONNECT, events.iErrorCode[FD_CONNECT_BIT]))
      return;
  }

  if ((events.lNetworkEvents & FD_CLOSE) != 0) {
    if (!pOver->port.OnEvent(pOver, FD_CLOSE, events.iErrorCode[FD_CLOSE_BIT]))
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
  if (::QueueUserAPC(OnEvent, hThread, (ULONG_PTR)pOverlapped))
    ((ListenOverlapped *)pOverlapped)->LockDelete();
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
