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
 * Revision 1.3  2008/10/16 09:24:23  vfrolov
 * Changed return type of ROUTINE_MSG_REPLACE_*() to BOOL
 *
 * Revision 1.2  2008/10/16 06:34:27  vfrolov
 * Fixed typo
 *
 * Revision 1.1  2008/09/30 08:34:38  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "../plugins_api.h"

///////////////////////////////////////////////////////////////
static ROUTINE_MSG_INSERT_VAL *pMsgInsertVal = NULL;
static ROUTINE_MSG_REPLACE_NONE *pMsgReplaceNone = NULL;
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
class State {
  public:
    State()
    : br(CBR_19200),
      lc(VAL2LC_BYTESIZE(8)|VAL2LC_PARITY(NOPARITY)|VAL2LC_PARITY(ONESTOPBIT))
    {}

    ULONG br;
    DWORD lc;
};
///////////////////////////////////////////////////////////////
class Filter : public Valid {
  public:
    Filter(int argc, const char *const argv[]);
    void SetHub(HHUB _hHub) { hHub = _hHub; }
    State *GetState(int nPort);
    const char *PortName(int nPort) const { return pPortName(hHub, nPort); }
    const char *FilterName() const { return pFilterName(hHub, (HFILTER)this); }

    DWORD soOutMask;
    DWORD goInMask;

  private:
    HHUB hHub;

    typedef map<int, State*> PortsMap;
    typedef pair<int, State*> PortPair;

    PortsMap portsMap;
};

Filter::Filter(int argc, const char *const argv[])
  : soOutMask(SO_SET_BR|SO_SET_LC),
    goInMask(GO_RBR_STATUS|GO_RLC_STATUS),
    hHub(NULL)
{
  for (const char *const *pArgs = &argv[1] ; argc > 1 ; pArgs++, argc--) {
    const char *pArg = GetParam(*pArgs, "--");

    if (!pArg) {
      cerr << "Unknown option " << *pArgs << endl;
      Invalidate();
      continue;
    }

    {
      cerr << "Unknown option --" << pArg << endl;
      Invalidate();
    }
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
  "linectl",
  "Copyright (c) 2008 Vyacheslav Frolov",
  "GNU General Public License",
  "Baudrate and line control mapping filter",
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
  << endl
  << "Examples:" << endl
  << "  " << pProgPath << " --load=,,_END_" << endl
  << "      --create-filter=escparse" << endl
  << "      --add-filters=0,1:escparse" << endl
  << "      --create-filter=linectl" << endl
  << "      --add-filters=0,1:linectl" << endl
  << "      \\\\.\\CNCB0" << endl
  << "      \\\\.\\CNCB1" << endl
  << "      _END_" << endl
  << "    - transfer data, baudrate and line control between CNCB0 and CNCB1." << endl
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
    HHUB hHub)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(hHub != NULL);

  ((Filter *)hFilter)->SetHub(hHub);

  return TRUE;
}

static BOOL CALLBACK OutMethod(
    HFILTER hFilter,
    int nFromPort,
    int nToPort,
    HUB_MSG *pOutMsg)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(pOutMsg != NULL);

  switch (pOutMsg->type) {
    case HUB_MSG_TYPE_SET_OUT_OPTS: {
      // or'e with the required mask to set
      pOutMsg->u.val |= ((Filter *)hFilter)->soOutMask;

      State *pState = ((Filter *)hFilter)->GetState(nToPort);

      if (!pState)
        return FALSE;

      // init baudrate and line control
      if (((Filter *)hFilter)->soOutMask & SO_SET_BR)
        pOutMsg = pMsgInsertVal(pOutMsg, HUB_MSG_TYPE_SET_BR, pState->br);
      if (((Filter *)hFilter)->soOutMask & SO_SET_LC)
        pOutMsg = pMsgInsertVal(pOutMsg, HUB_MSG_TYPE_SET_LC, pState->lc);

      break;
    }
    case HUB_MSG_TYPE_GET_IN_OPTS: {
      _ASSERTE(pOutMsg->u.pv.pVal != NULL);

      // or'e with the required mask to get remote baudrate and line control
      *pOutMsg->u.pv.pVal |= (((Filter *)hFilter)->goInMask & pOutMsg->u.pv.val);
      break;
    }
    case HUB_MSG_TYPE_FAIL_IN_OPTS: {
      DWORD fail_options = (pOutMsg->u.val & ((Filter *)hFilter)->goInMask);

      if (fail_options) {
        cerr << ((Filter *)hFilter)->PortName(nFromPort)
             << " WARNING: Requested by filter " << ((Filter *)hFilter)->FilterName()
             << " for port " << ((Filter *)hFilter)->PortName(nToPort)
             << " option(s) 0x" << hex << fail_options << dec
             << " not accepted" << endl;
      }
      break;
    }
    case HUB_MSG_TYPE_SET_BR:
      // discard if controlled by this filter
      if (((Filter *)hFilter)->soOutMask & SO_SET_BR) {
        if (!pMsgReplaceNone(pOutMsg, HUB_MSG_TYPE_EMPTY))
          return FALSE;
      }
      break;
    case HUB_MSG_TYPE_SET_LC:
      // discard if controlled by this filter
      if (((Filter *)hFilter)->soOutMask & SO_SET_LC) {
        if (!pMsgReplaceNone(pOutMsg, HUB_MSG_TYPE_EMPTY))
          return FALSE;
      }
      break;
    case HUB_MSG_TYPE_RBR_STATUS: {
      if (((Filter *)hFilter)->soOutMask & SO_SET_BR) {
        State *pState = ((Filter *)hFilter)->GetState(nToPort);

        if (!pState)
          return FALSE;

        if (pState->br != pOutMsg->u.val) {
          pOutMsg = pMsgInsertVal(pOutMsg, HUB_MSG_TYPE_SET_BR, pOutMsg->u.val);
          pState->br = pOutMsg->u.val;
        }
      }
      break;
    }
    case HUB_MSG_TYPE_RLC_STATUS: {
      if (((Filter *)hFilter)->soOutMask & SO_SET_LC) {
        State *pState = ((Filter *)hFilter)->GetState(nToPort);

        if (!pState)
          return FALSE;

        _ASSERTE((pOutMsg->u.val & ~(VAL2LC_BYTESIZE(-1)|VAL2LC_PARITY(-1)|VAL2LC_STOPBITS(-1))) == 0);

        if (pState->lc != pOutMsg->u.val) {
          pOutMsg = pMsgInsertVal(pOutMsg, HUB_MSG_TYPE_SET_LC, pOutMsg->u.val);
          pState->lc = pOutMsg->u.val;
        }
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
      !ROUTINE_IS_VALID(pHubRoutines, pMsgReplaceNone) ||
      !ROUTINE_IS_VALID(pHubRoutines, pPortName) ||
      !ROUTINE_IS_VALID(pHubRoutines, pFilterName))
  {
    return NULL;
  }

  pMsgInsertVal = pHubRoutines->pMsgInsertVal;
  pMsgReplaceNone = pHubRoutines->pMsgReplaceNone;
  pPortName = pHubRoutines->pPortName;
  pFilterName = pHubRoutines->pFilterName;

  return plugins;
}
///////////////////////////////////////////////////////////////
