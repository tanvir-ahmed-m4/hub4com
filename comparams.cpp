/*
 * $Id$
 *
 * Copyright (c) 2006-2007 Vyacheslav Frolov
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
 * Revision 1.1  2007/01/23 09:13:10  vfrolov
 * Initial revision
 *
 *
 */

#include "precomp.h"
#include "comparams.h"

///////////////////////////////////////////////////////////////
ComParams::ComParams()
  : baudRate(CBR_19200),
    byteSize(8),
    parity(NOPARITY),
    stopBits(ONESTOPBIT)
{
}

BOOL ComParams::SetParity(const char *pParity)
{
  switch (*pParity) {
    case 'n': parity = NOPARITY; break;
    case 'o': parity = ODDPARITY; break;
    case 'e': parity = EVENPARITY; break;
    case 'm': parity = MARKPARITY; break;
    case 's': parity = SPACEPARITY; break;
    case 'd': parity = -1; break;
    default : return FALSE;
  }
  return TRUE;
}

BOOL ComParams::SetStopBits(const char *pStopBits)
{
  switch (*pStopBits) {
    case '1':
      if ((pStopBits[1] == '.' || pStopBits[1] == ',') && pStopBits[2] == '5')
        stopBits = ONE5STOPBITS;
      else
        stopBits = ONESTOPBIT;
      break;
    case '2': stopBits = TWOSTOPBITS; break;
    case 'd': stopBits = -1; break;
    default : return FALSE;
  }
  return TRUE;
}

const char *ComParams::ParityStr(int parity)
{
  switch (parity) {
    case NOPARITY: return "no";
    case ODDPARITY: return "odd";
    case EVENPARITY: return "even";
    case MARKPARITY: return "mark";
    case SPACEPARITY: return "space";
    case -1: return "default";
  }
  return "?";
}

const char *ComParams::StopBitsStr(int stopBits)
{
  switch (stopBits) {
    case ONESTOPBIT: return "1";
    case ONE5STOPBITS: return "1.5";
    case TWOSTOPBITS: return "2";
    case -1: return "default";
  }
  return "?";
}

const char *ComParams::BaudRateLst()
{
  return "a positive number or d[efault]";
}

const char *ComParams::ByteSizeLst()
{
  return "a positive number or d[efault]";
}

const char *ComParams::ParityLst()
{
  return "n[o], o[dd], e[ven], m[ark], s[pace] or d[efault]";
}

const char *ComParams::StopBitsLst()
{
  return "1, 1.5, 2 or d[efault]";
}
///////////////////////////////////////////////////////////////
