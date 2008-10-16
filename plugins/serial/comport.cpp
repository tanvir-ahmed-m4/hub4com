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
 * Revision 1.13  2008/10/16 16:04:39  vfrolov
 * Added LBR_STATUS and LLC_STATUS
 *
 * Revision 1.12  2008/09/30 08:28:32  vfrolov
 * Added ability to control OUT1 and OUT2 pins
 * Added ability to get remote baud rate and line control settings
 * Added ability to set baud rate and line control
 * Added fallback to non escape mode
 *
 * Revision 1.11  2008/08/29 13:02:37  vfrolov
 * Added ESC_OPTS_MAP_EO2GO() and ESC_OPTS_MAP_GO2EO()
 *
 * Revision 1.10  2008/08/28 16:07:09  vfrolov
 * Tracing of HUB_MSG_TYPE_SET_PIN_STATE moved to the trace filter
 *
 * Revision 1.9  2008/08/22 16:57:12  vfrolov
 * Added
 *   HUB_MSG_TYPE_GET_ESC_OPTS
 *   HUB_MSG_TYPE_FAIL_ESC_OPTS
 *   HUB_MSG_TYPE_BREAK_STATUS
 *
 * Revision 1.8  2008/08/22 12:45:34  vfrolov
 * Added masking to HUB_MSG_TYPE_MODEM_STATUS and HUB_MSG_TYPE_LINE_STATUS
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
    intercepted_options(0),
    inOptions(0),
    outOptions(0),
    writeQueueLimit(256),
    writeQueued(0),
    writeLost(0),
    writeLostTotal(0),
    errors(0)
{
  filterX = comParams.InX();
  string path(pPath);
  name = path.substr(path.rfind('\\') + 1);
  pComIo = new ComIo(*this);

  if (!pComIo) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  if (!pComIo->Open(pPath, comParams)) {
    delete pComIo;
    pComIo = NULL;
  }
}

BOOL ComPort::Init(HMASTERPORT _hMasterPort, HHUB _hHub)
{
  if (!pComIo) {
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

static FIELD2NAME codeNameTableGoOptions[] = {
  TOFIELD2NAME2(GO_, RBR_STATUS),
  TOFIELD2NAME2(GO_, RLC_STATUS),
  TOFIELD2NAME2(GO_, LBR_STATUS),
  TOFIELD2NAME2(GO_, LLC_STATUS),
  TOFIELD2NAME2(GO_, BREAK_STATUS),
  TOFIELD2NAME2(GO_, ESCAPE_MODE),
  {0, 0, NULL}
};

static void WarnIgnoredInOptions(
    const char *pHead,
    const char *pTail,
    DWORD goOptions,
    DWORD otherOptions)
{
  if (GO_O2V_MODEM_STATUS(goOptions)) {
    cerr << pHead << " WARNING: Changing of MODEM STATUS bit(s) ["
         << FieldToName(codeNameTableModemStatus, GO_O2V_MODEM_STATUS(goOptions))
         << "] will be ignored by driver" << pTail << endl;
  }

  if (GO_O2V_LINE_STATUS(goOptions)) {
    cerr << pHead << " WARNING: Changing of LINE STATUS bit(s) ["
         << FieldToName(codeNameTableLineStatus, GO_O2V_LINE_STATUS(goOptions))
         << "] will be ignored by driver" << pTail << endl;
  }

  goOptions &= ~(GO_V2O_MODEM_STATUS(-1) | GO_V2O_LINE_STATUS(-1));

  if (goOptions) {
    cerr << pHead << " WARNING: Requested input option(s) ["
         << FieldToName(codeNameTableGoOptions, goOptions)
         << "] will be ignored by driver" << pTail << endl;
  }

  if (otherOptions) {
    cerr << pHead << " WARNING: Requested input option(s) [0x"
         << hex << otherOptions << dec
         << "] will be ignored by driver" << pTail << endl;
  }
}

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
  _ASSERTE(pComIo != NULL);

  BYTE *pBuf = NULL;
  DWORD done = 0;
  HUB_MSG msg;

  if (intercepted_options & GO_ESCAPE_MODE) {
    DWORD escapeOptions = 0;

    msg.type = HUB_MSG_TYPE_GET_ESC_OPTS;
    msg.u.pv.pVal = &escapeOptions;
    msg.u.pv.val = 0;
    pOnRead(hHub, hMasterPort, &msg);

    escapeOptions = pComIo->SetEscMode(escapeOptions, &pBuf, &done);

    if (escapeOptions & ~ESC_OPTS_V2O_ESCCHAR(-1)) {
      WarnIgnoredInOptions(name.c_str(), " (requested for escape mode)",
                           ESC_OPTS_MAP_EO2GO(escapeOptions),
                           escapeOptions & ~(ESC_OPTS_MAP_GO2EO(-1) | ESC_OPTS_V2O_ESCCHAR(-1)));
    }

    if ((escapeOptions & ESC_OPTS_V2O_ESCCHAR(-1)) == 0) {
      inOptions |= GO_ESCAPE_MODE;

      msg.type = HUB_MSG_TYPE_FAIL_ESC_OPTS;
      msg.u.pv.pVal = &intercepted_options;
      msg.u.pv.val = escapeOptions;
      pOnRead(hHub, hMasterPort, &msg);
    }
  }

  inOptions |= (intercepted_options & (
        GO_RBR_STATUS    |
        GO_RLC_STATUS    |
        GO_LBR_STATUS    |
        GO_LLC_STATUS    |
        MODEM_STATUS_CTS |
        MODEM_STATUS_DSR |
        MODEM_STATUS_DCD |
        MODEM_STATUS_RI));

  DWORD fail_options = (intercepted_options & ~inOptions);

  if (fail_options)
    WarnIgnoredInOptions(name.c_str(), "", fail_options, 0);

  msg.type = HUB_MSG_TYPE_FAIL_IN_OPTS;
  msg.u.val = fail_options;
  pOnRead(hHub, hMasterPort, &msg);

  if (inOptions & GO_V2O_MODEM_STATUS(
        MODEM_STATUS_CTS |
        MODEM_STATUS_DSR |
        MODEM_STATUS_DCD |
        MODEM_STATUS_RI))
  {
    DWORD events = 0;

    if (inOptions & GO_V2O_MODEM_STATUS(MODEM_STATUS_CTS))
      events |= EV_CTS;

    if (inOptions & GO_V2O_MODEM_STATUS(MODEM_STATUS_DSR))
      events |= EV_DSR;

    if (inOptions & GO_V2O_MODEM_STATUS(MODEM_STATUS_DCD))
      events |= EV_RLSD;

    if (inOptions & GO_V2O_MODEM_STATUS(MODEM_STATUS_RI))
      events |= EV_RING;

    if (!pComIo->SetComEvents(&events) || !StartWaitCommEvent()) {
      pBufFree(pBuf);
      return FALSE;
    }

    cout << name << " Event(s) 0x" << hex << events << dec << " ["
         << FieldToName(codeNameTableComEvents, events)
         << "] will be monitired" << endl;
  }

  if (!StartRead()) {
    pBufFree(pBuf);
    return FALSE;
  }

  msg.type = HUB_MSG_TYPE_CONNECT;
  msg.u.val = TRUE;
  pOnRead(hHub, hMasterPort, &msg);

  if (inOptions & GO_LBR_STATUS) {
    msg.type = HUB_MSG_TYPE_LBR_STATUS;
    msg.u.val = pComIo->GetBaudRate();
    pOnRead(hHub, hMasterPort, &msg);
  }

  if (inOptions & GO_LLC_STATUS) {
    msg.type = HUB_MSG_TYPE_LLC_STATUS;
    msg.u.val = pComIo->GetLineControl();
    pOnRead(hHub, hMasterPort, &msg);
  }

  if (inOptions & GO_RBR_STATUS) {
    cerr << name << " WARNING: Suppose remote baud rate is equal local settings" << endl;

    msg.type = HUB_MSG_TYPE_RBR_STATUS;
    msg.u.val = pComIo->GetBaudRate();
    pOnRead(hHub, hMasterPort, &msg);
  }

  if (inOptions & GO_RLC_STATUS) {
    cerr << name << " WARNING: Suppose remote byte size, parity and stop bits are equal local settings" << endl;

    msg.type = HUB_MSG_TYPE_RLC_STATUS;
    msg.u.val = pComIo->GetLineControl();
    pOnRead(hHub, hMasterPort, &msg);
  }

  if (pBuf) {
    msg.type = HUB_MSG_TYPE_LINE_DATA;
    msg.u.buf.pBuf = pBuf;
    msg.u.buf.size = done;
    pOnRead(hHub, hMasterPort, &msg);
  }

  CheckComEvents(DWORD(-1));

  return TRUE;
}

BOOL ComPort::StartRead()
{
  if (countReadOverlapped)
    return TRUE;

  if (!pComIo)
    return FALSE;

  ReadOverlapped *pOverlapped;

  pOverlapped = new ReadOverlapped(*pComIo);

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

  if (pInMsg->type == HUB_MSG_TYPE_GET_IN_OPTS) {
    // get interceptable options from subsequent filters separately

    DWORD interceptable_options = (pInMsg->u.pv.val &
        GO_ESCAPE_MODE |
        GO_RBR_STATUS |
        GO_RLC_STATUS |
        GO_LBR_STATUS |
        GO_LLC_STATUS |
        GO_BREAK_STATUS |
        GO_V2O_MODEM_STATUS(-1) |
        GO_V2O_LINE_STATUS(-1));

    pInMsg->u.pv.val &= ~interceptable_options;

    pInMsg = pMsgInsertNone(pInMsg, HUB_MSG_TYPE_EMPTY);

    if (pInMsg) {
      pInMsg->type = HUB_MSG_TYPE_GET_IN_OPTS;
      pInMsg->u.pv.pVal = &intercepted_options;
      pInMsg->u.pv.val = interceptable_options;
    }
  }

  return pInMsg != NULL;
}

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

    if (!pComIo) {
      writeLost += len;
      return FALSE;
    }

    WriteOverlapped *pOverlapped;

    pOverlapped = new WriteOverlapped(*pComIo, pBuf, len);

    if (!pOverlapped) {
      writeLost += len;
      return FALSE;
    }

    pMsg->type = HUB_MSG_TYPE_EMPTY;

    if (writeQueued > writeQueueLimit)
      pComIo->PurgeWrite();

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
    if (!pComIo)
      return FALSE;

    pComIo->SetPinState((WORD)pMsg->u.val, MASK2VAL(pMsg->u.val) & SO_O2V_PIN_STATE(outOptions));
  }
  else
  if (pMsg->type == HUB_MSG_TYPE_SET_BR) {
    if (!pComIo)
      return FALSE;

    DWORD oldVal = pComIo->GetBaudRate();
    DWORD curVal = pComIo->SetBaudRate(pMsg->u.val);

    if (curVal != pMsg->u.val) {
      cerr << name << " WARNING: can't change"
           << " baud rate " << oldVal
           << " to " << pMsg->u.val
           << " (current=" << curVal << ")"
           << endl;
    }

    if (oldVal != curVal) {
      if (inOptions & GO_LBR_STATUS) {
        HUB_MSG msg;

        msg.type = HUB_MSG_TYPE_LBR_STATUS;
        msg.u.val = curVal;
        pOnRead(hHub, hMasterPort, &msg);
      }

      if (inOptions & GO_RBR_STATUS) {
        HUB_MSG msg;

        msg.type = HUB_MSG_TYPE_RBR_STATUS;
        msg.u.val = curVal;                 // suppose remote equal local
        pOnRead(hHub, hMasterPort, &msg);
      }
    }
  }
  else
  if (pMsg->type == HUB_MSG_TYPE_SET_LC) {
    if (!pComIo)
      return FALSE;

    DWORD oldVal = pComIo->GetLineControl();;
    DWORD curVal = pComIo->SetLineControl(pMsg->u.val);

    if (curVal != pMsg->u.val) {
      cerr << name << " WARNING: can't change"
           << hex
           << " line control 0x" << oldVal
           << " to 0x" << pMsg->u.val
           << " (current=0x" << curVal << ")"
           << dec
           << endl;
    }

    if (oldVal != curVal) {
      if (inOptions & GO_LLC_STATUS) {
        HUB_MSG msg;

        msg.type = HUB_MSG_TYPE_LLC_STATUS;
        msg.u.val = curVal;
        pOnRead(hHub, hMasterPort, &msg);
      }

      if (inOptions & GO_RLC_STATUS) {
        HUB_MSG msg;

        msg.type = HUB_MSG_TYPE_RLC_STATUS;
        msg.u.val = curVal;                 // suppose remote equal local
        pOnRead(hHub, hMasterPort, &msg);
      }
    }
  }
  else
  if (pMsg->type == HUB_MSG_TYPE_SET_OUT_OPTS) {
    if (!pComIo)
      return FALSE;

    pMsg->u.val &= ~outOptions;

    if (pMsg->u.val & SO_V2O_PIN_STATE(PIN_STATE_RTS)) {
      if (pComIo->SetManualRtsControl())
        outOptions |= SO_V2O_PIN_STATE(PIN_STATE_RTS);
      else
        cerr << name << " WARNING: can't set manual RTS state mode" << endl;
    }
    if (pMsg->u.val & SO_V2O_PIN_STATE(PIN_STATE_DTR)) {
      if (pComIo->SetManualDtrControl())
        outOptions |= SO_V2O_PIN_STATE(PIN_STATE_DTR);
      else
        cerr << name << " WARNING: can't set manual DTR state mode" << endl;
    }
    if (pMsg->u.val & SO_V2O_PIN_STATE(PIN_STATE_OUT1)) {
      if (pComIo->SetManualOut1Control())
        outOptions |= SO_V2O_PIN_STATE(PIN_STATE_OUT1);
      else
        cerr << name << " WARNING: can't set manual OUT1 state mode" << endl;
    }
    if (pMsg->u.val & SO_V2O_PIN_STATE(PIN_STATE_OUT2)) {
      if (pComIo->SetManualOut2Control())
        outOptions |= SO_V2O_PIN_STATE(PIN_STATE_OUT2);
      else
        cerr << name << " WARNING: can't set manual OUT2 state mode" << endl;
    }

    outOptions |= (
        SO_V2O_PIN_STATE(PIN_STATE_BREAK) |
        SO_SET_BR |
        SO_SET_LC);

    pMsg->u.val &= ~outOptions;

    if (pMsg->u.val) {
      cerr << name << " WARNING: Requested output option(s) [0x"
           << hex << pMsg->u.val << dec
           << "] will be ignored by driver" << endl;
    }
  }

  return TRUE;
}

BOOL ComPort::StartWaitCommEvent()
{
  if (countWaitCommEventOverlapped)
    return TRUE;

  if (!pComIo)
    return FALSE;

  WaitCommEventOverlapped *pOverlapped;

  pOverlapped = new WaitCommEventOverlapped(*pComIo);

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

    if (countXoff <= 0)
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

  if (!pOverlapped->StartWaitCommEvent()) {
    delete pOverlapped;
    countWaitCommEventOverlapped--;

    cout << name << " Stopped WaitCommEvent " << countWaitCommEventOverlapped << endl;
  }
}

void ComPort::CheckComEvents(DWORD eMask)
{
  if (GO_O2V_MODEM_STATUS(inOptions) && (eMask & (EV_CTS|EV_DSR|EV_RLSD|EV_RING)) != 0) {
    DWORD stat;

    if (::GetCommModemStatus(pComIo->Handle(), &stat)) {
      HUB_MSG msg;

      msg.type = HUB_MSG_TYPE_MODEM_STATUS;
      msg.u.val = ((DWORD)(BYTE)stat | VAL2MASK(GO_O2V_MODEM_STATUS(inOptions)));

      pOnRead(hHub, hMasterPort, &msg);
    }
  }

  if ((eMask & (EV_BREAK|EV_ERR)) != 0) {
    DWORD errs;

    if (::ClearCommError(pComIo->Handle(), &errs, NULL))
      errors |= errs;
  }
}

void ComPort::AddXoff(int count)
{
  _ASSERTE(pComIo != NULL);

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
  _ASSERTE(pComIo != NULL);

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

    if (DeviceIoControl(pComIo->Handle(), IOCTL_SERIAL_GET_STATS, NULL, 0, &stats, sizeof(stats), &size, NULL)) {
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
