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
 * Revision 1.5  2011/12/02 14:43:24  vfrolov
 * Added Tag synchronization filter
 * Added example
 * Allowed multi-instancing
 *
 * Revision 1.4  2009/02/02 15:21:42  vfrolov
 * Optimized filter's API
 *
 * Revision 1.3  2008/12/22 09:40:46  vfrolov
 * Optimized message switching
 *
 * Revision 1.2  2008/12/19 18:28:56  vfrolov
 * Fixed Release compile warning
 *
 * Revision 1.1  2008/11/27 16:38:05  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "../plugins_api.h"
///////////////////////////////////////////////////////////////
namespace FilterTag {
///////////////////////////////////////////////////////////////
static ROUTINE_MSG_REPLACE_BUF *pMsgReplaceBuf;
///////////////////////////////////////////////////////////////
#ifndef _DEBUG
  #define DEBUG_PARAM(par)
#else   /* _DEBUG */
  #define DEBUG_PARAM(par) par
#endif  /* _DEBUG */
///////////////////////////////////////////////////////////////
static const char *GetParam(const char *pArg, const char *pPattern)
{
  size_t lenPattern = strlen(pPattern);

  if (_strnicmp(pArg, pPattern, lenPattern) != 0)
    return NULL;

  return pArg + lenPattern;
}
///////////////////////////////////////////////////////////////
static BOOL StrToInt(const char *pStr, int *pNum)
{
  BOOL res = FALSE;
  int num;
  int sign = 1;

  switch (*pStr) {
    case '-':
      sign = -1;
    case '+':
      pStr++;
      break;
  }

  for (num = 0 ;; pStr++) {
    switch (*pStr) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        num = num*10 + (*pStr - '0');
        res = TRUE;
        continue;
      case 0:
        break;
      default:
        res = FALSE;
    }
    break;
  }

  if (pNum)
    *pNum = num*sign;

  return res;
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
    State() : isValOut(FALSE) {}

    BOOL isValOut;
    BOOL isMyValOut;
};
///////////////////////////////////////////////////////////////
class StateSync {
  public:
    StateSync() : isValOut(FALSE), isValIn(FALSE), periodOut(1) {}

    BOOL isValIn;
    BOOL isValOut;
    int periodOut;
};
///////////////////////////////////////////////////////////////
class Filter : public Valid {
  public:
    Filter(int argc, const char *const argv[]);

    int tagIn;
    int tagOut;
};

Filter::Filter(int argc, const char *const argv[])
  : tagIn(-1)
  , tagOut(-1)
{
  for (const char *const *pArgs = &argv[1] ; argc > 1 ; pArgs++, argc--) {
    const char *pArg = GetParam(*pArgs, "--");

    if (!pArg) {
      cerr << "Unknown option " << *pArgs << endl;
      Invalidate();
      continue;
    }

    const char *pParam;

    if ((pParam = GetParam(pArg, "tag=")) != NULL) {
      int tag;

      if (!StrToInt(pParam, &tag) || tag < 0 || tag > 254) {
        cerr << "Invalid tag in " << pParam << endl;
        Invalidate();
        continue;
      }

      tagIn = tagOut = tag;
    }
    else {
      cerr << "Unknown option " << pArg << endl;
      Invalidate();
    }
  }

  if (tagIn < 0) {
    cerr << "IN tag was not set" << endl;
    Invalidate();
  }

  if (tagOut < 0) {
    cerr << "OUT tag was not set" << endl;
    Invalidate();
  }
}
///////////////////////////////////////////////////////////////
class FilterSync : public Valid {
  public:
    FilterSync(int argc, const char *const argv[]);

    int syncIn;
    int syncOut;
    int periodOut;
};

FilterSync::FilterSync(int argc, const char *const argv[])
  : syncIn(-1)
  , syncOut(-1)
  , periodOut(0)
{
  for (const char *const *pArgs = &argv[1] ; argc > 1 ; pArgs++, argc--) {
    const char *pArg = GetParam(*pArgs, "--");

    if (!pArg) {
      cerr << "Unknown option " << *pArgs << endl;
      Invalidate();
      continue;
    }

    const char *pParam;

    if ((pParam = GetParam(pArg, "sync=")) != NULL) {
      int sync;

      if (!StrToInt(pParam, &sync) || sync < 0 || sync > 254) {
        cerr << "Invalid sync in " << pParam << endl;
        Invalidate();
        continue;
      }

      syncIn = syncOut = sync;
    }
    else
    if ((pParam = GetParam(pArg, "period=")) != NULL) {
      if (!StrToInt(pParam, &periodOut) || periodOut < 0) {
        cerr << "Invalid period in " << pParam << endl;
        Invalidate();
        continue;
      }
    }
    else {
      cerr << "Unknown option " << pArg << endl;
      Invalidate();
    }
  }

  if (syncIn < 0) {
    cerr << "IN sync was not set" << endl;
    Invalidate();
  }

  if (syncOut < 0) {
    cerr << "OUT sync was not set" << endl;
    Invalidate();
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
  "tag",
  "Copyright (c) 2008-2011 Vyacheslav Frolov",
  "GNU General Public License",
  "Tag filter",
};

static const PLUGIN_ABOUT_A * CALLBACK GetPluginAbout()
{
  return &about;
}
///////////////////////////////////////////////////////////////
static const PLUGIN_ABOUT_A aboutSync = {
  sizeof(PLUGIN_ABOUT_A),
  "tag-sync",
  "Copyright (c) 2011 Vyacheslav Frolov",
  "GNU General Public License",
  "Tag synchronization filter",
};

static const PLUGIN_ABOUT_A * CALLBACK GetPluginAboutSync()
{
  return &aboutSync;
}
///////////////////////////////////////////////////////////////
static void CALLBACK Help(const char *pProgPath)
{
  cerr
  << "Usage:" << endl
  << "  " << pProgPath << " ... --create-filter=" << GetPluginAbout()->pName << "[,<FID>][:<options>] ... --add-filters=<ports>:[...,]<FID>[,...] ..." << endl
  << endl
  << "Options:" << endl
  << "  --tag=<tag>           - set IN and OUT tags (number from 0 to 254," << endl
  << "                          mandatory)." << endl
  << endl
  << "IN method input data stream description:" << endl
  << "  LINE_DATA - raw data." << endl
  << endl
  << "IN method output data stream description:" << endl
  << "  LINE_DATA - tagged by IN tag data." << endl
  << endl
  << "OUT method input data stream description:" << endl
  << "  LINE_DATA - tagged data (tagged by not OUT tag data will be discarded)." << endl
  << endl
  << "OUT method output data stream description:" << endl
  << "  LINE_DATA - raw data (untagged tagged by OUT tag data)." << endl
  << endl
  << "Examples:" << endl
  << "  " << pProgPath << " --load=,,_END_" << endl
  << "      \\\\.\\COM1" << endl
  << "      --create-filter=tag-sync:--sync=120 --period=3" << endl
  << "      --add-filters=0:tag-sync" << endl
  << endl
  << "      --use-driver=connector" << endl
  << "      CNCB0-muxA" << endl
  << "      --create-filter=tag,CNCB0-tag:--tag=48" << endl
  << "      --add-filters=1:CNCB0-tag" << endl
  << "      CNCB0-muxB" << endl
  << "      --bi-connect=CNCB0-muxA:CNCB0-muxB" << endl
  << "      --use-driver=serial" << endl
  << "      \\\\.\\CNCB0" << endl
  << "      --bi-route=2:3" << endl
  << endl
  << "      --use-driver=connector" << endl
  << "      CNCB1-muxA" << endl
  << "      --create-filter=tag,CNCB1-tag:--tag=49" << endl
  << "      --add-filters=4:CNCB1-tag" << endl
  << "      CNCB1-muxB" << endl
  << "      --bi-connect=CNCB1-muxA:CNCB1-muxB" << endl
  << "      --use-driver=serial" << endl
  << "      \\\\.\\CNCB1" << endl
  << "      --bi-route=5:6" << endl
  << endl
  << "      --bi-route=0:1,4" << endl
  << "      --no-default-fc-route=1,4:0" << endl
  << "      _END_" << endl
  << endl
  << "    - If from port COM1 received \"0a1ex0b1f0c1gx0d1h...\" then to the port CNCB0" << endl
  << "      will be sent \"abcd...\" and to the port CNCB1 will be sent \"efgh...\"." << endl
  << "      If from port CNCB0 received \"abcd...\" and simultaneously from port CNCB1" << endl
  << "      received \"efgh...\" then to the port COM1 will be sent something like" << endl
  << "      \"x0a1e0bx1f0c1gx0d1h...\"." << endl
  ;
}
///////////////////////////////////////////////////////////////
static void CALLBACK HelpSync(const char *pProgPath)
{
  cerr
  << "Usage:" << endl
  << "  " << pProgPath << " ... --create-filter=" << GetPluginAboutSync()->pName << "[,<FID>][:<options>] ... --add-filters=<ports>:[...,]<FID>[,...] ..." << endl
  << endl
  << "Options:" << endl
  << "  --sync=<sync>         - set IN and OUT syncs (number from 0 to 254," << endl
  << "                          mandatory)." << endl
  << "  --period=<sync>       - set period for OUT syncs (0 by default)." << endl
  << "                          If period is O then OUT sync will be inserted once" << endl
  << "                          before 1-st tag. If period is N then OUT sync will be" << endl
  << "                          inserted before 1-st and each subsequent N-th tag" << endl
  << endl
  << "IN method input data stream description:" << endl
  << "  LINE_DATA - tagged data with IN syncs." << endl
  << endl
  << "IN method output data stream description:" << endl
  << "  LINE_DATA - tagged data." << endl
  << endl
  << "OUT method input data stream description:" << endl
  << "  LINE_DATA - tagged data." << endl
  << endl
  << "OUT method output data stream description:" << endl
  << "  LINE_DATA - tagged data with OUT syncs." << endl
  << endl
  << "Examples:" << endl
  << "  See examples for Tag filter." << endl
  ;
}
///////////////////////////////////////////////////////////////
static HFILTER CALLBACK Create(
    HMASTERFILTER DEBUG_PARAM(hMasterFilter),
    HCONFIG /*hConfig*/,
    int argc,
    const char *const argv[])
{
  _ASSERTE(hMasterFilter != NULL);

  Filter *pFilter = new Filter(argc, argv);

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
static HFILTER CALLBACK CreateSync(
    HMASTERFILTER DEBUG_PARAM(hMasterFilter),
    HCONFIG /*hConfig*/,
    int argc,
    const char *const argv[])
{
  _ASSERTE(hMasterFilter != NULL);

  FilterSync *pFilter = new FilterSync(argc, argv);

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
static void CALLBACK DeleteSync(
    HFILTER hFilter)
{
  _ASSERTE(hFilter != NULL);

  delete (FilterSync *)hFilter;
}
///////////////////////////////////////////////////////////////
static HFILTERINSTANCE CALLBACK CreateInstance(
    HMASTERFILTERINSTANCE DEBUG_PARAM(hMasterFilterInstance))
{
  _ASSERTE(hMasterFilterInstance != NULL);

  return (HFILTERINSTANCE)new State();
}
///////////////////////////////////////////////////////////////
static HFILTERINSTANCE CALLBACK CreateInstanceSync(
    HMASTERFILTERINSTANCE DEBUG_PARAM(hMasterFilterInstance))
{
  _ASSERTE(hMasterFilterInstance != NULL);

  return (HFILTERINSTANCE)new StateSync();
}
///////////////////////////////////////////////////////////////
static void CALLBACK DeleteInstance(
    HFILTERINSTANCE hFilterInstance)
{
  _ASSERTE(hFilterInstance != NULL);

  delete (State *)hFilterInstance;
}
///////////////////////////////////////////////////////////////
static void CALLBACK DeleteInstanceSync(
    HFILTERINSTANCE hFilterInstance)
{
  _ASSERTE(hFilterInstance != NULL);

  delete (StateSync *)hFilterInstance;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK InMethod(
    HFILTER hFilter,
    HFILTERINSTANCE DEBUG_PARAM(hFilterInstance),
    HUB_MSG *pInMsg,
    HUB_MSG **DEBUG_PARAM(ppEchoMsg))
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(hFilterInstance != NULL);
  _ASSERTE(pInMsg != NULL);
  _ASSERTE(ppEchoMsg != NULL);
  _ASSERTE(*ppEchoMsg == NULL);

  switch (HUB_MSG_T2N(pInMsg->type)) {
    case HUB_MSG_T2N(HUB_MSG_TYPE_LINE_DATA): {
      _ASSERTE(pInMsg->u.buf.pBuf != NULL || pInMsg->u.buf.size == 0);

      DWORD len = pInMsg->u.buf.size;

      if (len == 0)
        break;

      BYTE tag = (BYTE)((Filter *)hFilter)->tagIn;
      basic_string<BYTE> line_data;

      for (const BYTE *pBuf = pInMsg->u.buf.pBuf ; len ; len--) {
        line_data.append(&tag, 1);
        line_data.append(pBuf++, 1);
      }

      if (!pMsgReplaceBuf(pInMsg, HUB_MSG_TYPE_LINE_DATA, line_data.data(), (DWORD)line_data.size()))
        return FALSE;

      break;
    }
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK OutMethod(
    HFILTER hFilter,
    HFILTERINSTANCE hFilterInstance,
    HMASTERPORT DEBUG_PARAM(hFromPort),
    HUB_MSG *pOutMsg)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(hFilterInstance != NULL);
  _ASSERTE(hFromPort != NULL);
  _ASSERTE(pOutMsg != NULL);

  switch (HUB_MSG_T2N(pOutMsg->type)) {
    case HUB_MSG_T2N(HUB_MSG_TYPE_LINE_DATA): {
      _ASSERTE(pOutMsg->u.buf.pBuf != NULL || pOutMsg->u.buf.size == 0);

      DWORD len = pOutMsg->u.buf.size;

      if (len == 0)
        break;

      BYTE tag = (BYTE)((Filter *)hFilter)->tagOut;
      BOOL isValOut = ((State *)hFilterInstance)->isValOut;
      BOOL isMyValOut = ((State *)hFilterInstance)->isMyValOut;
      basic_string<BYTE> line_data;

      for (const BYTE *pBuf = pOutMsg->u.buf.pBuf ; len ; len--) {
        BYTE ch = *pBuf++;

        if (isValOut) {
          if (isMyValOut)
            line_data.append(&ch, 1);

          isValOut = FALSE;
        } else {
          isMyValOut = (ch == tag);
          isValOut = TRUE;
        }
      }

      ((State *)hFilterInstance)->isValOut = isValOut;
      ((State *)hFilterInstance)->isMyValOut = isMyValOut;

      if (!pMsgReplaceBuf(pOutMsg, HUB_MSG_TYPE_LINE_DATA, line_data.data(), (DWORD)line_data.size()))
        return FALSE;

      break;
    }
  }

  return pOutMsg != NULL;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK InMethodSync(
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
    case HUB_MSG_T2N(HUB_MSG_TYPE_LINE_DATA): {
      _ASSERTE(pInMsg->u.buf.pBuf != NULL || pInMsg->u.buf.size == 0);

      DWORD len = pInMsg->u.buf.size;

      if (len == 0)
        break;

      // Discard syncs from stream

      BYTE sync = (BYTE)((FilterSync *)hFilter)->syncIn;
      BOOL isValIn = ((StateSync *)hFilterInstance)->isValIn;
      basic_string<BYTE> line_data;

      for (const BYTE *pBuf = pInMsg->u.buf.pBuf ; len ; len--) {
        BYTE ch = *pBuf++;

        if (isValIn) {
          line_data.append(&ch, 1);
          isValIn = FALSE;
        } else {
          if (ch != sync) {
            line_data.append(&ch, 1);
            isValIn = TRUE;
          }
        }
      }

      ((StateSync *)hFilterInstance)->isValIn = isValIn;

      if (!pMsgReplaceBuf(pInMsg, HUB_MSG_TYPE_LINE_DATA, line_data.data(), (DWORD)line_data.size()))
        return FALSE;

      break;
    }
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK OutMethodSync(
    HFILTER hFilter,
    HFILTERINSTANCE hFilterInstance,
    HMASTERPORT DEBUG_PARAM(hFromPort),
    HUB_MSG *pOutMsg)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(hFilterInstance != NULL);
  _ASSERTE(hFromPort != NULL);
  _ASSERTE(pOutMsg != NULL);

  switch (HUB_MSG_T2N(pOutMsg->type)) {
    case HUB_MSG_T2N(HUB_MSG_TYPE_LINE_DATA): {
      _ASSERTE(pOutMsg->u.buf.pBuf != NULL || pOutMsg->u.buf.size == 0);

      DWORD len = pOutMsg->u.buf.size;

      if (len == 0)
        break;

      // Add syncs to stream

      BYTE sync = (BYTE)((FilterSync *)hFilter)->syncOut;
      BOOL isValOut = ((StateSync *)hFilterInstance)->isValOut;
      int periodOut = ((StateSync *)hFilterInstance)->periodOut;
      basic_string<BYTE> line_data;

      for (const BYTE *pBuf = pOutMsg->u.buf.pBuf ; len ; len--) {
        BYTE ch = *pBuf++;

        if (isValOut) {
          line_data.append(&ch, 1);
          isValOut = FALSE;
        } else {
          if (periodOut > 0) {
            if (periodOut-- == 1) {
              line_data.append(&sync, 1);
              periodOut = ((FilterSync *)hFilter)->periodOut;
            }
          }

          line_data.append(&ch, 1);
          isValOut = TRUE;
        }
      }

      ((StateSync *)hFilterInstance)->isValOut = isValOut;
      ((StateSync *)hFilterInstance)->periodOut = periodOut;

      if (!pMsgReplaceBuf(pOutMsg, HUB_MSG_TYPE_LINE_DATA, line_data.data(), (DWORD)line_data.size()))
        return FALSE;

      break;
    }
  }

  return TRUE;
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
  OutMethod,
};

static const FILTER_ROUTINES_A routinesSync = {
  sizeof(FILTER_ROUTINES_A),
  GetPluginType,
  GetPluginAboutSync,
  HelpSync,
  NULL,           // ConfigStart
  NULL,           // Config
  NULL,           // ConfigStop
  CreateSync,
  DeleteSync,
  CreateInstanceSync,
  DeleteInstanceSync,
  InMethodSync,
  OutMethodSync,
};

static const PLUGIN_ROUTINES_A *const plugins[] = {
  (const PLUGIN_ROUTINES_A *)&routines,
  (const PLUGIN_ROUTINES_A *)&routinesSync,
  NULL
};
///////////////////////////////////////////////////////////////
PLUGIN_INIT_A InitA;
const PLUGIN_ROUTINES_A *const * CALLBACK InitA(
    const HUB_ROUTINES_A * pHubRoutines)
{
  if (!ROUTINE_IS_VALID(pHubRoutines, pMsgReplaceBuf))
    return NULL;

  pMsgReplaceBuf = pHubRoutines->pMsgReplaceBuf;

  return plugins;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
