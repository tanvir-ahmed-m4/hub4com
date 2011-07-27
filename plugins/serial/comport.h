/*
 * $Id$
 *
 * Copyright (c) 2006-2011 Vyacheslav Frolov
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
 * Revision 1.19  2011/07/27 17:08:33  vfrolov
 * Implemented serial port share mode
 *
 * Revision 1.18  2011/05/19 16:47:00  vfrolov
 * Fixed unexpected assertion
 * Added human readable printing output options set
 *
 * Revision 1.17  2008/12/18 16:50:52  vfrolov
 * Extended the number of possible IN options
 *
 * Revision 1.16  2008/12/17 11:52:35  vfrolov
 * Replaced ComIo::dcb by serialBaudRate, serialLineControl,
 * serialHandFlow and serialChars
 * Replaced ComPort::filterX by ComIo::FilterX()
 * Replaced SetManual*() by PinStateControlMask()
 *
 * Revision 1.15  2008/12/11 13:07:54  vfrolov
 * Added PURGE_TX
 *
 * Revision 1.14  2008/12/01 17:06:29  vfrolov
 * Improved write buffering
 *
 * Revision 1.13  2008/11/27 16:25:08  vfrolov
 * Improved write buffering
 *
 * Revision 1.12  2008/11/24 16:30:56  vfrolov
 * Removed pOnXoffXon
 *
 * Revision 1.11  2008/11/24 12:37:00  vfrolov
 * Changed plugin API
 *
 * Revision 1.10  2008/11/13 07:35:10  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.9  2008/09/30 08:28:32  vfrolov
 * Added ability to control OUT1 and OUT2 pins
 * Added ability to get remote baud rate and line control settings
 * Added ability to set baud rate and line control
 * Added fallback to non escape mode
 *
 * Revision 1.8  2008/08/22 16:57:12  vfrolov
 * Added
 *   HUB_MSG_TYPE_GET_ESC_OPTS
 *   HUB_MSG_TYPE_FAIL_ESC_OPTS
 *   HUB_MSG_TYPE_BREAK_STATUS
 *
 * Revision 1.7  2008/08/20 14:30:19  vfrolov
 * Redesigned serial port options
 *
 * Revision 1.6  2008/08/15 12:44:59  vfrolov
 * Added fake read filter method to ports
 *
 * Revision 1.5  2008/08/13 15:14:02  vfrolov
 * Print bit values in readable form
 *
 * Revision 1.4  2008/08/11 07:15:34  vfrolov
 * Replaced
 *   HUB_MSG_TYPE_COM_FUNCTION
 *   HUB_MSG_TYPE_INIT_LSR_MASK
 *   HUB_MSG_TYPE_INIT_MST_MASK
 * by
 *   HUB_MSG_TYPE_SET_PIN_STATE
 *   HUB_MSG_TYPE_GET_OPTIONS
 *   HUB_MSG_TYPE_SET_OPTIONS
 *
 * Revision 1.3  2008/04/11 14:48:42  vfrolov
 * Replaced SET_RT_EVENTS by INIT_LSR_MASK and INIT_MST_MASK
 * Replaced COM_ERRORS by LINE_STATUS
 *
 * Revision 1.2  2008/04/07 12:28:03  vfrolov
 * Replaced --rt-events option by SET_RT_EVENTS message
 *
 * Revision 1.1  2008/03/26 08:44:34  vfrolov
 * Redesigned for using plugins
 *
 * Revision 1.3  2007/02/06 11:53:33  vfrolov
 * Added options --odsr, --ox, --ix and --idsr
 * Added communications error reporting
 *
 * Revision 1.2  2007/02/05 09:33:20  vfrolov
 * Implemented internal flow control
 *
 * Revision 1.1  2007/01/23 09:13:10  vfrolov
 * Initial revision
 *
 */

#ifndef _COMPORT_H
#define _COMPORT_H

///////////////////////////////////////////////////////////////
class ComParams;
class WriteOverlapped;
class ReadOverlapped;
class WaitCommEventOverlapped;
class ComIo;
///////////////////////////////////////////////////////////////
class ComPort
{
  public:
    ComPort(
      const ComParams &comParams,
      const char *pPath);

    BOOL Init(HMASTERPORT _hMasterPort);
    BOOL Start();
    BOOL FakeReadFilter(HUB_MSG *pInMsg);
    BOOL Write(HUB_MSG *pMsg);

    void OnWrite(WriteOverlapped *pOverlapped, DWORD len, DWORD done);
    void OnRead(ReadOverlapped *pOverlapped, BYTE *pBuf, DWORD done);
    void OnCommEvent(WaitCommEventOverlapped *pOverlapped, DWORD eMask);
    void OnPortFree() { Update(); }

    void LostReport();

    const string &Name() const { return name; }
    void Name(const char *pName) { name = pName; }

  private:
    void FlowControlUpdate();
    void PurgeWrite(BOOL withLost);
    void FilterX(BYTE *pBuf, DWORD &len);
    void UpdateOutOptions(DWORD options);
    void StartDisconnect();
    void Update();
    BOOL Start(BOOL first);
    BOOL StartRead();
    BOOL StartWaitCommEvent();
    void CheckComEvents(DWORD eMask);

    ComIo *pComIo;
    string name;
    HMASTERPORT hMasterPort;

    int countReadOverlapped;
    int countWaitCommEventOverlapped;
    int countXoff;

    DWORD intercepted_options[2];
    DWORD inOptions[2];
    DWORD escapeOptions;
    DWORD outOptions;
#ifdef _DEBUG
    DWORD outOptionsRequested;
#endif

    int connectionCounter;
    BOOL connectSent;
    BOOL permanent;

    DWORD writeQueueLimit;
    DWORD writeQueueLimitSendXoff;
    DWORD writeQueueLimitSendXon;
    DWORD writeQueued;
    BOOL writeSuspended;
    DWORD writeLost;
    DWORD writeLostTotal;
    DWORD errors;

    queue<WriteOverlapped *> writeOverlappedBuf;
    BYTE *pWriteBuf;
    DWORD lenWriteBuf;

#ifdef _DEBUG
  private:
    ComPort(const ComPort &) {}
    ~ComPort() {}
    void operator=(const ComPort &) {}
#endif  /* _DEBUG */
};
///////////////////////////////////////////////////////////////

#endif  // _COMPORT_H
