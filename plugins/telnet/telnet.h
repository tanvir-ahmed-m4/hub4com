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
 * Revision 1.2  2008/10/09 11:02:58  vfrolov
 * Redesigned class TelnetProtocol
 *
 * Revision 1.1  2008/03/28 16:05:15  vfrolov
 * Initial revision
 *
 */

#ifndef _TELNET_H
#define _TELNET_H

#include "../plugins_api.h"

///////////////////////////////////////////////////////////////
typedef vector<BYTE> BYTE_vector;
typedef basic_string<BYTE> BYTE_string;
///////////////////////////////////////////////////////////////
class TelnetProtocol
{
  public:
    TelnetProtocol(const char *pName);
    void SetTerminalType(const char *pTerminalType);

    HUB_MSG *Decode(HUB_MSG *pMsg);
    HUB_MSG *Encode(HUB_MSG *pMsg);
    HUB_MSG *FlushEncodedStream() { return FlushEncodedStream(NULL); }

    void Clear();
  protected:
    void SendOption(BYTE code, BYTE option);
    void SendSubNegotiation(int option, const BYTE_vector &params);

    static HUB_MSG *Flush(HUB_MSG *pMsg, BYTE_string &stream);

    HUB_MSG *FlushEncodedStream(HUB_MSG *pMsg) { return Flush(pMsg, streamEncoded); }
    HUB_MSG *FlushDecodedStream(HUB_MSG *pMsg) { return Flush(pMsg, streamDecoded); }

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

    BYTE_string streamEncoded;
    BYTE_string streamDecoded;
};
///////////////////////////////////////////////////////////////

#endif  // _TELNET_H
