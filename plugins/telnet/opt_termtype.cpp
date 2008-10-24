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
 * Revision 1.1  2008/10/24 06:51:23  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "opt_termtype.h"

///////////////////////////////////////////////////////////////
enum {
  ttIs = 0,
  ttSend = 1,
};

TelnetOptionTerminalType::TelnetOptionTerminalType(
    TelnetProtocol &_telnet,
    const char *pTerminalType)
  : TelnetOption(_telnet, 24 /*TERMINAL-TYPE*/)
{
  if (!pTerminalType)
    pTerminalType = "UNKNOWN";

  while (*pTerminalType)
    terminalType.push_back(*pTerminalType++);
}

BOOL TelnetOptionTerminalType::OnSubNegotiation(const BYTE_vector &params, HUB_MSG ** /*ppMsg*/)
{
  if (params.empty())
    return FALSE;

  switch (params[0]) {
    case ttIs:
      return FALSE;
    case ttSend: {
      BYTE_vector answer;

      answer.push_back(ttIs);
      answer.insert(answer.end(), terminalType.begin(), terminalType.end());

      SendSubNegotiation(answer);
      break;
    }
    default:
      return FALSE;
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
