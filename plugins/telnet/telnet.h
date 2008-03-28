/*
 * $Id$
 *
 * Copyright (c) 2005-2008 Vyacheslav Frolov
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
 * Revision 1.1  2008/03/28 16:05:15  vfrolov
 * Initial revision
 *
 *
 */

#ifndef _TELNET_H
#define _TELNET_H

///////////////////////////////////////////////////////////////
typedef vector<BYTE> BYTE_vector;
typedef basic_string<BYTE> BYTE_string;
///////////////////////////////////////////////////////////////
class TelnetProtocol
{
  public:
    TelnetProtocol(const char *pName);
    void SetTerminalType(const char *pTerminalType);

    void Write(const BYTE *pBuf, DWORD count);
    void Send(const BYTE *pBuf, DWORD count);

    DWORD ReadDataLength() const { return (DWORD)streamSendRead.size(); }
    const BYTE *ReadData() const { return streamSendRead.data(); }
    void ReadDataClear() { streamSendRead.clear(); }

    DWORD RecvDataLength() const { return (DWORD)streamWriteRecv.size(); }
    const BYTE *RecvData() const { return streamWriteRecv.data(); }
    void RecvDataClear() { streamWriteRecv.clear(); }

    void Clear();
  protected:
    void SendOption(BYTE code, BYTE option);
    void SendSubNegotiation(int option, const BYTE_vector &params);
    void SendRaw(const BYTE *pBuf, DWORD count) { streamSendRead.append(pBuf, count); }
    void WriteRaw(const BYTE *pBuf, DWORD count) { streamWriteRecv.append(pBuf, count); }

    string name;

    int state;
    int code;
    int option;
    BYTE_vector params;

    struct OptionState
    {
      enum {osCant, osNo, osYes};
      int localOptionState  : 2;
      int remoteOptionState : 2;
    };

    OptionState options[256];
    BYTE_vector terminalType;

    BYTE_string streamSendRead;
    BYTE_string streamWriteRecv;
};
///////////////////////////////////////////////////////////////

#endif  // _TELNET_H
