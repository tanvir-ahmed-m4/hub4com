/*
 * $Id$
 *
 * Copyright (c) 2008-2011 Vyacheslav Frolov
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
 * Revision 1.21  2011/07/25 07:03:55  vfrolov
 * Fixed set-ID field assertion
 *
 * Revision 1.20  2009/09/14 11:22:46  vfrolov
 * Added discarding owned tick (for optimization)
 *
 * Revision 1.19  2009/03/06 07:56:28  vfrolov
 * Fixed assertion with non ascii chars
 *
 * Revision 1.18  2009/02/20 18:32:35  vfrolov
 * Added info about location of options
 *
 * Revision 1.17  2009/02/17 14:17:37  vfrolov
 * Redesigned timer's API
 *
 * Revision 1.16  2009/02/02 15:21:42  vfrolov
 * Optimized filter's API
 *
 * Revision 1.15  2009/01/26 15:07:52  vfrolov
 * Implemented --keep-active option
 *
 * Revision 1.14  2009/01/23 17:01:28  vfrolov
 * Fixed discarding of data if engine not started
 *
 * Revision 1.13  2008/12/22 09:40:46  vfrolov
 * Optimized message switching
 *
 * Revision 1.12  2008/12/18 16:50:52  vfrolov
 * Extended the number of possible IN options
 *
 * Revision 1.11  2008/12/11 13:13:40  vfrolov
 * Implemented PURGE-DATA (RFC 2217)
 *
 * Revision 1.10  2008/12/05 14:12:04  vfrolov
 * Fixed return values
 *
 * Revision 1.9  2008/11/25 16:40:40  vfrolov
 * Added assert for port handle
 *
 * Revision 1.8  2008/11/24 16:39:58  vfrolov
 * Implemented FLOWCONTROL-SUSPEND and FLOWCONTROL-RESUME commands (RFC 2217)
 *
 * Revision 1.7  2008/11/24 12:37:00  vfrolov
 * Changed plugin API
 *
 * Revision 1.6  2008/11/13 07:44:13  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.5  2008/10/24 08:29:01  vfrolov
 * Implemented RFC 2217
 *
 * Revision 1.4  2008/10/09 11:02:58  vfrolov
 * Redesigned class TelnetProtocol
 *
 * Revision 1.3  2008/08/20 10:08:29  vfrolov
 * Added strict option checking
 *
 * Revision 1.2  2008/04/14 07:32:04  vfrolov
 * Renamed option --use-port-module to --use-driver
 *
 * Revision 1.1  2008/03/28 16:05:44  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "../plugins_api.h"
///////////////////////////////////////////////////////////////
namespace FilterTelnet {
///////////////////////////////////////////////////////////////
#include "import.h"
#include "opt_termtype.h"
#include "opt_comport.h"
///////////////////////////////////////////////////////////////
static ROUTINE_PORT_NAME_A *pPortName;
static ROUTINE_FILTER_NAME_A *pFilterName;
static ROUTINE_TIMER_CREATE *pTimerCreate;
static ROUTINE_TIMER_SET *pTimerSet;
static ROUTINE_TIMER_DELETE *pTimerDelete;
static ROUTINE_FILTERPORT *pFilterPort;
static ROUTINE_GET_ARG_INFO_A *pGetArgInfo;
///////////////////////////////////////////////////////////////
#ifndef _DEBUG
  #define DEBUG_PARAM(par)
#else   /* _DEBUG */
  #define DEBUG_PARAM(par) par
#endif  /* _DEBUG */
///////////////////////////////////////////////////////////////
const char *GetParam(const char *pArg, const char *pPattern)
{
  size_t lenPattern = strlen(pPattern);

  if (_strnicmp(pArg, pPattern, lenPattern) != 0)
    return NULL;

  return pArg + lenPattern;
}
///////////////////////////////////////////////////////////////
static void Diag(const char *pPref, const char *pArg)
{
  cerr << pPref << "'" << pArg << "'";

  string pref(" (");
  string suff;

  const ARG_INFO_A *pInfo = pGetArgInfo(pArg);

  if (ITEM_IS_VALID(pInfo, pFile) && pInfo->pFile != NULL) {
    cerr << pref << "file " << pInfo->pFile;
    pref = ", ";
    suff = ")";
  }

  if (ITEM_IS_VALID(pInfo, iLine) && pInfo->iLine >= 0) {
    cerr << pref << "line " << (pInfo->iLine + 1);
    pref = ", ";
    suff = ")";
  }

  cerr << suff;

  if (ITEM_IS_VALID(pInfo, pReference) && pInfo->pReference != NULL)
    cerr << "," << endl << pInfo->pReference;

  cerr << endl;
}
///////////////////////////////////////////////////////////////
class Valid {
  public:
    Valid() : isValid(TRUE) {}
    void Invalidate() { isValid = FALSE; }
    BOOL IsValid() const { return isValid; }
  private:
    BOOL isValid;
};
///////////////////////////////////////////////////////////////
class Filter;

class State {
  public:
    State(HMASTERPORT _hMasterPort)
    : hMasterPort(_hMasterPort),
      pName(pPortName(_hMasterPort)),
      pTelnetProtocol(NULL),
      pComPort(NULL),
      soMask(0),
      pinState(0),
      br(0),
      lc(0),
      hKeepActiveTimer(NULL)
    {
      for (int iGo = 0 ; iGo < sizeof(goMask)/sizeof(goMask[0]) ; iGo++)
        goMask[iGo] = 0;
    }

    ~State() {
      if (pTelnetProtocol)
        delete pTelnetProtocol;

      if (hKeepActiveTimer)
        pTimerDelete(hKeepActiveTimer);
    }

    TelnetProtocol *CreateProtocol(const Filter &filter);

    void DelProtocol() {
      _ASSERTE(pTelnetProtocol != NULL);

      pComPort = NULL;
      delete pTelnetProtocol;

      pTelnetProtocol = NULL;
    }

    const HMASTERPORT hMasterPort;
    const char *const pName;

    TelnetProtocol *pTelnetProtocol;
    TelnetOptionComPort *pComPort;

    DWORD soMask;
    DWORD goMask[2];
    WORD pinState;
    DWORD br;
    DWORD lc;

    HMASTERTIMER hKeepActiveTimer;
};
///////////////////////////////////////////////////////////////
class Filter : public Valid {
  public:
    Filter(const char *pName, int argc, const char *const argv[]);

    const char *FilterName() const { return pName; }

    DWORD soMask;
    DWORD goMask[2];

    unsigned keepActive;

  private:
    friend class State;

    const char *const pName;
    string terminalType;
    BOOL suppressEcho;

    enum {
      comport_no,
      comport_client,
      comport_server
    } comport;
};

Filter::Filter(const char *_pName, int argc, const char *const argv[])
  : pName(_pName),
    terminalType("UNKNOWN"),
    suppressEcho(FALSE),
    keepActive(0),
    comport(comport_no)
{
  for (const char *const *pArgs = &argv[1] ; argc > 1 ; pArgs++, argc--) {
    const char *pArg = GetParam(*pArgs, "--");

    if (!pArg) {
      Diag("Unknown option ", *pArgs);
      Invalidate();
      continue;
    }

    const char *pParam;

    if ((pParam = GetParam(pArg, "terminal=")) != NULL) {
      terminalType = pParam;
    }
    else
    if ((pParam = GetParam(pArg, "comport=")) != NULL) {
      switch (tolower((unsigned char)*pParam)) {
        case 'n':
          comport = comport_no;
          break;
        case 'c':
          comport = comport_client;
          break;
        case 's':
          comport = comport_server;
          break;
        default:
          Diag("Unknown value in ", *pArgs);
          Invalidate();
      }
    }
    else
    if ((pParam = GetParam(pArg, "suppress-echo=")) != NULL) {
      switch (tolower((unsigned char)*pParam)) {
        case 'y':
          suppressEcho = TRUE;
          break;
        case 'n':
          suppressEcho = FALSE;
          break;
        default:
          Diag("Unknown value in ", *pArgs);
          Invalidate();
      }
    }
    else
    if ((pParam = GetParam(pArg, "keep-active=")) != NULL) {
      if (isdigit((unsigned char)*pParam)) {
        keepActive = (unsigned)atol(pParam);
      } else {
        Diag("Invalid value in ", *pArgs);
        Invalidate();
      }
    }
    else {
      Diag("Unknown option ", *pArgs);
      Invalidate();
    }
  }

  switch (comport) {
    case comport_client:
      soMask = SO_V2O_PIN_STATE(PIN_STATE_DTR|PIN_STATE_RTS|PIN_STATE_BREAK) |
               SO_SET_BR|SO_SET_LC|SO_PURGE_TX;

      goMask[0] = GO0_LBR_STATUS|GO0_LLC_STATUS;
      goMask[1] = GO1_V2O_LINE_STATUS(-1) | GO1_V2O_MODEM_STATUS(-1);
      break;
    case comport_server:
      soMask = SO_V2O_PIN_STATE(SPS_V2P_MST(-1)) |
               SO_V2O_LINE_STATUS(-1) |
               SO_SET_BR|SO_SET_LC;

      goMask[0] = 0;
      goMask[1] = GO1_V2O_MODEM_STATUS(MODEM_STATUS_CTS|MODEM_STATUS_DSR) |
                  GO1_RBR_STATUS|GO1_RLC_STATUS|GO1_BREAK_STATUS|GO1_PURGE_TX_IN;
      break;
    default:
      soMask = 0;
      goMask[0] = 0;
      goMask[1] = 0;
  }
}
///////////////////////////////////////////////////////////////
TelnetProtocol *State::CreateProtocol(const Filter &filter)
{
  _ASSERTE(pTelnetProtocol == NULL);

  pTelnetProtocol = new TelnetProtocol(pName);

  if (!pTelnetProtocol)
    return NULL;

  TelnetOption *pTo;

  if (!filter.terminalType.empty()) {
    pTo = new TelnetOptionTerminalType(*pTelnetProtocol, filter.terminalType.c_str());

    if (pTo)
      pTo->LocalCan();
  }

  pTo = new TelnetOption(*pTelnetProtocol, 0 /*TRANSMIT-BINARY*/);

  if (pTo) {
    pTo->RemoteMay();
    pTo->LocalCan();
  }

  pTo = new TelnetOption(*pTelnetProtocol, 1 /*ECHO*/);

  if (pTo) {
    pTo->RemoteMay();

    if (filter.suppressEcho)
      pTo->LocalWill();
  }

  pTo = new TelnetOption(*pTelnetProtocol, 3 /*SUPPRESS-GO-AHEAD*/);

  if (pTo) {
    pTo->RemoteMay();
    pTo->LocalCan();
  }

  if (filter.comport == filter.comport_client) {
    pTo = new TelnetOptionComPortClient(*pTelnetProtocol, goMask, soMask);

    if (pTo) {
      pTo->LocalWill();
      pComPort = (TelnetOptionComPortClient *)pTo;
    }
  }
  else
  if (filter.comport == filter.comport_server) {
    pTo = new TelnetOptionComPortServer(*pTelnetProtocol, goMask, soMask);

    if (pTo) {
      pTo->RemoteDo();
      pComPort = (TelnetOptionComPortServer *)pTo;
    }
  }
  else {
    pComPort = NULL;
  }

  return pTelnetProtocol;
}
///////////////////////////////////////////////////////////////
static PLUGIN_TYPE CALLBACK GetPluginType()
{
  return PLUGIN_TYPE_FILTER;
}
///////////////////////////////////////////////////////////////
static const PLUGIN_ABOUT_A about = {
  sizeof(PLUGIN_ABOUT_A),
  "telnet",
  "Copyright (c) 2008 Vyacheslav Frolov",
  "GNU General Public License",
  "Telnet protocol filter",
};

static const PLUGIN_ABOUT_A * CALLBACK GetPluginAbout()
{
  return &about;
}
///////////////////////////////////////////////////////////////
static void CALLBACK Help(const char *pProgPath)
{
  cerr
  << "Usage:" << endl
  << "  " << pProgPath << " ... --create-filter=" << GetPluginAbout()->pName << "[,<FID>][:<options>] ... --add-filters=<ports>:[...,]<FID>[,...] ..." << endl
  << endl
  << "Options:" << endl
  << "  --terminal=<type>        - use terminal type <type> (RFC 1091)." << endl
  << "  --comport=<mode>         - enable/disable Com Port Control Option (RFC 2217)." << endl
  << "                             Where <mode> is c[lient], s[erver] or n[o] (no by" << endl
  << "                             default)." << endl
  << "  --suppress-echo=<c>      - enable/disable local echo suppression on the" << endl
  << "                             remote side. Where <c> is y[es] or n[o] (no by" << endl
  << "                             default)." << endl
  << "  --keep-active=<s>        - send NOP command every <s> seconds to keep the" << endl
  << "                             connection active if data is not transferred." << endl
  << endl
  << "IN method input data stream description:" << endl
  << "  LINE_DATA - telnet protocol data (will be discarded if engine not started)." << endl
  << "  CONNECT(TRUE) - start telnet protocol engine." << endl
  << "  CONNECT(FALSE) - stop telnet protocol engine." << endl
  << endl
  << "IN method output data stream description:" << endl
  << "  LINE_DATA - raw data." << endl
  << "  ADD_XOFF_XON - flow control." << endl
  << "  " << endl
  << "  Notifications about events on the server side (RFC 2217 client mode):" << endl
  << "  " << endl
  << "  LBR_STATUS - baud rate was set." << endl
  << "  LLC_STATUS - data bits, parity or stop bits was set." << endl
  << "  LINE_STATUS - line state was changed." << endl
  << "  MODEM_STATUS - modem line state was changed." << endl
  << "  " << endl
  << "  Notifications about commands from client side (RFC 2217 server mode):" << endl
  << "  " << endl
  << "  RBR_STATUS - set baud rate." << endl
  << "  RLC_STATUS - set data bits, parity or stop bits." << endl
  << "  BREAK_STATUS - set BREAK state." << endl
  << "  MODEM_STATUS(DSR/CTS) - set modem lines (DTR/RTS)." << endl
  << endl
  << "IN method echo data stream description:" << endl
  << "  LINE_DATA - telnet protocol data." << endl
  << endl
  << "OUT method input data stream description:" << endl
  << "  LINE_DATA - raw data  (will be discarded if engine not started)." << endl
  << "  ADD_XOFF_XON - flow control." << endl
  << "  " << endl
  << "  RFC 2217 client mode:" << endl
  << "  " << endl
  << "  SET_BR - set baud rate." << endl
  << "  SET_LC - set data bits, parity or stop bits." << endl
  << "  SET_PIN_STATE - set modem lines (DTR/RTS)." << endl
  << "  " << endl
  << "  RFC 2217 server mode:" << endl
  << "  " << endl
  << "  SET_BR - baud rate was set." << endl
  << "  SET_LC - data bits, parity or stop bits was set." << endl
  << "  SET_LSR - line state was changed." << endl
  << "  SET_PIN_STATE - modem line state was changed." << endl
  << endl
  << "OUT method output data stream description:" << endl
  << "  LINE_DATA - telnet protocol data." << endl
  << endl
  << "Examples:" << endl
  << "  " << pProgPath << " --load=,,_END_" << endl
  << "      COM1" << endl
  << "      --create-filter=telnet:--terminal=ANSI" << endl
  << "      add-filters=1:telnet" << endl
  << "      --use-driver=tcp" << endl
  << "      *your.telnet.server:telnet" << endl
  << "      _END_" << endl
  << "    - use the ANSI terminal connected to the port COM1 for working with the" << endl
  << "      telnet server your.telnet.server." << endl
  ;
}
///////////////////////////////////////////////////////////////
static HFILTER CALLBACK Create(
    HMASTERFILTER hMasterFilter,
    HCONFIG /*hConfig*/,
    int argc,
    const char *const argv[])
{
  _ASSERTE(hMasterFilter != NULL);

  Filter *pFilter = new Filter(pFilterName(hMasterFilter), argc, argv);

  if (!pFilter) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  if (!pFilter->IsValid()) {
    delete pFilter;
    return NULL;
  }

  return (HFILTER)pFilter;
}
///////////////////////////////////////////////////////////////
static void CALLBACK Delete(
    HFILTER hFilter)
{
  _ASSERTE(hFilter != NULL);

  delete (Filter *)hFilter;
}
///////////////////////////////////////////////////////////////
static HFILTERINSTANCE CALLBACK CreateInstance(
    HMASTERFILTERINSTANCE hMasterFilterInstance)
{
  _ASSERTE(hMasterFilterInstance != NULL);

  HMASTERPORT hMasterPort = pFilterPort(hMasterFilterInstance);

  _ASSERTE(hMasterPort != NULL);

  return (HFILTERINSTANCE)new State(hMasterPort);
}
///////////////////////////////////////////////////////////////
static void CALLBACK DeleteInstance(
    HFILTERINSTANCE hFilterInstance)
{
  _ASSERTE(hFilterInstance != NULL);

  delete (State *)hFilterInstance;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK InMethod(
    HFILTER hFilter,
    HFILTERINSTANCE hFilterInstance,
    HUB_MSG *pInMsg,
    HUB_MSG **ppEchoMsg)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(hFilterInstance != NULL);
  _ASSERTE(pInMsg != NULL);
  _ASSERTE(ppEchoMsg != NULL);
  _ASSERTE(*ppEchoMsg == NULL);

  switch (HUB_MSG_T2N(pInMsg->type)) {
    case HUB_MSG_T2N(HUB_MSG_TYPE_GET_IN_OPTS): {
      int iGo = GO_O2I(pInMsg->u.pv.val);

      if (iGo != 0 && iGo != 1)
        break;

      if (!((Filter *)hFilter)->goMask[iGo == 0 ? 0 : 1])
        break;

      // get interceptable options from subsequent filters separately

      DWORD interceptable_options = (pInMsg->u.pv.val & ((Filter *)hFilter)->goMask[iGo == 0 ? 0 : 1]);

      _ASSERTE((interceptable_options & GO_I2O(-1)) == 0);
      interceptable_options &= ~GO_I2O(-1);

      pInMsg->u.pv.val &= ~interceptable_options;

      pInMsg = pMsgInsertNone(pInMsg, HUB_MSG_TYPE_EMPTY);

      if (!pInMsg)
        return FALSE;

      pInMsg->type = HUB_MSG_TYPE_GET_IN_OPTS;
      pInMsg->u.pv.pVal = &((State *)hFilterInstance)->goMask[iGo == 0 ? 0 : 1];
      pInMsg->u.pv.val = interceptable_options | GO_I2O(iGo);

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_LINE_DATA): {
      _ASSERTE(pInMsg->u.buf.pBuf != NULL || pInMsg->u.buf.size == 0);

      if (pInMsg->u.buf.size == 0)
        break;

      TelnetProtocol *pTelnetProtocol = ((State *)hFilterInstance)->pTelnetProtocol;

      if (!pTelnetProtocol) {
        // discard data
        if (!pMsgReplaceNone(pInMsg, HUB_MSG_TYPE_EMPTY))
          return FALSE;

        break;
      }

      if (!pTelnetProtocol->Decode(pInMsg))
        return FALSE;

      pTelnetProtocol->FlushEncodedStream(ppEchoMsg);

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_CONNECT): {
      if (!pInMsg->u.val) {
        // CONNECT(FALSE)

        if (((State *)hFilterInstance)->hKeepActiveTimer) {
          pTimerDelete(((State *)hFilterInstance)->hKeepActiveTimer);
          ((State *)hFilterInstance)->hKeepActiveTimer = NULL;
        }

        if (((State *)hFilterInstance)->pComPort) {
          if (((State *)hFilterInstance)->goMask[1] & GO1_BREAK_STATUS) {
            pInMsg = pMsgInsertVal(pInMsg, HUB_MSG_TYPE_BREAK_STATUS, FALSE);

            if (!pInMsg)
              return FALSE;
          }

          if (((State *)hFilterInstance)->goMask[1] & GO1_V2O_MODEM_STATUS(-1)) {
            pInMsg = pMsgInsertVal(pInMsg, HUB_MSG_TYPE_MODEM_STATUS,
                 0 | VAL2MASK(GO1_O2V_MODEM_STATUS(((State *)hFilterInstance)->goMask[1])));

            if (!pInMsg)
              return FALSE;
          }
        }

        ((State *)hFilterInstance)->DelProtocol();

        break;
      }

      // CONNECT(TRUE)

      TelnetProtocol *pTelnetProtocol = ((State *)hFilterInstance)->CreateProtocol(*(Filter *)hFilter);

      if (!pTelnetProtocol)
        return FALSE;

      pTelnetProtocol->Start();

      if (((State *)hFilterInstance)->pComPort) {
        // Set current state

        if ((((State *)hFilterInstance)->soMask & SO_SET_BR) && ((State *)hFilterInstance)->br != 0)
          ((State *)hFilterInstance)->pComPort->SetBR(((State *)hFilterInstance)->br);

        if (((State *)hFilterInstance)->soMask & SO_SET_LC)
          ((State *)hFilterInstance)->pComPort->SetLC(((State *)hFilterInstance)->lc);

        if (((State *)hFilterInstance)->soMask & SO_V2O_PIN_STATE(SPS_V2P_MST(-1)))
          ((State *)hFilterInstance)->pComPort->NotifyMST(SPS_P2V_MST(((State *)hFilterInstance)->pinState));

        if (((State *)hFilterInstance)->soMask & SO_V2O_PIN_STATE(SPS_V2P_MCR(-1)))
          ((State *)hFilterInstance)->pComPort->SetMCR(SPS_P2V_MCR(((State *)hFilterInstance)->pinState), SPS_P2V_MCR(SO_O2V_PIN_STATE(((State *)hFilterInstance)->soMask)));

        if (((State *)hFilterInstance)->soMask & SO_V2O_PIN_STATE(PIN_STATE_BREAK))
          ((State *)hFilterInstance)->pComPort->SetBreak((((State *)hFilterInstance)->pinState & PIN_STATE_BREAK) != 0);

        if (((State *)hFilterInstance)->soMask & SO_V2O_LINE_STATUS(-1))
          ((State *)hFilterInstance)->pComPort->NotifyLSR(0);
      }

      if (((Filter *)hFilter)->keepActive) {
        _ASSERTE(((State *)hFilterInstance)->hKeepActiveTimer == NULL);

        ((State *)hFilterInstance)->hKeepActiveTimer = pTimerCreate((HTIMEROWNER)hFilterInstance);

        if (((State *)hFilterInstance)->hKeepActiveTimer) {
          LARGE_INTEGER firstReportTime;

          firstReportTime.QuadPart = -10000000LL * ((Filter *)hFilter)->keepActive;

          pTimerSet(
              ((State *)hFilterInstance)->hKeepActiveTimer,
              ((State *)hFilterInstance)->hMasterPort,
              &firstReportTime,
              ((Filter *)hFilter)->keepActive * 1000L,
              (HTIMERPARAM)((State *)hFilterInstance)->hKeepActiveTimer);
        }
      }

      pTelnetProtocol->FlushEncodedStream(ppEchoMsg);

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_TICK): {
      if (pInMsg->u.hv2.hVal0 != hFilterInstance)
        break;

      if (pInMsg->u.hv2.hVal1 == ((State *)hFilterInstance)->hKeepActiveTimer) {
        TelnetProtocol *pTelnetProtocol = ((State *)hFilterInstance)->pTelnetProtocol;

        if (pTelnetProtocol) {
          pTelnetProtocol->KeepActive();
          pTelnetProtocol->FlushEncodedStream(ppEchoMsg);
        }
      }

      // discard owned tick
      if (!pMsgReplaceNone(pInMsg, HUB_MSG_TYPE_EMPTY))
        return FALSE;

      break;
    }
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK OutMethod(
    HFILTER hFilter,
    HFILTERINSTANCE hFilterInstance,
    HMASTERPORT DEBUG_PARAM(hFromPort),
    HUB_MSG *pOutMsg)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(hFilterInstance != NULL);
  _ASSERTE(hFromPort != NULL);
  _ASSERTE(pOutMsg != NULL);

  switch (HUB_MSG_T2N(pOutMsg->type)) {
    case HUB_MSG_T2N(HUB_MSG_TYPE_SET_OUT_OPTS): {
      DWORD soMask = (pOutMsg->u.val & ((Filter *)hFilter)->soMask);

      // discard supported options
      pOutMsg->u.val &= ~((Filter *)hFilter)->soMask;

      ((State *)hFilterInstance)->soMask |= soMask;

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_SET_BR): {
      _ASSERTE(((((State *)hFilterInstance)->soMask | ~((Filter *)hFilter)->soMask) & SO_SET_BR) != 0);

      if (((State *)hFilterInstance)->soMask & SO_SET_BR) {
        DWORD val = pOutMsg->u.val;

        // discard messages for supported options
        if (!pMsgReplaceNone(pOutMsg, HUB_MSG_TYPE_EMPTY))
          return FALSE;

        if (((State *)hFilterInstance)->br != val) {
          ((State *)hFilterInstance)->br = val;

          if (((State *)hFilterInstance)->pComPort) {
            ((State *)hFilterInstance)->pComPort->SetBR(((State *)hFilterInstance)->br);
            _ASSERTE(((State *)hFilterInstance)->pTelnetProtocol != NULL);
            ((State *)hFilterInstance)->pTelnetProtocol->FlushEncodedStream(&pOutMsg);
          }
        }
      }

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_SET_LC): {
      _ASSERTE((pOutMsg->u.val & ~(VAL2LC_BYTESIZE(-1)|LC_MASK_BYTESIZE
                                  |VAL2LC_PARITY(-1)|LC_MASK_PARITY
                                  |VAL2LC_STOPBITS(-1)|LC_MASK_STOPBITS)) == 0);

      _ASSERTE(((((State *)hFilterInstance)->soMask | ~((Filter *)hFilter)->soMask) & SO_SET_LC) != 0);

      if (((State *)hFilterInstance)->soMask & SO_SET_LC) {
        DWORD val = pOutMsg->u.val;

        // discard messages for supported options
        if (!pMsgReplaceNone(pOutMsg, HUB_MSG_TYPE_EMPTY))
          return FALSE;

        if ((val & LC_MASK_BYTESIZE) == 0) {
          _ASSERTE((val & VAL2LC_BYTESIZE(-1)) == 0);
          val |= (((State *)hFilterInstance)->lc & (VAL2LC_BYTESIZE(-1)|LC_MASK_BYTESIZE));
        }

        if ((val & LC_MASK_PARITY) == 0) {
          _ASSERTE((val & VAL2LC_PARITY(-1)) == 0);
          val |= (((State *)hFilterInstance)->lc & (VAL2LC_PARITY(-1)|LC_MASK_PARITY));
        }

        if ((val & LC_MASK_STOPBITS) == 0) {
          _ASSERTE((val & VAL2LC_STOPBITS(-1)) == 0);
          val |= (((State *)hFilterInstance)->lc & (VAL2LC_STOPBITS(-1)|LC_MASK_STOPBITS));
        }

        if (((State *)hFilterInstance)->lc != val) {
          ((State *)hFilterInstance)->lc = val;

          if (((State *)hFilterInstance)->pComPort) {
            ((State *)hFilterInstance)->pComPort->SetLC(((State *)hFilterInstance)->lc);
            _ASSERTE(((State *)hFilterInstance)->pTelnetProtocol != NULL);
            ((State *)hFilterInstance)->pTelnetProtocol->FlushEncodedStream(&pOutMsg);
          }
        }
      }

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_SET_PIN_STATE): {
      _ASSERTE(((((State *)hFilterInstance)->soMask | ~((Filter *)hFilter)->soMask) & SO_V2O_PIN_STATE(-1)) != 0);

      WORD mask = MASK2VAL(pOutMsg->u.val) & SO_O2V_PIN_STATE(((State *)hFilterInstance)->soMask);

      if (!mask)
        break;

      WORD pinState = (WORD)pOutMsg->u.val;

      // discard settings for supported options
      pOutMsg->u.val &= ~VAL2MASK(mask);

      pinState = ((pinState & mask) | (((State *)hFilterInstance)->pinState & ~mask));
      mask = ((State *)hFilterInstance)->pinState ^ pinState;

      if (mask) {
        if (((State *)hFilterInstance)->pComPort) {
          if (SPS_P2V_MST(mask))
            ((State *)hFilterInstance)->pComPort->NotifyMST(SPS_P2V_MST(pinState));

          if (SPS_P2V_MCR(mask))
            ((State *)hFilterInstance)->pComPort->SetMCR(SPS_P2V_MCR(pinState), SPS_P2V_MCR(mask));

          if (mask & PIN_STATE_BREAK)
            ((State *)hFilterInstance)->pComPort->SetBreak((pinState & PIN_STATE_BREAK) != 0);

          _ASSERTE(((State *)hFilterInstance)->pTelnetProtocol != NULL);
          ((State *)hFilterInstance)->pTelnetProtocol->FlushEncodedStream(&pOutMsg);
        }

        ((State *)hFilterInstance)->pinState = pinState;
      }

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_SET_LSR): {
      _ASSERTE(((((State *)hFilterInstance)->soMask | ~((Filter *)hFilter)->soMask) & SO_V2O_LINE_STATUS(-1)) != 0);

      BYTE lsr;

      lsr = (BYTE)pOutMsg->u.val & (BYTE)MASK2VAL(pOutMsg->u.val) & SO_O2V_LINE_STATUS(((State *)hFilterInstance)->soMask);

      // discard settings for supported options
      pOutMsg->u.val &= ~VAL2MASK(SO_O2V_LINE_STATUS(((State *)hFilterInstance)->soMask));

      if (!lsr)
        break;

      if (((State *)hFilterInstance)->pComPort) {
        ((State *)hFilterInstance)->pComPort->NotifyLSR(lsr);
        _ASSERTE(((State *)hFilterInstance)->pTelnetProtocol != NULL);
        ((State *)hFilterInstance)->pTelnetProtocol->FlushEncodedStream(&pOutMsg);
      }

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_PURGE_TX): {
      _ASSERTE(((((State *)hFilterInstance)->soMask | ~((Filter *)hFilter)->soMask) & SO_PURGE_TX) != 0);

      if (((State *)hFilterInstance)->soMask & SO_PURGE_TX) {
        _ASSERTE(pOutMsg->u.val == (BYTE)pOutMsg->u.val);

        // discard messages for supported options
        if (!pMsgReplaceNone(pOutMsg, HUB_MSG_TYPE_EMPTY))
          return FALSE;

        if (((State *)hFilterInstance)->pComPort) {
          ((State *)hFilterInstance)->pComPort->PurgeTx();
          _ASSERTE(((State *)hFilterInstance)->pTelnetProtocol != NULL);
          ((State *)hFilterInstance)->pTelnetProtocol->FlushEncodedStream(&pOutMsg);
        }
      }

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_ADD_XOFF_XON): {
      if (((State *)hFilterInstance)->pComPort) {
        ((State *)hFilterInstance)->pComPort->AddXoffXon(pOutMsg->u.val);
        _ASSERTE(((State *)hFilterInstance)->pTelnetProtocol != NULL);
        ((State *)hFilterInstance)->pTelnetProtocol->FlushEncodedStream(&pOutMsg);
      }

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_LINE_DATA): {
      _ASSERTE(pOutMsg->u.buf.pBuf != NULL || pOutMsg->u.buf.size == 0);

      if (pOutMsg->u.buf.size == 0)
        break;

      TelnetProtocol *pTelnetProtocol = ((State *)hFilterInstance)->pTelnetProtocol;

      if (!pTelnetProtocol) {
        // discard data
        if (!pMsgReplaceNone(pOutMsg, HUB_MSG_TYPE_EMPTY))
          return FALSE;

        break;
      }

      if (!pTelnetProtocol->Encode(pOutMsg))
        return FALSE;

      break;
    }
  }

  return pOutMsg != NULL;
}
///////////////////////////////////////////////////////////////
static const FILTER_ROUTINES_A routines = {
  sizeof(FILTER_ROUTINES_A),
  GetPluginType,
  GetPluginAbout,
  Help,
  NULL,           // ConfigStart
  NULL,           // Config
  NULL,           // ConfigStop
  Create,
  Delete,
  CreateInstance,
  DeleteInstance,
  InMethod,
  OutMethod,
};

static const PLUGIN_ROUTINES_A *const plugins[] = {
  (const PLUGIN_ROUTINES_A *)&routines,
  NULL
};
///////////////////////////////////////////////////////////////
ROUTINE_MSG_INSERT_VAL *pMsgInsertVal;
ROUTINE_MSG_REPLACE_BUF *pMsgReplaceBuf;
ROUTINE_MSG_INSERT_BUF *pMsgInsertBuf;
ROUTINE_MSG_REPLACE_NONE *pMsgReplaceNone;
ROUTINE_MSG_INSERT_NONE *pMsgInsertNone;
///////////////////////////////////////////////////////////////
PLUGIN_INIT_A InitA;
const PLUGIN_ROUTINES_A *const * CALLBACK InitA(
    const HUB_ROUTINES_A * pHubRoutines)
{
  if (!ROUTINE_IS_VALID(pHubRoutines, pMsgInsertVal) ||
      !ROUTINE_IS_VALID(pHubRoutines, pMsgReplaceBuf) ||
      !ROUTINE_IS_VALID(pHubRoutines, pMsgInsertBuf) ||
      !ROUTINE_IS_VALID(pHubRoutines, pMsgReplaceNone) ||
      !ROUTINE_IS_VALID(pHubRoutines, pMsgInsertNone) ||
      !ROUTINE_IS_VALID(pHubRoutines, pPortName) ||
      !ROUTINE_IS_VALID(pHubRoutines, pFilterName) ||
      !ROUTINE_IS_VALID(pHubRoutines, pTimerCreate) ||
      !ROUTINE_IS_VALID(pHubRoutines, pTimerSet) ||
      !ROUTINE_IS_VALID(pHubRoutines, pTimerDelete) ||
      !ROUTINE_IS_VALID(pHubRoutines, pFilterPort) ||
      !ROUTINE_IS_VALID(pHubRoutines, pGetArgInfo))
  {
    return NULL;
  }

  pMsgInsertVal = pHubRoutines->pMsgInsertVal;
  pMsgReplaceBuf = pHubRoutines->pMsgReplaceBuf;
  pMsgInsertBuf = pHubRoutines->pMsgInsertBuf;
  pMsgReplaceNone = pHubRoutines->pMsgReplaceNone;
  pMsgInsertNone = pHubRoutines->pMsgInsertNone;
  pPortName = pHubRoutines->pPortName;
  pFilterName = pHubRoutines->pFilterName;
  pTimerCreate = pHubRoutines->pTimerCreate;
  pTimerSet = pHubRoutines->pTimerSet;
  pTimerDelete = pHubRoutines->pTimerDelete;
  pFilterPort = pHubRoutines->pFilterPort;
  pGetArgInfo = pHubRoutines->pGetArgInfo;

  return plugins;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
