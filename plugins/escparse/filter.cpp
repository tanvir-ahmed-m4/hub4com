/*
 * $Id$
 *
 * Copyright (c) 2008-2009 Vyacheslav Frolov
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
 * Revision 1.11  2009/02/02 15:21:42  vfrolov
 * Optimized filter's API
 *
 * Revision 1.10  2008/12/22 09:40:45  vfrolov
 * Optimized message switching
 *
 * Revision 1.9  2008/12/18 16:50:51  vfrolov
 * Extended the number of possible IN options
 *
 * Revision 1.8  2008/11/25 16:40:40  vfrolov
 * Added assert for port handle
 *
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
static ROUTINE_MSG_INSERT_VAL *pMsgInsertVal;
static ROUTINE_MSG_INSERT_NONE *pMsgInsertNone;
static ROUTINE_MSG_REPLACE_NONE *pMsgReplaceNone;
static ROUTINE_MSG_INSERT_BUF *pMsgInsertBuf;
static ROUTINE_MSG_REPLACE_BUF *pMsgReplaceBuf;
static ROUTINE_PORT_NAME_A *pPortName;
static ROUTINE_FILTER_NAME_A *pFilterName;
static ROUTINE_FILTERPORT *pFilterPort;
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
    State(HMASTERPORT hMasterPort)
      : pName(pPortName(hMasterPort)),
        escMode(FALSE),
        intercepted_options(0),
        maskMst(0),
        maskLsr(0),
        _options(0)
    {
      Reset();
    }

    HUB_MSG *Convert(BYTE escapeChar, HUB_MSG *pMsg);

    DWORD Options() const {
      return _options;
    }

    void OptionsDel(DWORD opts) {
      _options &= ~opts;
      maskMst = GO1_O2V_MODEM_STATUS(_options);
      maskLsr = GO1_O2V_LINE_STATUS(_options);
    }

    void OptionsAdd(DWORD opts) {
      _options |= opts;
      maskMst = GO1_O2V_MODEM_STATUS(_options);
      maskLsr = GO1_O2V_LINE_STATUS(_options);
    }

    const char *const pName;
    BOOL escMode;
    DWORD intercepted_options;

  private:
    void Reset() { state = subState = 0; }
    HUB_MSG *Flush(HUB_MSG *pMsg);

    BYTE maskMst;
    BYTE maskLsr;
    int state;
    BYTE code;
    int subState;
    BYTE data[sizeof(ULONG)];
    basic_string<BYTE> line_data;

    DWORD _options;
};

HUB_MSG *State::Flush(HUB_MSG *pMsg)
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

HUB_MSG *State::Convert(BYTE escapeChar, HUB_MSG *pMsg)
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
                if (Options() & GO1_BREAK_STATUS) {
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
                if (Options() & GO1_RBR_STATUS) {
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
                if (Options() & GO1_RLC_STATUS) {
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
    Filter(const char *pName, int argc, const char *const argv[]);

    const char *FilterName() const { return pName; }

    BOOL requestEscMode;
    BYTE escapeChar;
    DWORD acceptableOptions;

  private:
    const char *pName;
};

Filter::Filter(const char *_pName, int argc, const char *const argv[])
  : pName(_pName),
    requestEscMode(TRUE),
    escapeChar(0xFF),
    acceptableOptions(
      GO1_RBR_STATUS |
      GO1_RLC_STATUS |
      GO1_BREAK_STATUS |
      GO1_V2O_MODEM_STATUS(-1) |
      GO1_V2O_LINE_STATUS(-1))
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
    HUB_MSG ** DEBUG_PARAM(ppEchoMsg))
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(hFilterInstance != NULL);
  _ASSERTE(pInMsg != NULL);
  _ASSERTE(ppEchoMsg != NULL);
  _ASSERTE(*ppEchoMsg == NULL);

  switch (HUB_MSG_T2N(pInMsg->type)) {
    case HUB_MSG_T2N(HUB_MSG_TYPE_COUNT_REPEATS):
      if (HUB_MSG_T2N(pInMsg->u.pv.val) == HUB_MSG_T2N(HUB_MSG_TYPE_GET_IN_OPTS)) {
        // we need it twice to
        //   - get interceptable options from subsequent filters
        //   - accept the received options and request the escape mode

        (*pInMsg->u.pv.pVal)++;
      }
      break;
    case HUB_MSG_T2N(HUB_MSG_TYPE_GET_IN_OPTS): {
      int iGo = GO_O2I(pInMsg->u.pv.val);

      if (iGo == 0) {
        // if the subsequent filters require interceptable options then
        // accept the received options and request the escape mode

        ((State *)hFilterInstance)->OptionsAdd((((State *)hFilterInstance)->intercepted_options & ((Filter *)hFilter)->acceptableOptions));

        if (((Filter *)hFilter)->requestEscMode) {
          if (((State *)hFilterInstance)->Options() && (pInMsg->u.pv.val & GO0_ESCAPE_MODE)) {
            ((State *)hFilterInstance)->escMode = TRUE;
            *pInMsg->u.pv.pVal |= GO0_ESCAPE_MODE; // request the escape mode
          }
        } else {
          ((State *)hFilterInstance)->escMode = TRUE;
        }

        // the subsequent filters can't request the escape mode

        pInMsg->u.pv.val &= ~GO0_ESCAPE_MODE;
      }
      else
      if (iGo == 1) {
        // get interceptable options from subsequent filters separately

        DWORD interceptable_options = (pInMsg->u.pv.val & ((Filter *)hFilter)->acceptableOptions);

        pInMsg->u.pv.val &= ~interceptable_options;

        pInMsg = pMsgInsertNone(pInMsg, HUB_MSG_TYPE_EMPTY);

        if (!pInMsg)
          return FALSE;

        pInMsg->type = HUB_MSG_TYPE_GET_IN_OPTS;
        pInMsg->u.pv.pVal = &((State *)hFilterInstance)->intercepted_options;
        _ASSERTE((interceptable_options & GO_I2O(-1)) == 0);
        pInMsg->u.pv.val = interceptable_options | GO_I2O(iGo);
      }

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_GET_ESC_OPTS): {
      *pInMsg->u.pv.pVal = ESC_OPTS_MAP_GO1_2_EO(((State *)hFilterInstance)->Options()) |
                           ESC_OPTS_V2O_ESCCHAR(((Filter *)hFilter)->escapeChar);

      // hide this message from subsequent filters
      if (!pMsgReplaceNone(pInMsg, HUB_MSG_TYPE_EMPTY))
        return FALSE;

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_FAIL_ESC_OPTS): {
      DWORD fail_options = (pInMsg->u.pv.val & ESC_OPTS_MAP_GO1_2_EO(((State *)hFilterInstance)->Options()));

      if (fail_options) {
        cerr << ((State *)hFilterInstance)->pName
             << " WARNING: Requested by filter " << ((Filter *)hFilter)->FilterName()
             << " escape mode option(s) 0x" << hex << fail_options << dec
             << " not accepted (will try non escape mode option(s))" << endl;

        ((State *)hFilterInstance)->OptionsDel(ESC_OPTS_MAP_EO_2_GO1(fail_options));
        *pInMsg->u.pv.pVal |= ESC_OPTS_MAP_EO_2_GO1(fail_options);
      }

      // hide this message from subsequent filters
      if (!pMsgReplaceNone(pInMsg, HUB_MSG_TYPE_EMPTY))
        return FALSE;

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_FAIL_IN_OPTS): {
      if (GO_O2I(pInMsg->u.pv.val) != 0)
        break;

      if ((pInMsg->u.val & GO0_ESCAPE_MODE) && ((Filter *)hFilter)->requestEscMode) {
        cerr << ((State *)hFilterInstance)->pName
             << " WARNING: Requested by filter " << ((Filter *)hFilter)->FilterName()
             << " option ESCAPE_MODE not accepted" << endl;

        ((State *)hFilterInstance)->escMode = FALSE;
        ((State *)hFilterInstance)->OptionsDel((DWORD)-1);
      }

      pInMsg->u.val &= ~GO0_ESCAPE_MODE; // hide from subsequent filters

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_LINE_DATA): {
      _ASSERTE(pInMsg->u.buf.pBuf != NULL || pInMsg->u.buf.size == 0);

      if (pInMsg->u.buf.size == 0)
        return TRUE;

      pInMsg = ((State *)hFilterInstance)->Convert(((Filter *)hFilter)->escapeChar, pInMsg);

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_MODEM_STATUS): {
      // discard any status settings controlled by this filter
      pInMsg->u.val &= ~VAL2MASK(GO1_O2V_MODEM_STATUS(((State *)hFilterInstance)->Options()));
      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_LINE_STATUS): {
      // discard any status settings controlled by this filter
      pInMsg->u.val &= ~VAL2MASK(GO1_O2V_LINE_STATUS(((State *)hFilterInstance)->Options()));
      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_RBR_STATUS): {
      if (((State *)hFilterInstance)->Options() & GO1_RBR_STATUS) {
        // discard any status settings controlled by this filter
        if (!pMsgReplaceNone(pInMsg, HUB_MSG_TYPE_EMPTY))
          return FALSE;
      }
      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_RLC_STATUS): {
      if (((State *)hFilterInstance)->Options() & GO1_RLC_STATUS) {
        // discard any status settings controlled by this filter
        if (!pMsgReplaceNone(pInMsg, HUB_MSG_TYPE_EMPTY))
          return FALSE;
      }
      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_BREAK_STATUS): {
      if (((State *)hFilterInstance)->Options() & GO1_BREAK_STATUS) {
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
  Delete,
  CreateInstance,
  DeleteInstance,
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
      !ROUTINE_IS_VALID(pHubRoutines, pFilterName) ||
      !ROUTINE_IS_VALID(pHubRoutines, pFilterPort))
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
  pFilterPort = pHubRoutines->pFilterPort;

  return plugins;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
