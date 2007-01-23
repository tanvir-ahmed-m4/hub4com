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

#ifndef _COMPARAMS_H
#define _COMPARAMS_H

///////////////////////////////////////////////////////////////
class ComParams
{
  public:
    ComParams();

    void SetBaudRate(const char *pBaudRate) { baudRate = atol(pBaudRate); }
    void SetByteSize(const char *pByteSize) { byteSize = atoi(pByteSize); }
    BOOL SetParity(const char *pParity);
    BOOL SetStopBits(const char *pStopBits);
 
    static const char *ParityStr(int parity);
    static const char *StopBitsStr(int stopBits);

    static const char *BaudRateLst();
    static const char *ByteSizeLst();
    static const char *ParityLst();
    static const char *StopBitsLst();

    long BaudRate() const { return baudRate; }
    int ByteSize() const { return byteSize; }
    int Parity() const { return parity; }
    int StopBits() const { return stopBits; }

  private:
    long baudRate;
    int byteSize;
    int parity;
    int stopBits;
};
///////////////////////////////////////////////////////////////

#endif  // _COMPARAMS_H
