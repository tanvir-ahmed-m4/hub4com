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
class Valid {
  public:
    Valid() : isValid(TRUE) {}
    void Invalidate() { isValid = FALSE; }
    BOOL IsValid() const { return isValid; }
  private:
    BOOL isValid;
};
///////////////////////////////////////////////////////////////
class State {
  public:
    State()
    : pTelnetProtocol(NULL),
      pComPort(NULL),
      soMask(0),
      pinState(0),
      br(0),
      lc(0)
    {
      for (int iGo = 0 ; iGo < sizeof(goMask)/sizeof(goMask[0]) ; iGo++)
        goMask[iGo] = 0;
    }

    void SetProtocol(TelnetProtocol *_pTelnetProtocol) {
      pComPort = NULL;
      if (pTelnetProtocol)
        delete pTelnetProtocol;
      pTelnetProtocol = _pTelnetProtocol;
    }

    void SetOption(TelnetOptionComPort *_pComPort) {
      pComPort = _pComPort;
    }

    TelnetProtocol *pTelnetProtocol;
    TelnetOptionComPort *pComPort;
    DWORD soMask;
    DWORD goMask[2];
    WORD pinState;
    DWORD br;
    DWORD lc;
};
///////////////////////////////////////////////////////////////
class Filter : public Valid {
  public:
    Filter(int argc, const char *const argv[]);
    State *GetState(HMASTERPORT hPort);
    TelnetProtocol *CreateProtocol(State *pState, const char *pName);
    BOOL DelProtocol(State *pState);

    void SetFilterName(const char *_pName) { pName = _pName; }
    const char *FilterName() const { return pName; }

    TelnetProtocol *GetProtocol(HMASTERPORT hPort) {
      State *pState = GetState(hPort);

      if (!pState)
        return NULL;

      return pState->pTelnetProtocol;
    }

    DWORD soMask;
    DWORD goMask[2];

  private:
    const char *pName;
    string terminalType;
    BOOL suppressEcho;

    enum {
      comport_no,
      comport_client,
      comport_server
    } comport;

    typedef map<HMASTERPORT, State*> PortsMap;
    typedef pair<HMASTERPORT, State*> PortPair;

    PortsMap portsMap;
};

Filter::Filter(int argc, const char *const argv[])
  : pName(NULL),
    terminalType("UNKNOWN"),
    suppressEcho(FALSE),
    comport(comport_no)
{
  for (const char *const *pArgs = &argv[1] ; argc > 1 ; pArgs++, argc--) {
    const char *pArg = GetParam(*pArgs, "--");

    if (!pArg) {
      cerr << "Unknown option " << *pArgs << endl;
      Invalidate();
      continue;
    }

    const char *pParam;

    if ((pParam = GetParam(pArg, "terminal=")) != NULL) {
      terminalType = pParam;
    }
    else
    if ((pParam = GetParam(pArg, "comport=")) != NULL) {
      switch (tolower(*pParam)) {
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
          cerr << "Unknown value in " << pArg << endl;
          Invalidate();
      }
    }
    else
    if ((pParam = GetParam(pArg, "suppress-echo=")) != NULL) {
      switch (tolower(*pParam)) {
        case 'y':
          suppressEcho = TRUE;
          break;
        case 'n':
          suppressEcho = FALSE;
          break;
        default:
          cerr << "Unknown value in " << pArg << endl;
          Invalidate();
      }
    }
    else {
      cerr << "Unknown option " << pArg << endl;
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

State *Filter::GetState(HMASTERPORT hPort)
{
  PortsMap::iterator iPair = portsMap.find(hPort);

  if (iPair == portsMap.end()) {
      portsMap.insert(PortPair(hPort, NULL));

      iPair = portsMap.find(hPort);

      if (iPair == portsMap.end())
        return NULL;
  }

  if (!iPair->second)
    iPair->second = new State();

  return iPair->second;
}

TelnetProtocol *Filter::CreateProtocol(State *pState, const char *pName)
{
  _ASSERTE(pState->pTelnetProtocol == NULL);

  TelnetProtocol *pTelnetProtocol = new TelnetProtocol(pName);

  if (!pTelnetProtocol)
    return NULL;

  pState->SetProtocol(pTelnetProtocol);

  TelnetOption *pTo;

  if (!terminalType.empty()) {
    pTo = new TelnetOptionTerminalType(*pTelnetProtocol, terminalType.c_str());

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

    if (suppressEcho)
      pTo->LocalWill();
  }

  pTo = new TelnetOption(*pTelnetProtocol, 3 /*SUPPRESS-GO-AHEAD*/);

  if (pTo) {
    pTo->RemoteMay();
    pTo->LocalCan();
  }

  if (comport == comport_client) {
    pTo = new TelnetOptionComPortClient(*pTelnetProtocol, pState->goMask, pState->soMask);

    if (pTo) {
      pTo->LocalWill();
      pState->SetOption((TelnetOptionComPortClient *)pTo);
    }
  }
  else
  if (comport == comport_server) {
    pTo = new TelnetOptionComPortServer(*pTelnetProtocol, pState->goMask, pState->soMask);

    if (pTo) {
      pTo->RemoteDo();
      pState->SetOption((TelnetOptionComPortServer *)pTo);
    }
  }

  return pTelnetProtocol;
}

BOOL Filter::DelProtocol(State *pState)
{
  _ASSERTE(pState->pTelnetProtocol != NULL);

  if (!pState->pTelnetProtocol)
    return FALSE;

  pState->SetProtocol(NULL);

  return TRUE;
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
    HCONFIG /*hConfig*/,
    int argc,
    const char *const argv[])
{
  Filter *pFilter = new Filter(argc, argv);

  if (!pFilter)
    return NULL;

  if (!pFilter->IsValid()) {
    delete pFilter;
    return NULL;
  }

  return (HFILTER)pFilter;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK Init(
    HFILTER hFilter,
    HMASTERFILTER hMasterFilter)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(hMasterFilter != NULL);

  ((Filter *)hFilter)->SetFilterName(pFilterName(hMasterFilter));

  return TRUE;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK InMethod(
    HFILTER hFilter,
    HMASTERPORT hFromPort,
    HUB_MSG *pInMsg,
    HUB_MSG **ppEchoMsg)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(hFromPort != NULL);
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

      State *pState = ((Filter *)hFilter)->GetState(hFromPort);

      if (!pState)
        return FALSE;

      // get interceptable options from subsequent filters separately

      DWORD interceptable_options = (pInMsg->u.pv.val & ((Filter *)hFilter)->goMask[iGo == 0 ? 0 : 1]);

      pInMsg->u.pv.val &= ~interceptable_options;

      pInMsg = pMsgInsertNone(pInMsg, HUB_MSG_TYPE_EMPTY);

      if (!pInMsg)
        return FALSE;

      pInMsg->type = HUB_MSG_TYPE_GET_IN_OPTS;
      pInMsg->u.pv.pVal = &pState->goMask[iGo == 0 ? 0 : 1];
      _ASSERTE((interceptable_options & GO_I2O(-1)) == 0);
      pInMsg->u.pv.val = interceptable_options | GO_I2O(iGo);

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_LINE_DATA): {
      _ASSERTE(pInMsg->u.buf.pBuf != NULL || pInMsg->u.buf.size == 0);

      if (pInMsg->u.buf.size == 0)
        break;

      TelnetProtocol *pTelnetProtocol = ((Filter *)hFilter)->GetProtocol(hFromPort);

      if (!pTelnetProtocol)
        return FALSE;

      if (!pTelnetProtocol->Decode(pInMsg))
        return FALSE;

      pTelnetProtocol->FlushEncodedStream(ppEchoMsg);

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_CONNECT): {
      State *pState = ((Filter *)hFilter)->GetState(hFromPort);

      if (!pState)
        return FALSE;

      if (!pInMsg->u.val) {
        if (pState->pComPort) {
          if (pState->goMask[1] & GO1_BREAK_STATUS) {
            pInMsg = pMsgInsertVal(pInMsg, HUB_MSG_TYPE_BREAK_STATUS, FALSE);

            if (!pInMsg)
              return FALSE;
          }

          if (pState->goMask[1] & GO1_V2O_MODEM_STATUS(-1)) {
            pInMsg = pMsgInsertVal(pInMsg, HUB_MSG_TYPE_MODEM_STATUS,
                 0 | VAL2MASK(GO1_O2V_MODEM_STATUS(pState->goMask[1])));

            if (!pInMsg)
              return FALSE;
          }
        }

        if (!((Filter *)hFilter)->DelProtocol(pState))
          return FALSE;

        break;
      }

      TelnetProtocol *pTelnetProtocol = ((Filter *)hFilter)->CreateProtocol(pState, pPortName(hFromPort));

      if (!pTelnetProtocol)
        return FALSE;

      pTelnetProtocol->Start();

      if (pState->pComPort) {
        // Set current state

        if ((pState->soMask & SO_SET_BR) && pState->br != 0)
          pState->pComPort->SetBR(pState->br);

        if (pState->soMask & SO_SET_LC)
          pState->pComPort->SetLC(pState->lc);

        if (pState->soMask & SO_V2O_PIN_STATE(SPS_V2P_MST(-1)))
          pState->pComPort->NotifyMST(SPS_P2V_MST(pState->pinState));

        if (pState->soMask & SO_V2O_PIN_STATE(SPS_V2P_MCR(-1)))
          pState->pComPort->SetMCR(SPS_P2V_MCR(pState->pinState), SPS_P2V_MCR(SO_O2V_PIN_STATE(pState->soMask)));

        if (pState->soMask & SO_V2O_PIN_STATE(PIN_STATE_BREAK))
          pState->pComPort->SetBreak((pState->pinState & PIN_STATE_BREAK) != 0);

        if (pState->soMask & SO_V2O_LINE_STATUS(-1))
          pState->pComPort->NotifyLSR(0);
      }

      pTelnetProtocol->FlushEncodedStream(ppEchoMsg);

      break;
    }
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK OutMethod(
    HFILTER hFilter,
    HMASTERPORT DEBUG_PARAM(hFromPort),
    HMASTERPORT hToPort,
    HUB_MSG *pOutMsg)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(hFromPort != NULL);
  _ASSERTE(hToPort != NULL);
  _ASSERTE(pOutMsg != NULL);

  switch (HUB_MSG_T2N(pOutMsg->type)) {
    case HUB_MSG_T2N(HUB_MSG_TYPE_SET_OUT_OPTS): {
      DWORD soMask = (pOutMsg->u.val & ((Filter *)hFilter)->soMask);

      // discard supported options
      pOutMsg->u.val &= ~((Filter *)hFilter)->soMask;

      State *pState = ((Filter *)hFilter)->GetState(hToPort);

      if (!pState)
        return FALSE;

      pState->soMask |= soMask;

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_SET_BR): {
      State *pState = ((Filter *)hFilter)->GetState(hToPort);

      if (!pState)
        return FALSE;

      _ASSERTE(((pState->soMask | ~((Filter *)hFilter)->soMask) & SO_SET_BR) != 0);

      if (pState->soMask & SO_SET_BR) {
        DWORD val = pOutMsg->u.val;

        // discard messages for supported options
        if (!pMsgReplaceNone(pOutMsg, HUB_MSG_TYPE_EMPTY))
          return FALSE;

        if (pState->br != val) {
          pState->br = val;

          if (pState->pComPort) {
            pState->pComPort->SetBR(pState->br);
            _ASSERTE(pState->pTelnetProtocol != NULL);
            pState->pTelnetProtocol->FlushEncodedStream(&pOutMsg);
          }
        }
      }

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_SET_LC): {
      _ASSERTE((pOutMsg->u.val & ~(VAL2LC_BYTESIZE(-1)|LC_MASK_BYTESIZE
                                  |VAL2LC_PARITY(-1)|LC_MASK_PARITY
                                  |VAL2LC_STOPBITS(-1)|LC_MASK_STOPBITS)) == 0);

      State *pState = ((Filter *)hFilter)->GetState(hToPort);

      if (!pState)
        return FALSE;

      _ASSERTE(((pState->soMask | ~((Filter *)hFilter)->soMask) & SO_SET_LC) != 0);

      if (pState->soMask & SO_SET_LC) {
        DWORD val = pOutMsg->u.val;

        // discard messages for supported options
        if (!pMsgReplaceNone(pOutMsg, HUB_MSG_TYPE_EMPTY))
          return FALSE;

        if ((val & LC_MASK_BYTESIZE) == 0) {
          _ASSERTE((val & VAL2LC_BYTESIZE(-1)) == 0);
          val |= (pState->lc & (VAL2LC_BYTESIZE(-1)|LC_MASK_BYTESIZE));
        }

        if ((val & LC_MASK_PARITY) == 0) {
          _ASSERTE((val & VAL2LC_PARITY(-1)) == 0);
          val |= (pState->lc & (VAL2LC_PARITY(-1)|LC_MASK_PARITY));
        }

        if ((val & LC_MASK_STOPBITS) == 0) {
          _ASSERTE((val & VAL2LC_STOPBITS(-1)) == 0);
          val |= (pState->lc & (VAL2LC_STOPBITS(-1)|LC_MASK_STOPBITS));
        }

        if (pState->lc != val) {
          pState->lc = val;

          if (pState->pComPort) {
            pState->pComPort->SetLC(pState->lc);
            _ASSERTE(pState->pTelnetProtocol != NULL);
            pState->pTelnetProtocol->FlushEncodedStream(&pOutMsg);
          }
        }
      }

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_SET_PIN_STATE): {
      State *pState = ((Filter *)hFilter)->GetState(hToPort);

      if (!pState)
        return FALSE;

      _ASSERTE(((pState->soMask | ~((Filter *)hFilter)->soMask) & SO_V2O_PIN_STATE(-1)) != 0);

      WORD mask = MASK2VAL(pOutMsg->u.val) & SO_O2V_PIN_STATE(pState->soMask);

      if (!mask)
        break;

      WORD pinState = (WORD)pOutMsg->u.val;

      // discard settings for supported options
      pOutMsg->u.val &= ~VAL2MASK(mask);

      pinState = ((pinState & mask) | (pState->pinState & ~mask));
      mask = pState->pinState ^ pinState;

      if (mask) {
        if (pState->pComPort) {
          if (SPS_P2V_MST(mask))
            pState->pComPort->NotifyMST(SPS_P2V_MST(pinState));

          if (SPS_P2V_MCR(mask))
            pState->pComPort->SetMCR(SPS_P2V_MCR(pinState), SPS_P2V_MCR(mask));

          if (mask & PIN_STATE_BREAK)
            pState->pComPort->SetBreak((pinState & PIN_STATE_BREAK) != 0);

          _ASSERTE(pState->pTelnetProtocol != NULL);
          pState->pTelnetProtocol->FlushEncodedStream(&pOutMsg);
        }

        pState->pinState = pinState;
      }

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_SET_LSR): {
      State *pState = ((Filter *)hFilter)->GetState(hToPort);

      if (!pState)
        return FALSE;

      _ASSERTE(((pState->soMask | ~((Filter *)hFilter)->soMask) & SO_V2O_LINE_STATUS(-1)) != 0);

      BYTE lsr;

      lsr = (BYTE)pOutMsg->u.val & (BYTE)MASK2VAL(pOutMsg->u.val) & SO_O2V_LINE_STATUS(pState->soMask);

      // discard settings for supported options
      pOutMsg->u.val &= ~VAL2MASK(SO_O2V_LINE_STATUS(pState->soMask));

      if (!lsr)
        break;

      if (pState->pComPort) {
        pState->pComPort->NotifyLSR(lsr);
        _ASSERTE(pState->pTelnetProtocol != NULL);
        pState->pTelnetProtocol->FlushEncodedStream(&pOutMsg);
      }

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_PURGE_TX): {
      State *pState = ((Filter *)hFilter)->GetState(hToPort);

      if (!pState)
        return FALSE;

      _ASSERTE(((pState->soMask | ~((Filter *)hFilter)->soMask) & SO_PURGE_TX) != 0);

      if (pState->soMask & SO_PURGE_TX) {
        _ASSERTE(pOutMsg->u.val == (BYTE)pOutMsg->u.val);

        // discard messages for supported options
        if (!pMsgReplaceNone(pOutMsg, HUB_MSG_TYPE_EMPTY))
          return FALSE;

        if (pState->pComPort) {
          pState->pComPort->PurgeTx();
          _ASSERTE(pState->pTelnetProtocol != NULL);
          pState->pTelnetProtocol->FlushEncodedStream(&pOutMsg);
        }
      }

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_ADD_XOFF_XON): {
      State *pState = ((Filter *)hFilter)->GetState(hToPort);

      if (!pState)
        break;

      if (pState->pComPort) {
        pState->pComPort->AddXoffXon(pOutMsg->u.val);
        _ASSERTE(pState->pTelnetProtocol != NULL);
        pState->pTelnetProtocol->FlushEncodedStream(&pOutMsg);
      }

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_LINE_DATA): {
      _ASSERTE(pOutMsg->u.buf.pBuf != NULL || pOutMsg->u.buf.size == 0);

      if (pOutMsg->u.buf.size == 0)
        break;

      TelnetProtocol *pTelnetProtocol = ((Filter *)hFilter)->GetProtocol(hToPort);

      if (!pTelnetProtocol)
        return FALSE;

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
  Init,
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
      !ROUTINE_IS_VALID(pHubRoutines, pFilterName))
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

  return plugins;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
