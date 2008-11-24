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
 * Revision 1.6  2008/11/24 12:37:00  vfrolov
 * Changed plugin API
 *
 * Revision 1.5  2008/11/17 16:44:57  vfrolov
 * Fixed race conditions
 *
 * Revision 1.4  2008/11/13 07:41:09  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.3  2008/10/06 12:15:14  vfrolov
 * Added --reconnect option
 *
 * Revision 1.2  2008/03/28 16:00:19  vfrolov
 * Added connectionCounter
 *
 * Revision 1.1  2008/03/27 17:18:27  vfrolov
 * Initial revision
 *
 */

#ifndef _COMPORT_H
#define _COMPORT_H

///////////////////////////////////////////////////////////////
class ComParams;
class WriteOverlapped;
class ReadOverlapped;
class WaitEventOverlapped;
class ListenOverlapped;
class ComPort;
///////////////////////////////////////////////////////////////
class Listener : public queue<ComPort *>
{
  public:
    Listener(const struct sockaddr_in &_snLocal);

    BOOL IsEqual(const struct sockaddr_in &_snLocal) {
      return memcmp(&snLocal, &_snLocal, sizeof(snLocal)) == 0;
    }

    BOOL Start();
    BOOL OnEvent(ListenOverlapped *pOverlapped, long e, int err);
    SOCKET Accept();

  private:
    struct sockaddr_in snLocal;
    SOCKET hSockListen;
};
///////////////////////////////////////////////////////////////
struct Buf {
  Buf(BYTE *_pBuf, DWORD _len) : pBuf(_pBuf), len(_len) {}

  BYTE *pBuf;
  DWORD len;
};

typedef vector<Buf> Bufs;
///////////////////////////////////////////////////////////////
class ComPort
{
  public:
    ComPort(
      vector<Listener *> &listeners,
      const ComParams &comParams,
      const char *pPath);

    BOOL Init(HMASTERPORT _hMasterPort);
    BOOL Start();
    BOOL Write(HUB_MSG *pMsg);
    void OnWrite(WriteOverlapped *pOverlapped, DWORD len, DWORD done);
    void OnRead(ReadOverlapped *pOverlapped, BYTE *pBuf, DWORD done);
    BOOL OnEvent(WaitEventOverlapped *pOverlapped, long e);
    void LostReport();
    void Accept();

    const string &Name() const { return name; }
    void Name(const char *pName) { name = pName; }
    HANDLE Handle() const { return (HANDLE)hSock; }
    BOOL CanConnect() const { return (permanent || connectionCounter > 0); }
    void StartConnect();

  private:
    BOOL StartRead();
    BOOL StartWaitEvent(SOCKET hSockWait);
    void OnConnect();
    void OnDisconnect();

    struct sockaddr_in snLocal;
    struct sockaddr_in snRemote;
    Listener *pListener;

    BOOL isValid;

    SOCKET hSock;
    BOOL isConnected;
    BOOL isDisconnected;
    int connectionCounter;
    BOOL permanent;

    int reconnectTime;
    HANDLE hReconnectTimer;

    string name;
    HMASTERPORT hMasterPort;

    int countReadOverlapped;
    int countXoff;

    DWORD writeQueueLimit;
    DWORD writeQueued;
    DWORD writeLost;
    DWORD writeLostTotal;

    Bufs bufs;
};
///////////////////////////////////////////////////////////////

#endif  // _COMPORT_H
