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

#include "precomp.h"
#include "telnet.h"

///////////////////////////////////////////////////////////////
enum {
  cdSE   = 240,
  cdSB   = 250,
  cdWILL = 251,
  cdWONT = 252,
  cdDO   = 253,
  cdDONT = 254,
  cdIAC  = 255,
};

static const char *code2name(unsigned code)
{
  switch (code) {
    case cdSE:
      return "SE";
      break;
    case cdSB:
      return "SB";
      break;
    case cdWILL:
      return "WILL";
      break;
    case cdWONT:
      return "WONT";
      break;
    case cdDO:
      return "DO";
      break;
    case cdDONT:
      return "DONT";
      break;
  }
  return "UNKNOWN";
}
///////////////////////////////////////////////////////////////
enum {
  opEcho            = 1,
  opTerminalType    = 24,
};
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
  SetTerminalType(NULL);
  Clear();
}

void TelnetProtocol::SetTerminalType(const char *pTerminalType)
{
  terminalType.clear();

  if (!pTerminalType)
    pTerminalType = "UNKNOWN";

  while (*pTerminalType)
    terminalType.push_back(*pTerminalType++);
}

void TelnetProtocol::Clear()
{
  cout << name << " RESET" << endl;

  state = stData;

  for(int i = 0 ; i < sizeof(options)/sizeof(options[0]) ; i++) {
    options[i].remoteOptionState = OptionState::osCant;
    options[i].localOptionState = OptionState::osCant;
  }

  options[opEcho].remoteOptionState = OptionState::osNo;
  options[opTerminalType].localOptionState = OptionState::osNo;

  streamSendRead.clear();
  streamWriteRecv.clear();
}

void TelnetProtocol::Send(const BYTE *pBuf, DWORD count)
{
  for (DWORD i = 0 ; i < count ; i++) {
    BYTE ch = ((const BYTE *)pBuf)[i];

    if (ch == cdIAC)
          SendRaw(&ch, 1);

    SendRaw(&ch, 1);
  }
}

void TelnetProtocol::Write(const BYTE *pBuf, DWORD count)
{
  for (DWORD i = 0 ; i < count ; i++) {
    BYTE ch = ((const BYTE *)pBuf)[i];

    switch (state) {
      case stData:
        if (ch == cdIAC)
          state = stCode;
        else
          WriteRaw(&ch, 1);
        break;
      case stCode:
        switch (ch) {
          case cdIAC:
            WriteRaw(&ch, 1);
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
            switch (options[ch].remoteOptionState) {
              case OptionState::osCant:
                SendOption(cdDONT, ch);
                break;
              case OptionState::osNo:
                options[ch].remoteOptionState = OptionState::osYes;
                SendOption(cdDO, ch);
                break;
              case OptionState::osYes:
                break;
            }
            break;
          case cdWONT:
            switch (options[ch].remoteOptionState) {
              case OptionState::osCant:
              case OptionState::osNo:
                break;
              case OptionState::osYes:
                options[ch].remoteOptionState = OptionState::osNo;
                SendOption(cdDONT, ch);
                break;
            }
            break;
          case cdDO:
            switch (options[ch].localOptionState) {
              case OptionState::osCant:
                SendOption(cdWONT, ch);
                break;
              case OptionState::osNo:
                options[ch].localOptionState = OptionState::osYes;
                SendOption(cdWILL, ch);
                break;
              case OptionState::osYes:
                break;
            }
            break;
          case cdDONT:
            switch (options[ch].localOptionState) {
              case OptionState::osCant:
              case OptionState::osNo:
                break;
              case OptionState::osYes:
                options[ch].localOptionState = OptionState::osNo;
                SendOption(cdWONT, ch);
                break;
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
            state = stSubParams;
            break;
          case cdSE:
            cout << "  ";
            {
              for (BYTE_vector::const_iterator i = params.begin() ; i != params.end() ; i++)
                cout << (unsigned)*i << " ";
            }
            cout << "SE" << endl;

            switch (option) {
              case opTerminalType:
                params.clear();
                params.push_back(0);
                params.insert(params.end(), terminalType.begin(), terminalType.end());
                SendSubNegotiation(option, params);
                break;
              default:
                cout << "  ignored" << endl;
            }

            state = stData;
            break;
          default:
            cout << name << " RECV: unknown sub code " << (unsigned)ch << endl;
            state = stData;
        };
        break;
    }
  }
}

void TelnetProtocol::SendOption(BYTE code, BYTE option)
{
  BYTE buf[3] = {cdIAC, code, option};

  cout << name << " SEND: " << code2name(code) << " " << (unsigned)option << endl;

  SendRaw(buf, sizeof(buf));
}

void TelnetProtocol::SendSubNegotiation(int option, const BYTE_vector &params)
{
  SendOption(cdSB, (BYTE)option);

  cout << "  ";
  for (BYTE_vector::const_iterator i = params.begin() ; i != params.end() ; i++) {
    BYTE b = *i;

    cout << (unsigned)b << " ";
    SendRaw(&b, sizeof(b));
  }
  cout << "SE" << endl;

  BYTE buf[2] = {cdIAC, cdSE};

  SendRaw(buf, sizeof(buf));
}
///////////////////////////////////////////////////////////////
