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
 * Revision 1.7  2008/11/24 12:37:00  vfrolov
 * Changed plugin API
 *
 * Revision 1.6  2008/11/13 07:47:48  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.5  2008/10/22 08:27:26  vfrolov
 * Added ability to set bytesize, parity and stopbits separately
 *
 * Revision 1.4  2008/10/16 09:24:23  vfrolov
 * Changed return type of ROUTINE_MSG_REPLACE_*() to BOOL
 *
 * Revision 1.3  2008/09/30 08:28:32  vfrolov
 * Added ability to control OUT1 and OUT2 pins
 * Added ability to get remote baud rate and line control settings
 * Added ability to set baud rate and line control
 * Added fallback to non escape mode
 *
 * Revision 1.2  2008/08/29 13:02:37  vfrolov
 * Added ESC_OPTS_MAP_EO2GO() and ESC_OPTS_MAP_GO2EO()
 *
 * Revision 1.1  2008/08/22 17:02:59  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "../plugins_api.h"
#include "../cncext.h"
///////////////////////////////////////////////////////////////
namespace FilterEscParse {
///////////////////////////////////////////////////////////////
#ifndef _DEBUG
  #define DEBUG_PARAM(par)
#else   /* _DEBUG */
  #define DEBUG_PARAM(par) par
#endif  /* _DEBUG */
///////////////////////////////////////////////////////////////
static ROUTINE_MSG_INSERT_VAL *pMsgInsertVal = NULL;
static ROUTINE_MSG_INSERT_NONE *pMsgInsertNone = NULL;
static ROUTINE_MSG_REPLACE_NONE *pMsgReplaceNone = NULL;
static ROUTINE_MSG_INSERT_BUF *pMsgInsertBuf = NULL;
static ROUTINE_MSG_REPLACE_BUF *pMsgReplaceBuf = NULL;
static ROUTINE_PORT_NAME_A *pPortName = NULL;
static ROUTINE_FILTER_NAME_A *pFilterName = NULL;
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
class EscParse {
  public:
    EscParse(BYTE _escapeChar)
      : escMode(FALSE),
        intercepted_options(0),
        escapeChar(_escapeChar),
        maskMst(0),
        maskLsr(0),
        _options(0)
    {
      Reset();
    }

    HUB_MSG *Convert(HUB_MSG *pMsg);

    DWORD Options() const {
      return _options;
    }

    void OptionsDel(DWORD opts) {
      _options &= ~opts;
      maskMst = GO_O2V_MODEM_STATUS(_options);
      maskLsr = GO_O2V_LINE_STATUS(_options);
    }

    void OptionsAdd(DWORD opts) {
      _options |= opts;
      maskMst = GO_O2V_MODEM_STATUS(_options);
      maskLsr = GO_O2V_LINE_STATUS(_options);
    }

    BOOL escMode;
    DWORD intercepted_options;

  private:
    void Reset() { state = subState = 0; }
    HUB_MSG *Flush(HUB_MSG *pMsg);

    BYTE escapeChar;
    BYTE maskMst;
    BYTE maskLsr;
    int state;
    BYTE code;
    int subState;
    BYTE data[sizeof(ULONG)];
    basic_string<BYTE> line_data;

    DWORD _options;
};

HUB_MSG *EscParse::Flush(HUB_MSG *pMsg)
{
  if (!line_data.empty()) {
    pMsg = pMsgInsertBuf(pMsg,
                         HUB_MSG_TYPE_LINE_DATA,
                         line_data.data(),
                         (DWORD)line_data.size());

    line_data.clear();
  }

  return pMsg;
}

HUB_MSG *EscParse::Convert(HUB_MSG *pMsg)
{
  _ASSERTE(pMsg->type == HUB_MSG_TYPE_LINE_DATA);

  if (!escMode)
    return pMsg;

  DWORD len = pMsg->u.buf.size;
  basic_string<BYTE> org(pMsg->u.buf.pBuf, len);
  const BYTE *pBuf = org.data();

  // discard original data from the stream
  if (!pMsgReplaceBuf(pMsg, HUB_MSG_TYPE_LINE_DATA, NULL, 0))
    return NULL;

  for (; len ; len--) {
    BYTE ch = *pBuf++;

    switch (state) {
      case 0:
        break;
      case 1:
        code = ch;
        state++;
      case 2:
        switch (code) {
          case SERIAL_LSRMST_ESCAPE:
            line_data.append(&escapeChar, 1);
            Reset();
            break;
          case SERIAL_LSRMST_LSR_DATA:
          case SERIAL_LSRMST_LSR_NODATA:
            _ASSERTE(subState >= 0 && (subState <= 1 || (code == SERIAL_LSRMST_LSR_DATA && subState <= 2)));

            switch (subState) {
              case 0:
                subState++;
                break;
              case 1:
                data[0] = ch;
                if (code == SERIAL_LSRMST_LSR_DATA) {
                  subState++;
                  break;
                }
              case 2:
                if (Options() & GO_BREAK_STATUS) {
                  pMsg = Flush(pMsg);
                  if (!pMsg)
                    return NULL;
                  pMsg = pMsgInsertVal(pMsg, HUB_MSG_TYPE_BREAK_STATUS, (data[0] & LINE_STATUS_BI) != 0);
                  if (!pMsg)
                    return NULL;
                }
                if (data[0] & maskLsr) {
                  pMsg = Flush(pMsg);
                  if (!pMsg)
                    return NULL;
                  pMsg = pMsgInsertVal(pMsg, HUB_MSG_TYPE_LINE_STATUS, data[0] | VAL2MASK(maskLsr));
                  if (!pMsg)
                    return NULL;
                }
                if (subState == 2 && ((data[0] & LINE_STATUS_BI) == 0 || ch != 0)) {
                  // insert error character if it is not part of break
                  line_data.append(&ch, 1);
                }
                Reset();
                break;
              default:
                cerr << "ERROR: SERIAL_LSRMST_LSR_*DATA subState=" << subState << endl;
                Reset();
            }
            break;
          case SERIAL_LSRMST_MST:
            _ASSERTE(subState >= 0 && subState <= 1);

            if (subState == 0) {
              subState++;
            } else if (subState == 1) {
              if (maskMst) {
                pMsg = Flush(pMsg);
                if (!pMsg)
                  return NULL;
                pMsg = pMsgInsertVal(pMsg, HUB_MSG_TYPE_MODEM_STATUS, ch | VAL2MASK(maskMst));
                if (!pMsg)
                  return NULL;
              }
              Reset();
            } else {
              cerr << "ERROR: SERIAL_LSRMST_MST subState=" << subState << endl;
              Reset();
            }
            break;
          case C0CE_INSERT_RBR:
            _ASSERTE(subState >= 0 && subState < (sizeof(ULONG) + 1));

            if (subState == 0) {
              subState++;
            } else if (subState >= 1 && subState < (sizeof(ULONG) + 1)) {
              data[subState - 1] = ch;
              if (subState < sizeof(ULONG)) {
                subState++;
              } else {
                if (Options() & GO_RBR_STATUS) {
                  pMsg = Flush(pMsg);
                  if (!pMsg)
                    return NULL;
                  pMsg = pMsgInsertVal(pMsg, HUB_MSG_TYPE_RBR_STATUS, *(ULONG *)data);
                  if (!pMsg)
                    return NULL;
                }
                Reset();
              }
            } else {
              cerr << "ERROR: C0CE_INSERT_RBR subState=" << subState << endl;
              Reset();
            }
            break;
          case C0CE_INSERT_RLC:
            _ASSERTE(subState >= 0 && subState <= 3);

            if (subState == 0) {
              subState++;
            }  else if (subState >= 1 && subState <= 3) {
              data[subState - 1] = ch;
              if (subState < 3) {
                subState++;
              } else {
                if (Options() & GO_RLC_STATUS) {
                  pMsg = Flush(pMsg);
                  if (!pMsg)
                    return NULL;
                  pMsg = pMsgInsertVal(pMsg, HUB_MSG_TYPE_RLC_STATUS,
                      (VAL2LC_BYTESIZE(data[0])|LC_MASK_BYTESIZE
                      |VAL2LC_PARITY(data[1])|LC_MASK_PARITY
                      |VAL2LC_STOPBITS(data[2])|LC_MASK_STOPBITS));
                  if (!pMsg)
                    return NULL;
                }
                Reset();
              }
            } else {
              cerr << "ERROR: C0CE_INSERT_RLC subState=" << subState << endl;
              _ASSERTE(FALSE);
              Reset();
            }
            break;
          default:
            cerr << "ERROR: SERIAL_LSRMST_" << (WORD)code << " subState=" << subState << endl;
            Reset();
        }
        continue;

      default:
        cerr << "ERROR: state=" << state << endl;
        Reset();
    }

    if (ch == escapeChar) {
      state = 1;
      continue;
    }

    line_data.append(&ch, 1);
  }

  return Flush(pMsg);
}
///////////////////////////////////////////////////////////////
class Filter : public Valid {
  public:
    Filter(int argc, const char *const argv[]);
    EscParse *GetEscParse(HMASTERPORT hPort);

    void SetFilterName(const char *_pName) { pName = _pName; }
    const char *FilterName() const { return pName; }

    BOOL requestEscMode;
    BYTE escapeChar;
    DWORD acceptableOptions;

  private:
    const char *pName;

    typedef map<HMASTERPORT, EscParse*> PortsMap;
    typedef pair<HMASTERPORT, EscParse*> PortPair;

    PortsMap portsMap;
};

Filter::Filter(int argc, const char *const argv[])
  : requestEscMode(TRUE),
    escapeChar(0xFF),
    acceptableOptions(
      GO_RBR_STATUS |
      GO_RLC_STATUS |
      GO_BREAK_STATUS |
      GO_V2O_MODEM_STATUS(-1) |
      GO_V2O_LINE_STATUS(-1)),
    pName(NULL)
{
  for (const char *const *pArgs = &argv[1] ; argc > 1 ; pArgs++, argc--) {
    const char *pArg = GetParam(*pArgs, "--");

    if (!pArg) {
      cerr << "Unknown option " << *pArgs << endl;
      Invalidate();
      continue;
    }

    const char *pParam;

    if ((pParam = GetParam(pArg, "request-esc-mode=")) != NULL) {
      if (_stricmp(pParam, "yes") == 0) {
        requestEscMode = TRUE;
      }
      else
      if (_stricmp(pParam, "no") == 0) {
        requestEscMode = FALSE;
      }
      else {
        cerr << "Unknown value --" << pArg << endl;
        Invalidate();
      }
    } else {
      cerr << "Unknown option --" << pArg << endl;
      Invalidate();
    }
  }
}

EscParse *Filter::GetEscParse(HMASTERPORT hPort)
{
  PortsMap::iterator iPair = portsMap.find(hPort);

  if (iPair == portsMap.end()) {
      portsMap.insert(PortPair(hPort, NULL));

      iPair = portsMap.find(hPort);

      if (iPair == portsMap.end())
        return NULL;
  }

  if (!iPair->second)
    iPair->second = new EscParse(escapeChar);

  return iPair->second;
}
///////////////////////////////////////////////////////////////
static PLUGIN_TYPE CALLBACK GetPluginType()
{
  return PLUGIN_TYPE_FILTER;
}
///////////////////////////////////////////////////////////////
static const PLUGIN_ABOUT_A about = {
  sizeof(PLUGIN_ABOUT_A),
  "escparse",
  "Copyright (c) 2008 Vyacheslav Frolov",
  "GNU General Public License",
  "Escaped data stream parsing filter",
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
  << "  --request-esc-mode={yes|no}  - send request to driver to produce escaped data" << endl
  << "                                 (yes by default)." << endl
  << endl
  << "Examples:" << endl
  << "  " << pProgPath << " --load=,,_END_" << endl
  << "      --create-filter=pinmap" << endl
  << "      --create-filter=escparse" << endl
  << "      --add-filters=0,1:pinmap,escparse" << endl
  << "      COM1" << endl
  << "      COM2" << endl
  << "      _END_" << endl
  << "    - transfer data and signals between COM1 and COM2." << endl
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
    HUB_MSG ** DEBUG_PARAM(ppEchoMsg))
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(pInMsg != NULL);
  _ASSERTE(ppEchoMsg != NULL);
  _ASSERTE(*ppEchoMsg == NULL);

  switch (pInMsg->type) {
    case HUB_MSG_TYPE_COUNT_REPEATS:
      if (pInMsg->u.pv.val == HUB_MSG_TYPE_GET_IN_OPTS) {
        // we need it twice to
        //   - get interceptable options from subsequent filters
        //   - accept the received options and request the escape mode

        (*pInMsg->u.pv.pVal)++;
      }
      break;
    case HUB_MSG_TYPE_GET_IN_OPTS: {
      EscParse *pEscParse = ((Filter *)hFilter)->GetEscParse(hFromPort);

      if (!pEscParse)
        return FALSE;

      // if the subsequent filters require interceptable options then
      // accept the received options and request the escape mode

      pEscParse->OptionsAdd((pEscParse->intercepted_options & ((Filter *)hFilter)->acceptableOptions));

      if (((Filter *)hFilter)->requestEscMode) {
        if (pEscParse->Options() && (pInMsg->u.pv.val & GO_ESCAPE_MODE)) {
          pEscParse->escMode = TRUE;
          *pInMsg->u.pv.pVal |= GO_ESCAPE_MODE; // request the escape mode
        }
      } else {
        pEscParse->escMode = TRUE;
      }

      // get interceptable options from subsequent filters separately

      DWORD interceptable_options = (pInMsg->u.pv.val & (GO_ESCAPE_MODE | ((Filter *)hFilter)->acceptableOptions));

      pInMsg->u.pv.val &= ~interceptable_options;

      pInMsg = pMsgInsertNone(pInMsg, HUB_MSG_TYPE_EMPTY);

      if (pInMsg) {
        pInMsg->type = HUB_MSG_TYPE_GET_IN_OPTS;
        pInMsg->u.pv.pVal = &pEscParse->intercepted_options;
        pInMsg->u.pv.val = interceptable_options;
      }

      break;
    }
    case HUB_MSG_TYPE_GET_ESC_OPTS: {
      EscParse *pEscParse = ((Filter *)hFilter)->GetEscParse(hFromPort);

      if (!pEscParse)
        return FALSE;

      *pInMsg->u.pv.pVal = ESC_OPTS_MAP_GO2EO(pEscParse->Options()) |
                           ESC_OPTS_V2O_ESCCHAR(((Filter *)hFilter)->escapeChar);

      // hide this message from subsequent filters
      if (!pMsgReplaceNone(pInMsg, HUB_MSG_TYPE_EMPTY))
        return FALSE;

      break;
    }
    case HUB_MSG_TYPE_FAIL_ESC_OPTS: {
      EscParse *pEscParse = ((Filter *)hFilter)->GetEscParse(hFromPort);

      if (!pEscParse)
        return FALSE;

      DWORD fail_options = (pInMsg->u.pv.val & ESC_OPTS_MAP_GO2EO(pEscParse->Options()));

      if (fail_options) {
        cerr << pPortName(hFromPort)
             << " WARNING: Requested by filter " << ((Filter *)hFilter)->FilterName()
             << " escape mode option(s) 0x" << hex << fail_options << dec
             << " not accepted (will try non escape mode option(s))" << endl;

        pEscParse->OptionsDel(ESC_OPTS_MAP_EO2GO(fail_options));
        *pInMsg->u.pv.pVal |= ESC_OPTS_MAP_EO2GO(fail_options);
      }

      // hide this message from subsequent filters
      if (!pMsgReplaceNone(pInMsg, HUB_MSG_TYPE_EMPTY))
        return FALSE;

      break;
    }
    case HUB_MSG_TYPE_FAIL_IN_OPTS: {
      EscParse *pEscParse = ((Filter *)hFilter)->GetEscParse(hFromPort);

      if (!pEscParse)
        return FALSE;

      if ((pInMsg->u.val & GO_ESCAPE_MODE) && ((Filter *)hFilter)->requestEscMode) {
        cerr << pPortName(hFromPort)
             << " WARNING: Requested by filter " << ((Filter *)hFilter)->FilterName()
             << " option ESCAPE_MODE not accepted" << endl;

        pEscParse->escMode = FALSE;
        pEscParse->OptionsDel((DWORD)-1);
      }

      pInMsg->u.val &= ~GO_ESCAPE_MODE; // hide from subsequent filters

      break;
    }
    case HUB_MSG_TYPE_LINE_DATA: {
      _ASSERTE(pInMsg->u.buf.pBuf != NULL || pInMsg->u.buf.size == 0);

      if (pInMsg->u.buf.size == 0)
        return TRUE;

      EscParse *pEscParse = ((Filter *)hFilter)->GetEscParse(hFromPort);

      if (!pEscParse)
        return FALSE;

      pInMsg = pEscParse->Convert(pInMsg);

      break;
    }
    case HUB_MSG_TYPE_MODEM_STATUS: {
      EscParse *pEscParse = ((Filter *)hFilter)->GetEscParse(hFromPort);

      if (!pEscParse)
        return FALSE;

      // discard any status settings controlled by this filter
      pInMsg->u.val &= ~VAL2MASK(GO_O2V_MODEM_STATUS(pEscParse->Options()));
      break;
    }
    case HUB_MSG_TYPE_LINE_STATUS: {
      EscParse *pEscParse = ((Filter *)hFilter)->GetEscParse(hFromPort);

      if (!pEscParse)
        return FALSE;

      // discard any status settings controlled by this filter
      pInMsg->u.val &= ~VAL2MASK(GO_O2V_LINE_STATUS(pEscParse->Options()));
      break;
    }
    case HUB_MSG_TYPE_RBR_STATUS: {
      EscParse *pEscParse = ((Filter *)hFilter)->GetEscParse(hFromPort);

      if (!pEscParse)
        return FALSE;

      if (pEscParse->Options() & GO_RBR_STATUS) {
        // discard any status settings controlled by this filter
        if (!pMsgReplaceNone(pInMsg, HUB_MSG_TYPE_EMPTY))
          return FALSE;
      }
      break;
    }
    case HUB_MSG_TYPE_RLC_STATUS: {
      EscParse *pEscParse = ((Filter *)hFilter)->GetEscParse(hFromPort);

      if (!pEscParse)
        return FALSE;

      if (pEscParse->Options() & GO_RLC_STATUS) {
        // discard any status settings controlled by this filter
        if (!pMsgReplaceNone(pInMsg, HUB_MSG_TYPE_EMPTY))
          return FALSE;
      }
      break;
    }
    case HUB_MSG_TYPE_BREAK_STATUS: {
      EscParse *pEscParse = ((Filter *)hFilter)->GetEscParse(hFromPort);

      if (!pEscParse)
        return FALSE;

      if (pEscParse->Options() & GO_BREAK_STATUS) {
        // discard any status settings controlled by this filter
        if (!pMsgReplaceNone(pInMsg, HUB_MSG_TYPE_EMPTY))
          return FALSE;
      }
      break;
    }
  }

  return pInMsg != NULL;
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
  NULL,           // OutMethod
};

static const PLUGIN_ROUTINES_A *const plugins[] = {
  (const PLUGIN_ROUTINES_A *)&routines,
  NULL
};
///////////////////////////////////////////////////////////////
PLUGIN_INIT_A InitA;
const PLUGIN_ROUTINES_A *const * CALLBACK InitA(
    const HUB_ROUTINES_A * pHubRoutines)
{
  if (!ROUTINE_IS_VALID(pHubRoutines, pMsgInsertVal) ||
      !ROUTINE_IS_VALID(pHubRoutines, pMsgInsertNone) ||
      !ROUTINE_IS_VALID(pHubRoutines, pMsgReplaceNone) ||
      !ROUTINE_IS_VALID(pHubRoutines, pMsgInsertBuf) ||
      !ROUTINE_IS_VALID(pHubRoutines, pMsgReplaceBuf) ||
      !ROUTINE_IS_VALID(pHubRoutines, pPortName) ||
      !ROUTINE_IS_VALID(pHubRoutines, pFilterName))
  {
    return NULL;
  }

  pMsgInsertVal = pHubRoutines->pMsgInsertVal;
  pMsgInsertNone = pHubRoutines->pMsgInsertNone;
  pMsgReplaceNone = pHubRoutines->pMsgReplaceNone;
  pMsgInsertBuf = pHubRoutines->pMsgInsertBuf;
  pMsgReplaceBuf = pHubRoutines->pMsgReplaceBuf;
  pPortName = pHubRoutines->pPortName;
  pFilterName = pHubRoutines->pFilterName;

  return plugins;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
