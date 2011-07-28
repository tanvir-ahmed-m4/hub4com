/*
 * $Id$
 *
 * Copyright (c) 2005-2011 Vyacheslav Frolov
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
 * Revision 1.6  2011/07/28 13:42:20  vfrolov
 * Implemented --ascii-cr-padding option
 *
 * Revision 1.5  2009/01/26 15:07:52  vfrolov
 * Implemented --keep-active option
 *
 * Revision 1.4  2008/11/13 07:44:12  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.3  2008/10/24 08:29:01  vfrolov
 * Implemented RFC 2217
 *
 * Revision 1.2  2008/10/09 11:02:58  vfrolov
 * Redesigned class TelnetProtocol
 *
 * Revision 1.1  2008/03/28 16:05:15  vfrolov
 * Initial revision
 *
 */

#ifndef _TELNET_H
#define _TELNET_H

///////////////////////////////////////////////////////////////
typedef vector<BYTE> BYTE_vector;
typedef basic_string<BYTE> BYTE_string;
///////////////////////////////////////////////////////////////
class TelnetOption;
///////////////////////////////////////////////////////////////
class TelnetProtocol
{
  public:
    TelnetProtocol(const char *pName);
    ~TelnetProtocol();

    void Start();
    void SetAsciiCrPadding(const BYTE_string &padding) { ascii_cr_padding = padding; }

    HUB_MSG *Decode(HUB_MSG *pMsg);
    void FlushEncodedStream(HUB_MSG **ppEchoMsg);
    HUB_MSG *Encode(HUB_MSG *pMsg);
    void KeepActive();

  public:
    enum {
      cdSE   = 240,
      cdNOP  = 241,
      cdSB   = 250,
      cdWILL = 251,
      cdWONT = 252,
      cdDO   = 253,
      cdDONT = 254,
      cdIAC  = 255,
    };

  protected:
    friend class TelnetOption;

    void SetOption(TelnetOption &telnetOption);

    void SendOption(BYTE code, BYTE option);
    void SendSubNegotiation(BYTE option, const BYTE_vector &params);

    static HUB_MSG *Flush(HUB_MSG *pMsg, BYTE_string &stream);

    HUB_MSG *FlushEncodedStream(HUB_MSG *pMsg) { return Flush(pMsg, streamEncoded); }
    HUB_MSG *FlushDecodedStream(HUB_MSG *pMsg) { return Flush(pMsg, streamDecoded); }

    string name;

    int state;
    BYTE code;
    BYTE option;
    BYTE_vector params;

    TelnetOption *options[BYTE(-1)];

    BYTE_string ascii_cr_padding;

    BYTE_string streamEncoded;
    BYTE_string streamDecoded;

#ifdef _DEBUG
  private:
    BOOL started;
#endif  /* _DEBUG */
};
///////////////////////////////////////////////////////////////
class TelnetOption
{
  public:
    TelnetOption(TelnetProtocol &_telnet, BYTE _option)
    : telnet(_telnet),
      option(_option),
      stateLocal(osCant),
      stateRemote(osCant)
    {
      telnet.SetOption(*this);
    }

    void LocalWill() { _ASSERTE(telnet.started != TRUE); stateLocal  = osYes; }
    void LocalCan()  { _ASSERTE(telnet.started != TRUE); stateLocal  = osNo; }
    void RemoteDo()  { _ASSERTE(telnet.started != TRUE); stateRemote = osYes; }
    void RemoteMay() { _ASSERTE(telnet.started != TRUE); stateRemote = osNo; }

  protected:
    friend class TelnetProtocol;

    virtual void Start();

    void SendOption(BYTE code) {
      telnet.SendOption(code, option);
    }

    void SendSubNegotiation(const BYTE_vector &params) {
      telnet.SendSubNegotiation(option, params);
    }

    HUB_MSG *FlushDecodedStream(HUB_MSG *pMsg) {
      return telnet.FlushDecodedStream(pMsg);
    }

    virtual BOOL OnSubNegotiation(const BYTE_vector &/*params*/, HUB_MSG ** /*ppMsg*/) {
      return FALSE;
    }

    TelnetProtocol &telnet;
    BYTE option;

    enum {osCant, osNo, osYes};

    int stateLocal;
    int stateRemote;
};
///////////////////////////////////////////////////////////////

#endif  // _TELNET_H
