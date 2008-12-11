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
 * Revision 1.1  2008/12/11 13:16:59  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "../plugins_api.h"
///////////////////////////////////////////////////////////////
namespace FilterPurge {
///////////////////////////////////////////////////////////////
static ROUTINE_MSG_REPLACE_NONE *pMsgReplaceNone = NULL;
static ROUTINE_MSG_INSERT_NONE *pMsgInsertNone = NULL;
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
class Filter : public Valid {
  public:
    Filter(int argc, const char *const argv[]);

    void SetFilterName(const char *_pName) { pName = _pName; }
    const char *FilterName() const { return pName; }

    DWORD soOutMask;
    DWORD goInMask;

  private:
    const char *pName;
};

Filter::Filter(int argc, const char *const argv[])
  : soOutMask(SO_PURGE_TX),
    goInMask(GO_PURGE_TX_IN),
    pName(NULL)
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
  "purge",
  "Copyright (c) 2008 Vyacheslav Frolov",
  "GNU General Public License",
  "Purge mapping filter",
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
    HMASTERFILTER hMasterFilter)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(hMasterFilter != NULL);

  ((Filter *)hFilter)->SetFilterName(pFilterName(hMasterFilter));

  return TRUE;
}
///////////////////////////////////////////////////////////////
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
      // or'e with the required mask to purge
      pOutMsg->u.val |= ((Filter *)hFilter)->soOutMask;
      break;
    }
    case HUB_MSG_TYPE_GET_IN_OPTS: {
      _ASSERTE(pOutMsg->u.pv.pVal != NULL);

      // or'e with the required mask to purge
      *pOutMsg->u.pv.pVal |= (((Filter *)hFilter)->goInMask & pOutMsg->u.pv.val);
      break;
    }
    case HUB_MSG_TYPE_FAIL_IN_OPTS: {
      DWORD fail_options = (pOutMsg->u.val & ((Filter *)hFilter)->goInMask);

      if (fail_options) {
        cerr << pPortName(hFromPort)
             << " WARNING: Requested by filter " << ((Filter *)hFilter)->FilterName()
             << " for port " << pPortName(hToPort)
             << " option(s) 0x" << hex << fail_options << dec
             << " not accepted" << endl;
      }
      break;
    }
    case HUB_MSG_TYPE_PURGE_TX:
      // discard if controlled by this filter
      if (((Filter *)hFilter)->soOutMask & SO_PURGE_TX) {
        if (!pMsgReplaceNone(pOutMsg, HUB_MSG_TYPE_EMPTY))
          return FALSE;
      }
      break;
    case HUB_MSG_TYPE_PURGE_TX_IN:
      if ((((Filter *)hFilter)->goInMask & GO_PURGE_TX_IN) == 0)
        break;

      if ((((Filter *)hFilter)->soOutMask & SO_PURGE_TX) == 0)
        break;

      pOutMsg = pMsgInsertNone(pOutMsg, HUB_MSG_TYPE_PURGE_TX);
      break;
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
  if (!ROUTINE_IS_VALID(pHubRoutines, pMsgReplaceNone) ||
      !ROUTINE_IS_VALID(pHubRoutines, pMsgInsertNone) ||
      !ROUTINE_IS_VALID(pHubRoutines, pPortName) ||
      !ROUTINE_IS_VALID(pHubRoutines, pFilterName))
  {
    return NULL;
  }

  pMsgReplaceNone = pHubRoutines->pMsgReplaceNone;
  pMsgInsertNone = pHubRoutines->pMsgInsertNone;
  pPortName = pHubRoutines->pPortName;
  pFilterName = pHubRoutines->pFilterName;

  return plugins;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
