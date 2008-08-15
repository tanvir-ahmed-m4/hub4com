/*
 * $Id$
 *
 * Copyright (c) 2006-2008 Vyacheslav Frolov
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
 * Revision 1.4  2007/04/16 07:33:38  vfrolov
 * Fixed LostReport()
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

#include "precomp.h"
#include "comport.h"
#include "comio.h"
#include "comparams.h"
#include "import.h"

///////////////////////////////////////////////////////////////
ComPort::ComPort(
    const ComParams &comParams,
    const char *pPath)
  : hMasterPort(NULL),
    hHub(NULL),
    countReadOverlapped(0),
    countWaitCommEventOverlapped(0),
    countXoff(0),
    filterX(FALSE),
    events(0),
    maskOutPins(0),
    options(0),
    writeQueueLimit(256),
    writeQueued(0),
    writeLost(0),
    writeLostTotal(0),
    errors(0)
{
  filterX = comParams.InX();
  string path(pPath);
  name = path.substr(path.rfind('\\') + 1);
  handle = ::OpenComPort(pPath, comParams);
}

BOOL ComPort::Init(HMASTERPORT _hMasterPort, HHUB _hHub)
{
  if (handle == INVALID_HANDLE_VALUE) {
    cerr << "ComPort::Init(): Invalid handle" << endl;
    return FALSE;
  }

  hMasterPort = _hMasterPort;
  hHub = _hHub;

  return TRUE;
}

struct FIELD2NAME {
  DWORD code;
  DWORD mask;
  const char *name;
};

static string FieldToName(const FIELD2NAME *pTable, DWORD mask, const char *pDelimiter = "|")
{
  stringstream str;
  int count = 0;

  if (pTable) {
    while (pTable->name) {
      DWORD m = (mask & pTable->mask);

      if (m == pTable->code) {
        mask &= ~pTable->mask;
        if (count)
          str << pDelimiter;
        str << pTable->name;
        count++;
      }
      pTable++;
    }
  }

  if (mask) {
    if (count)
      str << pDelimiter;
    str << "0x" << hex << (long)mask << dec;
  }

  return str.str();
}

#define TOFIELD2NAME2(p, s) { (ULONG)p##s, (ULONG)p##s, #s }

static FIELD2NAME codeNameTableModemStatus[] = {
  TOFIELD2NAME2(MODEM_STATUS_, DCTS),
  TOFIELD2NAME2(MODEM_STATUS_, DDSR),
  TOFIELD2NAME2(MODEM_STATUS_, TERI),
  TOFIELD2NAME2(MODEM_STATUS_, DDCD),
  TOFIELD2NAME2(MODEM_STATUS_, CTS),
  TOFIELD2NAME2(MODEM_STATUS_, DSR),
  TOFIELD2NAME2(MODEM_STATUS_, RI),
  TOFIELD2NAME2(MODEM_STATUS_, DCD),
  {0, 0, NULL}
};

static FIELD2NAME codeNameTableLineStatus[] = {
  TOFIELD2NAME2(LINE_STATUS_, DR),
  TOFIELD2NAME2(LINE_STATUS_, OE),
  TOFIELD2NAME2(LINE_STATUS_, PE),
  TOFIELD2NAME2(LINE_STATUS_, FE),
  TOFIELD2NAME2(LINE_STATUS_, BI),
  TOFIELD2NAME2(LINE_STATUS_, THRE),
  TOFIELD2NAME2(LINE_STATUS_, TEMT),
  TOFIELD2NAME2(LINE_STATUS_, FIFOERR),
  {0, 0, NULL}
};

static FIELD2NAME codeNameTableComEvents[] = {
  TOFIELD2NAME2(EV_, CTS),
  TOFIELD2NAME2(EV_, DSR),
  TOFIELD2NAME2(EV_, RLSD),
  TOFIELD2NAME2(EV_, RING),
  {0, 0, NULL}
};

BOOL ComPort::Start()
{
  //cout << name << " Start " << ::GetCurrentThreadId() << endl;

  _ASSERTE(hMasterPort != NULL);
  _ASSERTE(hHub != NULL);
  _ASSERTE(handle != INVALID_HANDLE_VALUE);

  BYTE optsLsr = GO_O2V_LINE_STATUS(options);
  BYTE optsMst = GO_O2V_MODEM_STATUS(options);

  if (optsLsr || optsMst) {
    if ((optsMst & MODEM_STATUS_CTS) != 0)
      events |= EV_CTS;

    if ((optsMst & MODEM_STATUS_DSR) != 0)
      events |= EV_DSR;

    if ((optsMst & MODEM_STATUS_DCD) != 0)
      events |= EV_RLSD;

    if ((optsMst & MODEM_STATUS_RI) != 0)
      events |= EV_RING;

    optsMst &= ~(MODEM_STATUS_CTS|MODEM_STATUS_DSR|MODEM_STATUS_DCD|MODEM_STATUS_RI);

    if (optsMst) {
      cout << name << " WARNING: Changing of MODEM STATUS bit(s) 0x"
           << hex << (unsigned)optsMst << dec << " ["
           << FieldToName(codeNameTableModemStatus, optsMst)
           << "] will be ignored" << endl;
    }

    if (optsLsr) {
      cout << name << " WARNING: Changing of LINE STATUS bit(s) 0x"
           << hex << (unsigned)optsLsr << dec << " ["
           << FieldToName(codeNameTableLineStatus, optsLsr)
           << "] will be ignored" << endl;
    }
  }

  if (events) {
    if (!SetComEvents(handle, &events))
      return FALSE;

    if (!StartWaitCommEvent())
      return FALSE;

    cout << name << " Event(s) 0x" << hex << events << dec << " ["
           << FieldToName(codeNameTableComEvents, events)
           << "] will be monitired" << endl;
  }

  CheckComEvents(DWORD(-1));

  if (!StartRead())
    return FALSE;

  HUB_MSG msg;

  msg.type = HUB_MSG_TYPE_CONNECT;
  msg.u.val = TRUE;
  pOnRead(hHub, hMasterPort, &msg);

  return TRUE;
}

BOOL ComPort::StartRead()
{
  if (countReadOverlapped)
    return TRUE;

  if (handle == INVALID_HANDLE_VALUE)
    return FALSE;

  ReadOverlapped *pOverlapped;

  pOverlapped = new ReadOverlapped(*this);

  if (!pOverlapped)
    return FALSE;

  if (!pOverlapped->StartRead()) {
    delete pOverlapped;
    return FALSE;
  }

  countReadOverlapped++;

  //cout << name << " Started Read " << countReadOverlapped << endl;

  return TRUE;
}

BOOL ComPort::FakeReadFilter(HUB_MSG *pInMsg)
{
  _ASSERTE(pInMsg != NULL);

  if (pInMsg->type == HUB_MSG_TYPE_GET_OPTIONS) {
    pInMsg->u.pv.val &= ~(GO_V2O_MODEM_STATUS(-1) | GO_V2O_LINE_STATUS(-1));

    pInMsg = pMsgInsertNone(pInMsg, HUB_MSG_TYPE_EMPTY);

    if (pInMsg) {
      pInMsg->type = HUB_MSG_TYPE_GET_OPTIONS;
      pInMsg->u.pv.pVal = &options;
      pInMsg->u.pv.val = GO_V2O_MODEM_STATUS(-1) | GO_V2O_LINE_STATUS(-1);
    }
  }

  return pInMsg != NULL;
}

static FIELD2NAME codeNameTableSetPinState[] = {
  TOFIELD2NAME2(PIN_STATE_, RTS),
  TOFIELD2NAME2(PIN_STATE_, DTR),
  TOFIELD2NAME2(PIN_STATE_, OUT1),
  TOFIELD2NAME2(PIN_STATE_, OUT2),
  TOFIELD2NAME2(PIN_STATE_, BREAK),
  {0, 0, NULL}
};

BOOL ComPort::Write(HUB_MSG *pMsg)
{
  _ASSERTE(pMsg != NULL);

  if (pMsg->type == HUB_MSG_TYPE_LINE_DATA) {
    BYTE *pBuf = pMsg->u.buf.pBuf;
    DWORD len = pMsg->u.buf.size;

    if (!len)
      return TRUE;

    if (!pBuf) {
      writeLost += len;
      return FALSE;
    }

    if (filterX) {
      BYTE *pSrc = pBuf;
      BYTE *pDst = pBuf;

      for (DWORD i = 0 ; i < len ; i++) {
        if (*pSrc == 0x11 || *pSrc == 0x13)
          pSrc++;
        else
          *pDst++ = *pSrc++;
      }

      len = DWORD(pDst - pBuf);
    }

    if (!len)
      return TRUE;

    if (handle == INVALID_HANDLE_VALUE) {
      writeLost += len;
      return FALSE;
    }

    WriteOverlapped *pOverlapped;

    pOverlapped = new WriteOverlapped(*this, pBuf, len);

    if (!pOverlapped) {
      writeLost += len;
      return FALSE;
    }

    pMsg->type = HUB_MSG_TYPE_EMPTY;

    if (writeQueued > writeQueueLimit)
      ::PurgeComm(handle, PURGE_TXABORT|PURGE_TXCLEAR);

    if (!pOverlapped->StartWrite()) {
      writeLost += len;
      delete pOverlapped;
      return FALSE;
    }

    if (writeQueued <= writeQueueLimit/2 && (writeQueued + len) > writeQueueLimit/2)
      pOnXoff(hHub, hMasterPort);

    writeQueued += len;

    //cout << name << " Started Write " << len << " " << writeQueued << endl;
  }
  else
  if (pMsg->type == HUB_MSG_TYPE_SET_PIN_STATE) {
    if (handle == INVALID_HANDLE_VALUE)
      return FALSE;

    WORD mask = SPS_MASK2PIN(pMsg->u.val);

    cout << name << " SET_PIN_STATE 0x" << hex << pMsg->u.val << dec << " SET["
         << FieldToName(codeNameTableSetPinState, pMsg->u.val & mask)
         << "] CLR["
         << FieldToName(codeNameTableSetPinState, ~pMsg->u.val & mask)
         << "]" << endl;

    mask &= maskOutPins;

    if (mask & PIN_STATE_RTS) {
      if (!CommFunction(handle, (pMsg->u.val & PIN_STATE_RTS) ? SETRTS : CLRRTS))
        cerr << name << " WARNING! can't change RTS state" << endl;
    }
    if (mask & PIN_STATE_DTR) {
      if (!CommFunction(handle, (pMsg->u.val & PIN_STATE_DTR) ? SETDTR : CLRDTR))
        cerr << name << " WARNING! can't change DTR state" << endl;
    }
    if (mask & PIN_STATE_OUT1) {
      cerr << name << " WARNING! can't change OUT1 state" << endl;
    }
    if (mask & PIN_STATE_OUT2) {
      cerr << name << " WARNING! can't change OUT2 state" << endl;
    }
    if (mask & PIN_STATE_BREAK) {
      if (!CommFunction(handle, (pMsg->u.val & PIN_STATE_BREAK) ? SETBREAK : CLRBREAK))
        cerr << name << " WARNING! can't change BREAK state" << endl;
    }
  }
  else
  if (pMsg->type == HUB_MSG_TYPE_SET_OPTIONS) {
    if (handle == INVALID_HANDLE_VALUE)
      return FALSE;

    BYTE addedPins = (~maskOutPins & SO_O2V_PIN_STATE(pMsg->u.val));

    if (addedPins & PIN_STATE_RTS) {
      if (!SetManualRtsControl(handle)) {
        addedPins &= ~PIN_STATE_RTS;
        cerr << name << " WARNING! can't set manual RTS state mode" << endl;
      }
    }
    if (addedPins & PIN_STATE_DTR) {
      if (!SetManualDtrControl(handle)) {
        addedPins &= ~PIN_STATE_DTR;
        cerr << name << " WARNING! can't set manual DTR state mode" << endl;
      }
    }
    if (addedPins & PIN_STATE_OUT1) {
      addedPins &= ~PIN_STATE_OUT1;
      cerr << name << " WARNING! can't set manual OUT1 state mode" << endl;
    }
    if (addedPins & PIN_STATE_OUT2) {
      addedPins &= ~PIN_STATE_OUT2;
      cerr << name << " WARNING! can't set manual OUT2 state mode" << endl;
    }

    maskOutPins |= addedPins;
  }

  return TRUE;
}

BOOL ComPort::StartWaitCommEvent()
{
  if (countWaitCommEventOverlapped)
    return TRUE;

  if (handle == INVALID_HANDLE_VALUE)
    return FALSE;

  WaitCommEventOverlapped *pOverlapped;

  pOverlapped = new WaitCommEventOverlapped(*this);

  if (!pOverlapped)
    return FALSE;

  if (!pOverlapped->StartWaitCommEvent()) {
    delete pOverlapped;
    return FALSE;
  }

  countWaitCommEventOverlapped++;

  /*
  cout << name << " Started WaitCommEvent " << countReadOverlapped
       << " " << hex << events << dec << " ["
       << FieldToName(codeNameTableComEvents, events)
       << "] << endl;
  */

  return TRUE;
}

void ComPort::OnWrite(WriteOverlapped *pOverlapped, DWORD len, DWORD done)
{
  //cout << name << " OnWrite " << ::GetCurrentThreadId() << endl;

  delete pOverlapped;

  if (len > done)
    writeLost += len - done;

  if (writeQueued > writeQueueLimit/2 && (writeQueued - len) <= writeQueueLimit/2)
    pOnXon(hHub, hMasterPort);

  writeQueued -= len;
}

void ComPort::OnRead(ReadOverlapped *pOverlapped, BYTE *pBuf, DWORD done)
{
  //cout << name << " OnRead " << ::GetCurrentThreadId() << endl;

  HUB_MSG msg;

  msg.type = HUB_MSG_TYPE_LINE_DATA;
  msg.u.buf.pBuf = pBuf;
  msg.u.buf.size = done;

  pOnRead(hHub, hMasterPort, &msg);

  if (countXoff > 0 || !pOverlapped->StartRead()) {
    delete pOverlapped;
    countReadOverlapped--;

    cout << name << " Stopped Read " << countReadOverlapped << endl;
  }
}

void ComPort::OnCommEvent(WaitCommEventOverlapped *pOverlapped, DWORD eMask)
{
  //cout << name << " OnCommEvent " << ::GetCurrentThreadId() << endl;

  /*
  cout << name << " Event(s): 0x" << hex << eMask << dec << " ["
       << FieldToName(codeNameTableComEvents, eMask)
       << "]" << endl;
  */

  CheckComEvents(eMask);

  if (!events || !pOverlapped->StartWaitCommEvent()) {
    delete pOverlapped;
    countWaitCommEventOverlapped--;

    cout << name << " Stopped WaitCommEvent " << countWaitCommEventOverlapped << endl;
  }
}

void ComPort::CheckComEvents(DWORD eMask)
{
  if ((eMask & (EV_CTS|EV_DSR|EV_RLSD|EV_RING)) != 0) {
    DWORD stat;

    if (::GetCommModemStatus(handle, &stat)) {
      /*
      cout << name << " MODEM STATUS bit(s): 0x"
           << hex << stat << dec << " ["
           << FieldToName(codeNameTableModemStatus, stat)
           << "]" << endl;
      */

      HUB_MSG msg;

      msg.type = HUB_MSG_TYPE_MODEM_STATUS;
      msg.u.val = stat;

      pOnRead(hHub, hMasterPort, &msg);
    }
  }

  if ((eMask & (EV_BREAK|EV_ERR)) != 0) {
    DWORD errs;

    if (::ClearCommError(handle, &errs, NULL))
      errors |= errs;
  }
}

void ComPort::AddXoff(int count)
{
  _ASSERTE(handle != INVALID_HANDLE_VALUE);

  countXoff += count;

  if (countXoff <= 0)
    StartRead();
}

static FIELD2NAME codeNameTableCommErrors[] = {
  TOFIELD2NAME2(CE_, RXOVER),
  TOFIELD2NAME2(CE_, OVERRUN),
  TOFIELD2NAME2(CE_, RXPARITY),
  TOFIELD2NAME2(CE_, FRAME),
  TOFIELD2NAME2(CE_, BREAK),
  TOFIELD2NAME2(CE_, TXFULL),
  {0, 0, NULL}
};

void ComPort::LostReport()
{
  _ASSERTE(handle != INVALID_HANDLE_VALUE);

  if (writeLost) {
    writeLostTotal += writeLost;
    cout << "Write lost " << name << ": " << writeLost << ", total " << writeLostTotal << endl;
    writeLost = 0;
  }

  CheckComEvents(EV_BREAK|EV_ERR);

  if (errors) {
    cout << "Error " << name << ": " << FieldToName(codeNameTableCommErrors, errors, " ");
    errors = 0;

    #define IOCTL_SERIAL_GET_STATS CTL_CODE(FILE_DEVICE_SERIAL_PORT,35,METHOD_BUFFERED,FILE_ANY_ACCESS)

    typedef struct _SERIALPERF_STATS {
      ULONG ReceivedCount;
      ULONG TransmittedCount;
      ULONG FrameErrorCount;
      ULONG SerialOverrunErrorCount;
      ULONG BufferOverrunErrorCount;
      ULONG ParityErrorCount;
    } SERIALPERF_STATS, *PSERIALPERF_STATS;

    SERIALPERF_STATS stats;
    DWORD size;

    if (DeviceIoControl(handle, IOCTL_SERIAL_GET_STATS, NULL, 0, &stats, sizeof(stats), &size, NULL)) {
      cout << ", total"
        << " RXOVER=" << stats.BufferOverrunErrorCount
        << " OVERRUN=" << stats.SerialOverrunErrorCount
        << " RXPARITY=" << stats.ParityErrorCount
        << " FRAME=" << stats.FrameErrorCount;
    }

    cout << endl;
  }
}
///////////////////////////////////////////////////////////////
