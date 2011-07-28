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
 * Revision 1.7  2011/07/28 13:42:20  vfrolov
 * Implemented --ascii-cr-padding option
 *
 * Revision 1.6  2009/01/26 15:07:52  vfrolov
 * Implemented --keep-active option
 *
 * Revision 1.5  2008/11/13 07:44:12  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.4  2008/10/24 08:29:01  vfrolov
 * Implemented RFC 2217
 *
 * Revision 1.3  2008/10/16 09:24:23  vfrolov
 * Changed return type of ROUTINE_MSG_REPLACE_*() to BOOL
 *
 * Revision 1.2  2008/10/09 11:02:58  vfrolov
 * Redesigned class TelnetProtocol
 *
 * Revision 1.1  2008/03/28 16:05:15  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "../plugins_api.h"
///////////////////////////////////////////////////////////////
namespace FilterTelnet {
///////////////////////////////////////////////////////////////
#include "import.h"
#include "telnet.h"
///////////////////////////////////////////////////////////////
static const char *code2name(unsigned code)
{
  switch (code) {
    case TelnetProtocol::cdSE:
      return "SE";
      break;
    case TelnetProtocol::cdNOP:
      return "NOP";
      break;
    case TelnetProtocol::cdSB:
      return "SB";
      break;
    case TelnetProtocol::cdWILL:
      return "WILL";
      break;
    case TelnetProtocol::cdWONT:
      return "WONT";
      break;
    case TelnetProtocol::cdDO:
      return "DO";
      break;
    case TelnetProtocol::cdDONT:
      return "DONT";
      break;
  }
  return "UNKNOWN";
}
///////////////////////////////////////////////////////////////
enum {
  stData,
  stCode,
  stOption,
  stSubParams,
  stSubCode,
};
///////////////////////////////////////////////////////////////
TelnetProtocol::TelnetProtocol(const char *pName)
  : name(pName ? pName : "NONAME")
{
  cout << name << " START" << endl;

  state = stData;

  for(int i = 0 ; i < BYTE(-1) ; i++)
    options[i] = NULL;
}

TelnetProtocol::~TelnetProtocol()
{
  cout << name << " STOP" << endl;

  for(int i = 0 ; i < BYTE(-1) ; i++) {
    if (options[i])
      delete options[i];
  }
}

void TelnetProtocol::Start()
{
  _ASSERTE(started != TRUE);

  for(int i = 0 ; i < BYTE(-1) ; i++) {
    if (options[i])
      options[i]->Start();
  }

#ifdef _DEBUG
  started = TRUE;
#endif  /* _DEBUG */
}

void TelnetProtocol::SetOption(TelnetOption &telnetOption)
{
  _ASSERTE(started != TRUE);
  _ASSERTE(options[telnetOption.option] == NULL);
  options[telnetOption.option] = &telnetOption;
}

HUB_MSG *TelnetProtocol::Flush(HUB_MSG *pMsg, BYTE_string &stream)
{
  if (!stream.empty()) {
    pMsg = pMsgInsertBuf(pMsg,
                         HUB_MSG_TYPE_LINE_DATA,
                         stream.data(),
                         (DWORD)stream.size());

    stream.clear();
  }

  return pMsg;
}

HUB_MSG *TelnetProtocol::Encode(HUB_MSG *pMsg)
{
  _ASSERTE(started == TRUE);
  _ASSERTE(pMsg->type == HUB_MSG_TYPE_LINE_DATA);

  DWORD len = pMsg->u.buf.size;
  BYTE_string org(pMsg->u.buf.pBuf, len);
  const BYTE *pBuf = org.data();

  // discard original data from the stream
  if (!pMsgReplaceBuf(pMsg, HUB_MSG_TYPE_LINE_DATA, NULL, 0))
    return NULL;

  for (; len ; len--) {
    BYTE ch = *pBuf++;

    if (ch == cdIAC)
      streamEncoded += ch;

    streamEncoded += ch;

    if (ch == 13 /*CR*/ && ascii_cr_padding.size()) {
      if (options[0 /*TRANSMIT-BINARY*/] == NULL || options[0 /*TRANSMIT-BINARY*/]->stateLocal != TelnetOption::osYes)
        streamEncoded += ascii_cr_padding;
    }
  }

  return FlushEncodedStream(pMsg);
}

void TelnetProtocol::KeepActive()
{
  cout << name << " SEND: NOP" << endl;

  streamEncoded += (BYTE)cdIAC;
  streamEncoded += (BYTE)cdNOP;
}

HUB_MSG *TelnetProtocol::Decode(HUB_MSG *pMsg)
{
  _ASSERTE(started == TRUE);
  _ASSERTE(pMsg->type == HUB_MSG_TYPE_LINE_DATA);

  DWORD len = pMsg->u.buf.size;
  BYTE_string org(pMsg->u.buf.pBuf, len);
  const BYTE *pBuf = org.data();

  // discard original data from the stream
  if (!pMsgReplaceBuf(pMsg, HUB_MSG_TYPE_LINE_DATA, NULL, 0))
    return NULL;

  for (; len ; len--) {
    BYTE ch = *pBuf++;

    switch (state) {
      case stData:
        if (ch == cdIAC)
          state = stCode;
        else
          streamDecoded += ch;
        break;
      case stCode:
        switch (ch) {
          case cdIAC:
            streamDecoded += ch;
            state = stData;
            break;
          case cdNOP:
            cout << name << " RECV: " << code2name(ch) << endl;
            state = stData;
            break;
          case cdSB:
          case cdWILL:
          case cdWONT:
          case cdDO:
          case cdDONT:
            code = ch;
            state = stOption;
            break;
          default:
            cout << name << " RECV: unknown code " << (unsigned)ch << endl;
            state = stData;
        }
        break;
      case stOption:
        cout << name << " RECV: " << code2name(code) << " " << (unsigned)ch << endl;
        switch (code) {
          case cdSB:
            option = ch;
            params.clear();
            state = stSubParams;
            break;
          case cdWILL:
            if (options[ch] == NULL || options[ch]->stateRemote == TelnetOption::osCant) {
              SendOption(cdDONT, ch);
            }
            else
            if (options[ch]->stateRemote == TelnetOption::osNo) {
              options[ch]->stateRemote = TelnetOption::osYes;
              SendOption(cdDO, ch);
            }
            break;
          case cdWONT:
            if (options[ch] != NULL && options[ch]->stateRemote == TelnetOption::osYes) {
              options[ch]->stateRemote = TelnetOption::osNo;
              SendOption(cdDONT, ch);
            }
            break;
          case cdDO:
            if (options[ch] == NULL || options[ch]->stateLocal == TelnetOption::osCant) {
              SendOption(cdWONT, ch);
            }
            else
            if (options[ch]->stateLocal == TelnetOption::osNo) {
              options[ch]->stateLocal = TelnetOption::osYes;
              SendOption(cdWILL, ch);
            }
            break;
          case cdDONT:
            if (options[ch] != NULL && options[ch]->stateLocal == TelnetOption::osYes) {
              options[ch]->stateLocal = TelnetOption::osNo;
              SendOption(cdWONT, ch);
            }
            break;
          default:
            cout << "  ignored" << endl;
        };
        if (state == stOption)
          state = stData;
        break;
      case stSubParams:
        if (ch == cdIAC)
          state = stSubCode;
        else
          params.push_back(ch);
        break;
      case stSubCode:
        switch (ch) {
          case cdIAC:
            params.push_back(ch);
            state = stSubParams;
            break;
          case cdSE:
            cout << "  ";
            {
              for (BYTE_vector::const_iterator i = params.begin() ; i != params.end() ; i++)
                cout << (unsigned)*i << " ";
            }
            cout << "SE" << endl;

            if (!options[option] || !options[option]->OnSubNegotiation(params, &pMsg))
              cout << "  ignored" << endl;

            if (!pMsg)
              return NULL;

            state = stData;
            break;
          default:
            cout << name << " RECV: unknown sub code " << (unsigned)ch << endl;
            state = stData;
        };
        break;
    }
  }

  return FlushDecodedStream(pMsg);
}

void TelnetProtocol::FlushEncodedStream(HUB_MSG **ppEchoMsg)
{
  _ASSERTE(started == TRUE);

  if (*ppEchoMsg)
    FlushEncodedStream(*ppEchoMsg);
  else
    *ppEchoMsg = FlushEncodedStream((HUB_MSG *)NULL);
}

void TelnetProtocol::SendOption(BYTE code, BYTE option)
{
  cout << name << " SEND: " << code2name(code) << " " << (unsigned)option << endl;

  streamEncoded += (BYTE)cdIAC;
  streamEncoded += code;
  streamEncoded += option;
}

void TelnetProtocol::SendSubNegotiation(BYTE option, const BYTE_vector &params)
{
  SendOption(cdSB, option);

  cout << "  ";
  for (BYTE_vector::const_iterator i = params.begin() ; i != params.end() ; i++) {
    BYTE b = *i;

    cout << (unsigned)b << " ";
    streamEncoded += b;

    if (b == cdIAC)
      streamEncoded += b;
  }
  cout << "SE" << endl;

  streamEncoded += (BYTE)cdIAC;
  streamEncoded += (BYTE)cdSE;
}
///////////////////////////////////////////////////////////////
void TelnetOption::Start()
{
  if (stateLocal == osYes)
    SendOption(TelnetProtocol::cdWILL);

  if (stateRemote == osYes)
    SendOption(TelnetProtocol::cdDO);
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
