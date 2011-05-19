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
 * Revision 1.8  2011/05/19 16:29:13  vfrolov
 * Fixed typo
 *
 * Revision 1.7  2009/02/02 15:21:42  vfrolov
 * Optimized filter's API
 *
 * Revision 1.6  2008/12/22 09:40:45  vfrolov
 * Optimized message switching
 *
 * Revision 1.5  2008/12/18 16:50:52  vfrolov
 * Extended the number of possible IN options
 *
 * Revision 1.4  2008/11/25 16:40:40  vfrolov
 * Added assert for port handle
 *
 * Revision 1.3  2008/11/24 12:37:00  vfrolov
 * Changed plugin API
 *
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
#ifndef _DEBUG
  #define DEBUG_PARAM(par)
#else   /* _DEBUG */
  #define DEBUG_PARAM(par) par
#endif  /* _DEBUG */
///////////////////////////////////////////////////////////////
static ROUTINE_MSG_INSERT_VAL *pMsgInsertVal;
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
class Filter : public Valid {
  public:
    Filter(const char *pName, int argc, const char *const argv[]);

    const char *FilterName() const { return pName; }

    BYTE lsrMask;

  private:
    const char *pName;
};

Filter::Filter(const char *_pName, int argc, const char *const argv[])
  : pName(_pName),
    lsrMask(LINE_STATUS_OE|LINE_STATUS_PE|LINE_STATUS_FE|LINE_STATUS_BI|LINE_STATUS_FIFOERR)
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
  "Copyright (c) 2008-2011 Vyacheslav Frolov",
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

  return (HFILTERINSTANCE)pPortName(hMasterPort);
}
///////////////////////////////////////////////////////////////
static void CALLBACK DeleteInstance(
    HFILTERINSTANCE DEBUG_PARAM(hFilterInstance))
{
  _ASSERTE(hFilterInstance != NULL);
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK OutMethod(
    HFILTER hFilter,
    HFILTERINSTANCE hFilterInstance,
    HMASTERPORT hFromPort,
    HUB_MSG *pOutMsg)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(hFilterInstance != NULL);
  _ASSERTE(hFromPort != NULL);
  _ASSERTE(pOutMsg != NULL);

  switch (HUB_MSG_T2N(pOutMsg->type)) {
    case HUB_MSG_T2N(HUB_MSG_TYPE_SET_OUT_OPTS): {
      // or'e with the required mask to set line status
      pOutMsg->u.val |= SO_V2O_LINE_STATUS(((Filter *)hFilter)->lsrMask);
      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_GET_IN_OPTS): {
      _ASSERTE(pOutMsg->u.pv.pVal != NULL);

      if (GO_O2I(pOutMsg->u.pv.val) != 1)
        break;

      // or'e with the required mask to get line status
      *pOutMsg->u.pv.pVal |= (GO1_V2O_LINE_STATUS(((Filter *)hFilter)->lsrMask) & pOutMsg->u.pv.val);
      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_FAIL_IN_OPTS): {
      if (GO_O2I(pOutMsg->u.pv.val) != 1)
        break;

      DWORD fail_options = (pOutMsg->u.val & GO1_V2O_LINE_STATUS(((Filter *)hFilter)->lsrMask));

      if (fail_options) {
        cerr << (const char *)hFilterInstance
             << " WARNING: Requested by filter " << ((Filter *)hFilter)->FilterName()
             << " for port " << pPortName(hFromPort)
             << " option(s) GO1_0x" << hex << fail_options << dec
             << " not accepted" << endl;
      }
      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_LINE_STATUS): {
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
  Delete,
  CreateInstance,
  DeleteInstance,
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
      !ROUTINE_IS_VALID(pHubRoutines, pFilterName) ||
      !ROUTINE_IS_VALID(pHubRoutines, pFilterPort))
  {
    return NULL;
  }

  pMsgInsertVal = pHubRoutines->pMsgInsertVal;
  pPortName = pHubRoutines->pPortName;
  pFilterName = pHubRoutines->pFilterName;
  pFilterPort = pHubRoutines->pFilterPort;

  return plugins;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
