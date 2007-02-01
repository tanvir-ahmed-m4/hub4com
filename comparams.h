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
 * Revision 1.2  2007/02/01 12:14:59  vfrolov
 * Redesigned COM port params
 *
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

    BOOL SetBaudRate(const char *pBaudRate);
    BOOL SetByteSize(const char *pByteSize);
    BOOL SetParity(const char *pParity);
    BOOL SetStopBits(const char *pStopBits);
    BOOL SetOutCts(const char *pOutCts);
    BOOL SetOutDsr(const char *pOutDsr);
 
    static string BaudRateStr(long baudRate);
    static string ByteSizeStr(int byteSize);
    static string ParityStr(int parity);
    static string StopBitsStr(int stopBits);
    static string OutCtsStr(int outCts);
    static string OutDsrStr(int outDsr);

    string BaudRateStr() const { return BaudRateStr(baudRate); }
    string ByteSizeStr() const { return ByteSizeStr(byteSize); }
    string ParityStr() const { return ParityStr(parity); }
    string StopBitsStr() const { return StopBitsStr(stopBits); }
    string OutCtsStr() const { return OutCtsStr(outCts); }
    string OutDsrStr() const { return OutDsrStr(outDsr); }

    static const char *BaudRateLst();
    static const char *ByteSizeLst();
    static const char *ParityLst();
    static const char *StopBitsLst();
    static const char *OutCtsLst();
    static const char *OutDsrLst();

    long BaudRate() const { return baudRate; }
    int ByteSize() const { return byteSize; }
    int Parity() const { return parity; }
    int StopBits() const { return stopBits; }
    int OutCts() const { return outCts; }
    int OutDsr() const { return outDsr; }

  private:
    long baudRate;
    int byteSize;
    int parity;
    int stopBits;
    int outCts;
    int outDsr;
};
///////////////////////////////////////////////////////////////

#endif  // _COMPARAMS_H
