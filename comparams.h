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
 * Revision 1.4  2007/05/14 12:06:37  vfrolov
 * Added read interval timeout option
 *
 * Revision 1.3  2007/02/06 11:53:33  vfrolov
 * Added options --odsr, --ox, --ix and --idsr
 * Added communications error reporting
 *
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
    BOOL SetOutCts(const char *pOutCts) { return SetFlag(pOutCts, &outCts); }
    BOOL SetOutDsr(const char *pOutDsr) { return SetFlag(pOutDsr, &outDsr); }
    BOOL SetOutX(const char *pOutX) { return SetFlag(pOutX, &outX); }
    BOOL SetInX(const char *pInX) { return SetFlag(pInX, &inX); }
    BOOL SetInDsr(const char *pInDsr) { return SetFlag(pInDsr, &inDsr); }
    BOOL SetIntervalTimeout(const char *pIntervalTimeout);
 
    static string BaudRateStr(long baudRate);
    static string ByteSizeStr(int byteSize);
    static string ParityStr(int parity);
    static string StopBitsStr(int stopBits);
    static string OutCtsStr(int outCts) { return FlagStr(outCts); }
    static string OutDsrStr(int outDsr) { return FlagStr(outDsr); }
    static string OutXStr(int outX) { return FlagStr(outX); }
    static string InXStr(int inX) { return FlagStr(inX); }
    static string InDsrStr(int inDsr) { return FlagStr(inDsr); }
    static string IntervalTimeoutStr(long intervalTimeout);

    string BaudRateStr() const { return BaudRateStr(baudRate); }
    string ByteSizeStr() const { return ByteSizeStr(byteSize); }
    string ParityStr() const { return ParityStr(parity); }
    string StopBitsStr() const { return StopBitsStr(stopBits); }
    string OutCtsStr() const { return OutCtsStr(outCts); }
    string OutDsrStr() const { return OutDsrStr(outDsr); }
    string OutXStr() const { return OutXStr(outX); }
    string InXStr() const { return InXStr(inX); }
    string InDsrStr() const { return InDsrStr(inDsr); }
    string IntervalTimeoutStr() const { return IntervalTimeoutStr(intervalTimeout); }

    static const char *BaudRateLst();
    static const char *ByteSizeLst();
    static const char *ParityLst();
    static const char *StopBitsLst();
    static const char *OutCtsLst() { return FlagLst(); }
    static const char *OutDsrLst() { return FlagLst(); }
    static const char *OutXLst() { return FlagLst(); }
    static const char *InXLst() { return FlagLst(); }
    static const char *InDsrLst() { return FlagLst(); }
    static const char *IntervalTimeoutLst();

    long BaudRate() const { return baudRate; }
    int ByteSize() const { return byteSize; }
    int Parity() const { return parity; }
    int StopBits() const { return stopBits; }
    int OutCts() const { return outCts; }
    int OutDsr() const { return outDsr; }
    int OutX() const { return outX; }
    int InX() const { return inX; }
    int InDsr() const { return inDsr; }
    long IntervalTimeout() const { return intervalTimeout; }

  private:
    BOOL SetFlag(const char *pFlagStr, int *pFlag);
    static string FlagStr(int flag);
    static const char *FlagLst();

    long baudRate;
    int byteSize;
    int parity;
    int stopBits;
    int outCts;
    int outDsr;
    int outX;
    int inX;
    int inDsr;
    long intervalTimeout;
};
///////////////////////////////////////////////////////////////

#endif  // _COMPARAMS_H
