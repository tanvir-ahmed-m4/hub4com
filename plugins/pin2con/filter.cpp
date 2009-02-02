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
 * Revision 1.16  2009/02/02 15:21:42  vfrolov
 * Optimized filter's API
 *
 * Revision 1.15  2008/12/22 09:40:46  vfrolov
 * Optimized message switching
 *
 * Revision 1.14  2008/12/18 16:50:52  vfrolov
 * Extended the number of possible IN options
 *
 * Revision 1.13  2008/11/25 16:40:40  vfrolov
 * Added assert for port handle
 *
 * Revision 1.12  2008/11/24 12:37:00  vfrolov
 * Changed plugin API
 *
 * Revision 1.11  2008/11/13 07:50:41  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.10  2008/10/16 09:24:23  vfrolov
 * Changed return type of ROUTINE_MSG_REPLACE_*() to BOOL
 *
 * Revision 1.9  2008/09/30 07:52:09  vfrolov
 * Removed HUB_MSG_TYPE_LINE_STATUS filtering
 *
 * Revision 1.8  2008/08/22 16:57:11  vfrolov
 * Added
 *   HUB_MSG_TYPE_GET_ESC_OPTS
 *   HUB_MSG_TYPE_FAIL_ESC_OPTS
 *   HUB_MSG_TYPE_BREAK_STATUS
 *
 * Revision 1.7  2008/08/22 12:45:34  vfrolov
 * Added masking to HUB_MSG_TYPE_MODEM_STATUS and HUB_MSG_TYPE_LINE_STATUS
 *
 * Revision 1.6  2008/08/20 14:30:19  vfrolov
 * Redesigned serial port options
 *
 * Revision 1.5  2008/08/11 07:15:33  vfrolov
 * Replaced
 *   HUB_MSG_TYPE_COM_FUNCTION
 *   HUB_MSG_TYPE_INIT_LSR_MASK
 *   HUB_MSG_TYPE_INIT_MST_MASK
 * by
 *   HUB_MSG_TYPE_SET_PIN_STATE
 *   HUB_MSG_TYPE_GET_OPTIONS
 *   HUB_MSG_TYPE_SET_OPTIONS
 *
 * Revision 1.4  2008/04/14 07:32:04  vfrolov
 * Renamed option --use-port-module to --use-driver
 *
 * Revision 1.3  2008/04/11 14:48:42  vfrolov
 * Replaced SET_RT_EVENTS by INIT_LSR_MASK and INIT_MST_MASK
 * Replaced COM_ERRORS by LINE_STATUS
 *
 * Revision 1.2  2008/04/07 12:24:17  vfrolov
 * Replaced --rt-events option by SET_RT_EVENTS message
 *
 * Revision 1.1  2008/04/02 10:33:23  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "../plugins_api.h"
///////////////////////////////////////////////////////////////
namespace FilterPin2Con {
///////////////////////////////////////////////////////////////
#ifndef _DEBUG
  #define DEBUG_PARAM(par)
#else   /* _DEBUG */
  #define DEBUG_PARAM(par) par
#endif  /* _DEBUG */
///////////////////////////////////////////////////////////////
static ROUTINE_MSG_INSERT_VAL *pMsgInsertVal;
static ROUTINE_MSG_REPLACE_NONE *pMsgReplaceNone;
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
    State(HMASTERPORT hMasterPort) : pName(pPortName(hMasterPort)), connect(FALSE) {}

    const char *const pName;
    BOOL connect;
};
///////////////////////////////////////////////////////////////
class Filter : public Valid {
  public:
    Filter(const char *_pName, int argc, const char *const argv[]);

    const char *FilterName() const { return pName; }

    DWORD pin;
    BOOL negative;

  private:
    const char *pName;
};

static struct {
  const char *pName;
  DWORD val;
} pin_names[] = {
  {"cts",  GO1_V2O_MODEM_STATUS(MODEM_STATUS_CTS)},
  {"dsr",  GO1_V2O_MODEM_STATUS(MODEM_STATUS_DSR)},
  {"dcd",  GO1_V2O_MODEM_STATUS(MODEM_STATUS_DCD)},
  {"ring", GO1_V2O_MODEM_STATUS(MODEM_STATUS_RI)},
  {"break", GO1_BREAK_STATUS},
};

Filter::Filter(const char *_pName, int argc, const char *const argv[])
  : pName(_pName),
    pin(GO1_V2O_MODEM_STATUS(MODEM_STATUS_DSR)),
    negative(FALSE)
{
  for (const char *const *pArgs = &argv[1] ; argc > 1 ; pArgs++, argc--) {
    const char *pArg = GetParam(*pArgs, "--");

    if (!pArg) {
      cerr << "Unknown option " << *pArgs << endl;
      Invalidate();
      continue;
    }

    const char *pParam;

    if ((pParam = GetParam(pArg, "connect=")) != NULL) {
      if (*pParam == '!') {
        negative = TRUE;
        pParam++;
      }

      pin = 0;

      for (int i = 0 ; i < sizeof(pin_names)/sizeof(pin_names[0]) ; i++) {
        if (_stricmp(pParam, pin_names[i].pName) == 0) {
          pin = pin_names[i].val;
          break;
        }
      }

      if (!pin) {
        cerr << "Unknown pin " << pParam << " in " << *pArgs << endl;
        Invalidate();
      }
    } else {
      cerr << "Unknown option " << *pArgs << endl;
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
  "pin2con",
  "Copyright (c) 2008 Vyacheslav Frolov",
  "GNU General Public License",
  "Connect or disconnect on changing of line or modem state filter",
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
  << "  --connect=[!]<state>  - <state> is cts, dsr, dcd, ring or break (dsr by" << endl
  << "                          default). The exclamation sign (!) can be used to" << endl
  << "                          invert the value." << endl
  << endl
  << "IN method input data stream description:" << endl
  << "  CONNECT(TRUE/FALSE)   - will be discarded from stream." << endl
  << "  BREAK_STATUS(<val>)   - current state of break." << endl
  << "  MODEM_STATUS(<val>)   - current state of modem." << endl
  << endl
  << "IN method output data stream description:" << endl
  << "  CONNECT(TRUE/FALSE)   - will be added on appropriate state changing." << endl
  << endl
  << "Examples:" << endl
  << "  " << pProgPath << " --load=,,_END_" << endl
  << "      --create-filter=pin2con" << endl
  << "      --add-filters=0:pin2con" << endl
  << "      COM1" << endl
  << "      --use-driver=tcp" << endl
  << "      111.11.11.11:1111" << endl
  << "      _END_" << endl
  << "    - wait DSR ON from COM1 and then establish connection to 111.11.11.11:1111" << endl
  << "      and disconnect on DSR OFF." << endl
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
static HUB_MSG *InsertConnectState(
    Filter &filter,
    State &state,
    HUB_MSG *pInMsg,
    BOOL pinState)
{
  if (filter.negative)
    pinState = !pinState;

  if (state.connect != pinState)
    pInMsg = pMsgInsertVal(pInMsg, HUB_MSG_TYPE_CONNECT, state.connect = pinState);

  return pInMsg;
}

static BOOL CALLBACK InMethod(
    HFILTER hFilter,
    HFILTERINSTANCE hFilterInstance,
    HUB_MSG *pInMsg,
    HUB_MSG **DEBUG_PARAM(ppEchoMsg))
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(hFilterInstance != NULL);
  _ASSERTE(pInMsg != NULL);
  _ASSERTE(ppEchoMsg != NULL);
  _ASSERTE(*ppEchoMsg == NULL);

  switch (HUB_MSG_T2N(pInMsg->type)) {
  case HUB_MSG_T2N(HUB_MSG_TYPE_GET_IN_OPTS):
    _ASSERTE(pInMsg->u.pv.pVal != NULL);

    if (GO_O2I(pInMsg->u.pv.val) != 1)
      break;

    // or'e with the required mask to get line status and modem status
    *pInMsg->u.pv.pVal |= (((Filter *)hFilter)->pin & pInMsg->u.pv.val);
    break;
  case HUB_MSG_T2N(HUB_MSG_TYPE_FAIL_IN_OPTS): {
    if (GO_O2I(pInMsg->u.pv.val) != 1)
      break;

    DWORD fail_options = (pInMsg->u.val & ((Filter *)hFilter)->pin);

    if (fail_options) {
      cerr << ((State *)hFilterInstance)->pName
           << " WARNING: Requested by filter " << ((Filter *)hFilter)->FilterName()
           << " option(s) GO1_0x" << hex << fail_options << dec
           << " not accepted" << endl;
    }
    break;
  }
  case HUB_MSG_T2N(HUB_MSG_TYPE_CONNECT):
    // discard any CONNECT messages from the input stream
    if (!pMsgReplaceNone(pInMsg, HUB_MSG_TYPE_EMPTY))
      return FALSE;
    break;
  case HUB_MSG_T2N(HUB_MSG_TYPE_MODEM_STATUS): {
    WORD pin;

    pin = GO1_O2V_MODEM_STATUS(((Filter *)hFilter)->pin);

    if ((pin & MASK2VAL(pInMsg->u.val)) == 0)
      break;

    pInMsg = InsertConnectState(*((Filter *)hFilter), *(State *)hFilterInstance, pInMsg, ((pInMsg->u.val & pin) != 0));

    break;
  }
  case HUB_MSG_T2N(HUB_MSG_TYPE_BREAK_STATUS):
    if (((Filter *)hFilter)->pin & GO1_BREAK_STATUS)
      pInMsg = InsertConnectState(*((Filter *)hFilter), *(State *)hFilterInstance, pInMsg, pInMsg->u.val != 0);
    break;
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
      !ROUTINE_IS_VALID(pHubRoutines, pMsgReplaceNone) ||
      !ROUTINE_IS_VALID(pHubRoutines, pPortName) ||
      !ROUTINE_IS_VALID(pHubRoutines, pFilterName) ||
      !ROUTINE_IS_VALID(pHubRoutines, pFilterPort))
  {
    return NULL;
  }

  pMsgInsertVal = pHubRoutines->pMsgInsertVal;
  pMsgReplaceNone = pHubRoutines->pMsgReplaceNone;
  pPortName = pHubRoutines->pPortName;
  pFilterName = pHubRoutines->pFilterName;
  pFilterPort = pHubRoutines->pFilterPort;

  return plugins;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
