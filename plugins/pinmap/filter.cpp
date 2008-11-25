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
 * Revision 1.13  2008/11/25 16:40:40  vfrolov
 * Added assert for port handle
 *
 * Revision 1.12  2008/11/24 12:37:00  vfrolov
 * Changed plugin API
 *
 * Revision 1.11  2008/11/13 07:51:34  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.10  2008/10/16 06:49:58  vfrolov
 * Added wiring to DCE's output pins
 *
 * Revision 1.9  2008/10/02 08:20:17  vfrolov
 * Added connect mapping
 *
 * Revision 1.8  2008/08/26 14:28:48  vfrolov
 * Removed option --break=break from default
 *
 * Revision 1.7  2008/08/25 08:08:22  vfrolov
 * Added init pin state
 *
 * Revision 1.6  2008/08/22 16:57:11  vfrolov
 * Added
 *   HUB_MSG_TYPE_GET_ESC_OPTS
 *   HUB_MSG_TYPE_FAIL_ESC_OPTS
 *   HUB_MSG_TYPE_BREAK_STATUS
 *
 * Revision 1.5  2008/08/22 12:45:34  vfrolov
 * Added masking to HUB_MSG_TYPE_MODEM_STATUS and HUB_MSG_TYPE_LINE_STATUS
 *
 * Revision 1.4  2008/08/20 14:30:19  vfrolov
 * Redesigned serial port options
 *
 * Revision 1.3  2008/08/14 09:49:45  vfrolov
 * Fixed output pins masking
 *
 * Revision 1.2  2008/08/13 14:31:41  vfrolov
 * Fixed Help
 *
 * Revision 1.1  2008/08/11 07:26:48  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "../plugins_api.h"
///////////////////////////////////////////////////////////////
namespace FilterPinMap {
///////////////////////////////////////////////////////////////
static ROUTINE_MSG_INSERT_VAL *pMsgInsertVal = NULL;
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
static struct {
  const char *pName;
  WORD val;
} pinOut_names[] = {
  {"rts=",    PIN_STATE_RTS},
  {"dtr=",    PIN_STATE_DTR},
  {"out1=",   PIN_STATE_OUT1},
  {"out2=",   PIN_STATE_OUT2},
  {"cts=",    PIN_STATE_CTS},
  {"dsr=",    PIN_STATE_DSR},
  {"ring=",   PIN_STATE_RI},
  {"dcd=",    PIN_STATE_DCD},
  {"break=",  PIN_STATE_BREAK},
};
///////////////////////////////////////////////////////////////
#define LM_BREAK    ((WORD)1 << 8)
#define LM_CONNECT  ((WORD)1 << 9)
#define MST2LM(m)   ((WORD)((BYTE)(m)))
#define LM2MST(lm)  ((BYTE)(lm))
#define LM2GO(lm)   (GO_V2O_MODEM_STATUS(LM2MST(lm)) | ((lm & LM_BREAK) ? GO_BREAK_STATUS : 0))

static struct {
  const char *pName;
  WORD lmVal;
} pinIn_names[] = {
  {"cts",     MST2LM(MODEM_STATUS_CTS)},
  {"dsr",     MST2LM(MODEM_STATUS_DSR)},
  {"dcd",     MST2LM(MODEM_STATUS_DCD)},
  {"ring",    MST2LM(MODEM_STATUS_RI)},
  {"break",   LM_BREAK},
  {"connect", LM_CONNECT},
};
///////////////////////////////////////////////////////////////
class State {
  public:
    State() : lmInVal(0) {}

    WORD lmInVal;
};
///////////////////////////////////////////////////////////////
class Filter : public Valid {
  public:
    Filter(int argc, const char *const argv[]);
    State *GetState(HMASTERPORT hPort);

    void SetFilterName(const char *_pName) { pName = _pName; }
    const char *FilterName() const { return pName; }

    struct PinOuts {
      PinOuts() : mask(0), val(0) {}

      WORD mask;
      WORD val;
    };

    PinOuts pinMap[sizeof(pinIn_names)/sizeof(pinIn_names[0])];
    WORD outMask;
    WORD lmInMask;

  private:
    const char *pName;

    typedef map<HMASTERPORT, State*> PortsMap;
    typedef pair<HMASTERPORT, State*> PortPair;

    PortsMap portsMap;

    void Parse(const char *pArg);
};

Filter::Filter(int argc, const char *const argv[])
  : outMask(0),
    lmInMask(0),
    pName(NULL)
{
  for (const char *const *pArgs = &argv[1] ; argc > 1 ; pArgs++, argc--) {
    const char *pArg = GetParam(*pArgs, "--");

    if (!pArg) {
      cerr << "Unknown option " << *pArgs << endl;
      Invalidate();
      continue;
    }

    Parse(pArg);
  }

  if (!lmInMask) {
    Parse("rts=cts");
    Parse("dtr=dsr");
  }
}

void Filter::Parse(const char *pArg)
{
  BOOL foundOut = FALSE;

  for (int iOut = 0 ; iOut < sizeof(pinOut_names)/sizeof(pinOut_names[0]) ; iOut++) {
    const char *pParam;

    if ((pParam = GetParam(pArg, pinOut_names[iOut].pName)) == NULL)
      continue;

    foundOut = TRUE;

    if ((outMask & pinOut_names[iOut].val) != 0) {
      cerr << "Duplicated option --" << pinOut_names[iOut].pName << endl;
      Invalidate();
    }

    outMask |= pinOut_names[iOut].val;

    BOOL negative;

    if (*pParam == '!') {
      negative = TRUE;
      pParam++;
    } else {
      negative = FALSE;
    }

    BOOL foundIn = FALSE;

    _ASSERTE(sizeof(pinIn_names)/sizeof(pinIn_names[0]) == sizeof(pinMap)/sizeof(pinMap[0]));

    for (int iIn = 0 ; iIn < sizeof(pinIn_names)/sizeof(pinIn_names[0]) ; iIn++) {
      if (_stricmp(pParam, pinIn_names[iIn].pName) == 0) {
        foundIn = TRUE;
        lmInMask |= pinIn_names[iIn].lmVal;

        pinMap[iIn].mask |= pinOut_names[iOut].val;

        if (negative)
          pinMap[iIn].val &= ~pinOut_names[iOut].val;
        else
          pinMap[iIn].val |= pinOut_names[iOut].val;

        break;
      }
    }

    if (!foundIn) {
      cerr << "Unknown pin " << pParam << endl;
      Invalidate();
    }

    break;
  }

  if (!foundOut) {
    cerr << "Unknown option " << pArg << endl;
    Invalidate();
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
///////////////////////////////////////////////////////////////
static PLUGIN_TYPE CALLBACK GetPluginType()
{
  return PLUGIN_TYPE_FILTER;
}
///////////////////////////////////////////////////////////////
static const PLUGIN_ABOUT_A about = {
  sizeof(PLUGIN_ABOUT_A),
  "pinmap",
  "Copyright (c) 2008 Vyacheslav Frolov",
  "GNU General Public License",
  "Pinouts mapping filter",
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
  << "Wire options:" << endl
  << "  --rts=[!]<s>          - wire input state of <s> to output pin RTS." << endl
  << "  --dtr=[!]<s>          - wire input state of <s> to output pin DTR." << endl
  << "  --out1=[!]<s>         - wire input state of <s> to output pin OUT1." << endl
  << "  --out2=[!]<s>         - wire input state of <s> to output pin OUT2." << endl
  << "  --cts=[!]<s>          - wire input state of <s> to output pin CTS." << endl
  << "  --dsr=[!]<s>          - wire input state of <s> to output pin DSR." << endl
  << "  --ring=[!]<s>         - wire input state of <s> to output pin RI." << endl
  << "  --dcd=[!]<s>          - wire input state of <s> to output pin DCD." << endl
  << "  --break=[!]<s>        - wire input state of <s> to output state of BREAK." << endl
  << endl
  << "  The possible values of <s> above can be cts, dsr, dcd, ring, break or" << endl
  << "  connect. The exclamation sign (!) can be used to invert the value. If no any" << endl
  << "  wire option specified, then the options --rts=cts --dtr=dsr are used by" << endl
  << "  default." << endl
  << endl
  << "OUT method input data stream description:" << endl
  << "  SET_PIN_STATE(<set>)  - pin settings controlled by this filter will be" << endl
  << "                          discarded from <set>." << endl
  << "  CONNECT(<val>)        - current state of connect." << endl
  << "  BREAK_STATUS(<val>)   - current state of break." << endl
  << "  MODEM_STATUS(<val>)   - current state of modem." << endl
  << endl
  << "OUT method output data stream description:" << endl
  << "  SET_PIN_STATE(<set>)  - will be added on appropriate state changing." << endl
  << endl
  << "Examples:" << endl
  << "  " << pProgPath << " --load=,,_END_" << endl
  << "      --create-filter=pinmap" << endl
  << "      --add-filters=0,1:pinmap" << endl
  << "      COM1" << endl
  << "      COM2" << endl
  << "      _END_" << endl
  << "    - transfer data and signals between COM1 and COM2." << endl
  << "  " << pProgPath << " --load=,,_END_" << endl
  << "      --create-filter=pinmap:\"--rts=cts\"" << endl
  << "      --add-filters=0,1:pinmap" << endl
  << "      --octs=off" << endl
  << "      COM1" << endl
  << "      COM2" << endl
  << "      _END_" << endl
  << "    - allow end-to-end RTS/CTS handshaking between COM1 and COM2." << endl
  << "  " << pProgPath << " --load=,,_END_" << endl
  << "      --create-filter=pinmap" << endl
  << "      --add-filters=0:pinmap" << endl
  << "      --echo-route=0" << endl
  << "      COM2" << endl
  << "      _END_" << endl
  << "    - receive data and signals from COM2 and send it back to COM2." << endl
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
static void InsertPinState(
    Filter &filter,
    WORD lmInMask,
    WORD lmInVal,
    HUB_MSG **ppOutMsg)
{
  if (!lmInMask)
    return;

  //cout << "InsertPinState lmInMask=0x" << hex << lmInMask << " lmInVal=0x" << lmInVal << dec << endl;

  WORD mask = 0;
  WORD val = 0;

  for (int iIn = 0 ; iIn < sizeof(pinIn_names)/sizeof(pinIn_names[0]) ; iIn++) {
    if ((lmInMask & pinIn_names[iIn].lmVal) == 0)
      continue;

    mask |= filter.pinMap[iIn].mask;

    if ((lmInVal & pinIn_names[iIn].lmVal) != 0)
      val |= (filter.pinMap[iIn].val & filter.pinMap[iIn].mask);
    else
      val |= (~filter.pinMap[iIn].val & filter.pinMap[iIn].mask);
  }

  if (mask) {
    DWORD dVal = (VAL2MASK(mask) | val);
    //cout << "SET_PIN_STATE 0x" << hex << dVal << dec << endl;
    *ppOutMsg = pMsgInsertVal(*ppOutMsg, HUB_MSG_TYPE_SET_PIN_STATE, dVal);
  }
}

static BOOL CALLBACK OutMethod(
    HFILTER hFilter,
    HMASTERPORT hFromPort,
    HMASTERPORT hToPort,
    HUB_MSG *pOutMsg)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(hFromPort != NULL);
  _ASSERTE(hToPort != NULL);
  _ASSERTE(pOutMsg != NULL);

  switch (pOutMsg->type) {
    case HUB_MSG_TYPE_SET_OUT_OPTS: {
      // or'e with the required mask to set pin state
      pOutMsg->u.val |= SO_V2O_PIN_STATE(((Filter *)hFilter)->outMask);

      State *pState = ((Filter *)hFilter)->GetState(hToPort);

      if (!pState)
        return FALSE;

      // init pin state
      InsertPinState(*(Filter *)hFilter, ((Filter *)hFilter)->lmInMask, pState->lmInVal, &pOutMsg);

      break;
    }
    case HUB_MSG_TYPE_GET_IN_OPTS: {
      _ASSERTE(pOutMsg->u.pv.pVal != NULL);

      // or'e with the required mask to get break status and modem status
      *pOutMsg->u.pv.pVal |= (LM2GO(((Filter *)hFilter)->lmInMask) & pOutMsg->u.pv.val);
      break;
    }
    case HUB_MSG_TYPE_FAIL_IN_OPTS: {
      DWORD fail_options = (pOutMsg->u.val & LM2GO(((Filter *)hFilter)->lmInMask));

      if (fail_options) {
        cerr << pPortName(hFromPort)
             << " WARNING: Requested by filter " << ((Filter *)hFilter)->FilterName()
             << " for port " << pPortName(hToPort)
             << " option(s) 0x" << hex << fail_options << dec
             << " not accepted" << endl;
      }
      break;
    }
    case HUB_MSG_TYPE_SET_PIN_STATE:
      // discard any pin settings controlled by this filter
      pOutMsg->u.val &= ~(VAL2MASK(((Filter *)hFilter)->outMask));
      break;
    case HUB_MSG_TYPE_MODEM_STATUS: {
      State *pState = ((Filter *)hFilter)->GetState(hToPort);

      if (!pState)
        return FALSE;

      WORD lmInMask = MST2LM(MASK2VAL(pOutMsg->u.val)) & ((Filter *)hFilter)->lmInMask;
      WORD lmInVal = ((MST2LM(pOutMsg->u.val) & lmInMask) | (pState->lmInVal & ~lmInMask));

      InsertPinState(*(Filter *)hFilter, pState->lmInVal ^ lmInVal, lmInVal, &pOutMsg);

      pState->lmInVal = lmInVal;
      break;
    }
    case HUB_MSG_TYPE_BREAK_STATUS: {
      if (((Filter *)hFilter)->lmInMask & LM_BREAK) {
        State *pState = ((Filter *)hFilter)->GetState(hToPort);

        if (!pState)
          return FALSE;

        WORD lmInVal = ((pOutMsg->u.val ? LM_BREAK : 0) | (pState->lmInVal & ~LM_BREAK));

        InsertPinState(*(Filter *)hFilter, pState->lmInVal ^ lmInVal, lmInVal, &pOutMsg);

        pState->lmInVal = lmInVal;
      }
      break;
    }
    case HUB_MSG_TYPE_CONNECT: {
      if (((Filter *)hFilter)->lmInMask & LM_CONNECT) {
        State *pState = ((Filter *)hFilter)->GetState(hToPort);

        if (!pState)
          return FALSE;

        WORD lmInVal = ((pOutMsg->u.val ? LM_CONNECT : 0) | (pState->lmInVal & ~LM_CONNECT));

        InsertPinState(*(Filter *)hFilter, pState->lmInVal ^ lmInVal, lmInVal, &pOutMsg);

        pState->lmInVal = lmInVal;
      }
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
  NULL,           // InMethod
  OutMethod,
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
      !ROUTINE_IS_VALID(pHubRoutines, pPortName) ||
      !ROUTINE_IS_VALID(pHubRoutines, pFilterName))
  {
    return NULL;
  }

  pMsgInsertVal = pHubRoutines->pMsgInsertVal;
  pPortName = pHubRoutines->pPortName;
  pFilterName = pHubRoutines->pFilterName;

  return plugins;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
