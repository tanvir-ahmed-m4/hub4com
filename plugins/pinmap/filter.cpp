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
static ROUTINE_MSG_INSERT_VAL *pMsgInsertVal = NULL;
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
  DWORD val;
} pinOut_names[] = {
  {"rts=",    SO_V2O_PIN_STATE(PIN_STATE_RTS)},
  {"dtr=",    SO_V2O_PIN_STATE(PIN_STATE_DTR)},
  {"out1=",   SO_V2O_PIN_STATE(PIN_STATE_OUT1)},
  {"out2=",   SO_V2O_PIN_STATE(PIN_STATE_OUT2)},
  {"break=",  SO_V2O_PIN_STATE(PIN_STATE_BREAK)},
};
///////////////////////////////////////////////////////////////
static struct {
  const char *pName;
  DWORD val;
} pinIn_names[] = {
  {"cts",   GO_V2O_MODEM_STATUS(MODEM_STATUS_CTS)},
  {"dsr",   GO_V2O_MODEM_STATUS(MODEM_STATUS_DSR)},
  {"dcd",   GO_V2O_MODEM_STATUS(MODEM_STATUS_DCD)},
  {"ring",  GO_V2O_MODEM_STATUS(MODEM_STATUS_RI)},
  {"break", GO_V2O_LINE_STATUS(LINE_STATUS_BI)},
};
///////////////////////////////////////////////////////////////
class State {
  public:
    State() : inVal(0) {}

    WORD inVal;
};
///////////////////////////////////////////////////////////////
class Filter : public Valid {
  public:
    Filter(int argc, const char *const argv[]);
    State *GetState(int nPort);

    struct PinOuts {
      PinOuts() : mask(0), val(0) {}

      WORD mask;
      WORD val;
    };

    PinOuts pinMap[sizeof(pinIn_names)/sizeof(pinIn_names[0])];
    WORD outMask;
    WORD inMask;

  private:
    typedef map<int, State*> PortsMap;
    typedef pair<int, State*> PortPair;

    PortsMap portsMap;

    void Parse(const char *pArg);
};

Filter::Filter(int argc, const char *const argv[])
  : outMask(0),
    inMask(0)
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

  if (!inMask) {
    Parse("rts=cts");
    Parse("dtr=dsr");
    Parse("break=break");
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
        inMask |= pinIn_names[iIn].val;

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

State *Filter::GetState(int nPort)
{
  PortsMap::iterator iPair = portsMap.find(nPort);

  if (iPair == portsMap.end()) {
      portsMap.insert(PortPair(nPort, NULL));

      iPair = portsMap.find(nPort);

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
  << "  --break=[!]<s>        - wire input state of <s> to output pin BREAK." << endl
  << endl
  << "  The possible values of <s> above can be cts, dsr, dcd, ring or break. The" << endl
  << "  exclamation sign (!) can be used to invert the value. If no any wire option" << endl
  << "  specified, then the options --rts=cts --dtr=dsr --break=break are used by" << endl
  << "  default." << endl
  << endl
  << "OUT method input data stream description:" << endl
  << "  SET_OPTIONS(<opts>)   - the value <opts> will be or'ed with the required mask" << endl
  << "                          to to set pin state." << endl
  << "  GET_OPTIONS(<pOpts>)  - the value pointed by <pOpts> will be or'ed with" << endl
  << "                          the required mask to get line status and modem" << endl
  << "                          status." << endl
  << "  SET_PIN_STATE(<set>)  - pin settings controlled by this filter will be" << endl
  << "                          discarded from <set>." << endl
  << "  LINE_STATUS(<val>)    - current state of line." << endl
  << "  MODEM_STATUS(<val>)   - current state of modem." << endl
  << endl
  << "OUT method output data stream description:" << endl
  << "  SET_PIN_STATE(<set>)  - will be added on appropriate state changing." << endl
  << endl
  << "Examples:" << endl
  << "  " << pProgPath << " --create-filter=" << GetPluginAbout()->pName << " --add-filters=0,1:" << GetPluginAbout()->pName << " COM1 COM2" << endl
  << "    - transfer data and signals between COM1 and COM2." << endl
  << "  " << pProgPath << " --create-filter=" << GetPluginAbout()->pName << ":\"--rts=cts\" --add-filters=0,1:" << GetPluginAbout()->pName << " --octs=off COM1 COM2" << endl
  << "    - allow end-to-end RTS/CTS handshaking between COM1 and COM2." << endl
  << "  " << pProgPath << " --create-filter=" << GetPluginAbout()->pName << " --add-filters=0:" << GetPluginAbout()->pName << " --echo-route=0 COM2" << endl
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
static void InsertPinState(
    Filter &filter,
    WORD inMask,
    WORD inVal,
    HUB_MSG **ppOutMsg)
{
  if (!inMask)
    return;

  //cout << "InsertPinState inMask=0x" << hex << inMask << " inVal=0x" << inVal << dec << endl;

  WORD mask = 0;
  WORD val = 0;

  for (int iIn = 0 ; iIn < sizeof(pinIn_names)/sizeof(pinIn_names[0]) ; iIn++) {
    if ((inMask & pinIn_names[iIn].val) == 0)
      continue;

    mask |= filter.pinMap[iIn].mask;

    if ((inVal & pinIn_names[iIn].val) != 0)
      val |= (filter.pinMap[iIn].val & filter.pinMap[iIn].mask);
    else
      val |= (~filter.pinMap[iIn].val & filter.pinMap[iIn].mask);
  }

  if (mask) {
    DWORD dVal = (SPS_PIN2MASK(mask) | val);
    //cout << "SET_PIN_STATE 0x" << hex << dVal << dec << endl;
    *ppOutMsg = pMsgInsertVal(*ppOutMsg, HUB_MSG_TYPE_SET_PIN_STATE, dVal);
  }
}

static BOOL CALLBACK OutMethod(
    HFILTER hFilter,
    int /*nFromPort*/,
    int nToPort,
    HUB_MSG *pOutMsg)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(pOutMsg != NULL);

  switch (pOutMsg->type) {
    case HUB_MSG_TYPE_SET_OPTIONS: {
      // or'e with the required mask to set pin state
      pOutMsg->u.val |= ((Filter *)hFilter)->outMask;
      break;
    }
    case HUB_MSG_TYPE_GET_OPTIONS: {
      _ASSERTE(pOutMsg->u.pv.pVal != NULL);

      // or'e with the required mask to get line status and modem status
      WORD mask = (WORD)(((Filter *)hFilter)->inMask & pOutMsg->u.pv.val);
      *pOutMsg->u.pv.pVal |= mask;
      break;
    }
    case HUB_MSG_TYPE_SET_PIN_STATE:
      // discard any pin settings controlled by this filter
      pOutMsg->u.val &= ~(SPS_PIN2MASK(((Filter *)hFilter)->outMask));
      break;
    case HUB_MSG_TYPE_LINE_STATUS:
    case HUB_MSG_TYPE_MODEM_STATUS: {
      State *pState = ((Filter *)hFilter)->GetState(nToPort);

      if (!pState)
        return FALSE;

      WORD inVal;

      if (pOutMsg->type == HUB_MSG_TYPE_MODEM_STATUS)
        inVal = ((WORD)pOutMsg->u.val & 0x00FF) | (pState->inVal & 0xFF00);
      else
        inVal = ((WORD)pOutMsg->u.val << 8) | (pState->inVal & 0x00FF);

      inVal &= ((Filter *)hFilter)->inMask;

      InsertPinState(*(Filter *)hFilter, pState->inVal ^ inVal, inVal, &pOutMsg);
      pState->inVal = inVal;
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
  NULL,           // Init
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
  if (!ROUTINE_IS_VALID(pHubRoutines, pMsgInsertVal)) {
    return NULL;
  }

  pMsgInsertVal = pHubRoutines->pMsgInsertVal;

  return plugins;
}
///////////////////////////////////////////////////////////////
