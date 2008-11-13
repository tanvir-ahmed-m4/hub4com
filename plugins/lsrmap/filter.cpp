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
 * Revision 1.2  2008/11/13 07:49:45  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.1  2008/10/16 07:05:53  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "../plugins_api.h"
///////////////////////////////////////////////////////////////
namespace FilterLsrMap {
///////////////////////////////////////////////////////////////
static ROUTINE_MSG_INSERT_VAL *pMsgInsertVal;
static ROUTINE_PORT_NAME_A *pPortName;
static ROUTINE_FILTER_NAME_A *pFilterName;
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
class Filter : public Valid {
  public:
    Filter(int argc, const char *const argv[]);
    void SetHub(HHUB _hHub) { hHub = _hHub; }
    const char *PortName(int nPort) const { return pPortName(hHub, nPort); }
    const char *FilterName() const { return pFilterName(hHub, (HFILTER)this); }

    BYTE lsrMask;

  private:
    HHUB hHub;
};

Filter::Filter(int argc, const char *const argv[])
  : lsrMask(LINE_STATUS_OE|LINE_STATUS_PE|LINE_STATUS_FE|LINE_STATUS_BI|LINE_STATUS_FIFOERR),
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
///////////////////////////////////////////////////////////////
static PLUGIN_TYPE CALLBACK GetPluginType()
{
  return PLUGIN_TYPE_FILTER;
}
///////////////////////////////////////////////////////////////
static const PLUGIN_ABOUT_A about = {
  sizeof(PLUGIN_ABOUT_A),
  "lsrmap",
  "Copyright (c) 2008 Vyacheslav Frolov",
  "GNU General Public License",
  "LSR mapping filter",
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
///////////////////////////////////////////////////////////////
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
      // or'e with the required mask to set line status
      pOutMsg->u.val |= SO_V2O_LINE_STATUS(((Filter *)hFilter)->lsrMask);
      break;
    }
    case HUB_MSG_TYPE_GET_IN_OPTS: {
      _ASSERTE(pOutMsg->u.pv.pVal != NULL);

      // or'e with the required mask to get line status
      *pOutMsg->u.pv.pVal |= (GO_V2O_LINE_STATUS(((Filter *)hFilter)->lsrMask) & pOutMsg->u.pv.val);
      break;
    }
    case HUB_MSG_TYPE_FAIL_IN_OPTS: {
      DWORD fail_options = (pOutMsg->u.val & GO_V2O_LINE_STATUS(((Filter *)hFilter)->lsrMask));

      if (fail_options) {
        cerr << ((Filter *)hFilter)->PortName(nFromPort)
             << " WARNING: Requested by filter " << ((Filter *)hFilter)->FilterName()
             << " for port " << ((Filter *)hFilter)->PortName(nToPort)
             << " option(s) 0x" << hex << fail_options << dec
             << " not accepted" << endl;
      }
      break;
    }
    case HUB_MSG_TYPE_LINE_STATUS: {
      BYTE lsr;

      lsr = (BYTE)pOutMsg->u.val & (BYTE)MASK2VAL(pOutMsg->u.val) & ((Filter *)hFilter)->lsrMask;

      if (lsr)
        pOutMsg = pMsgInsertVal(pOutMsg, HUB_MSG_TYPE_SET_LSR, VAL2MASK(lsr) | lsr);

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
