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
class Filter : public Valid {
  public:
    Filter(int argc, const char *const argv[]);

    int inTag;
    int outTag;
    enum OutTagState {No, My, Alien} outTagState;
};

Filter::Filter(int argc, const char *const argv[])
  : inTag(-1),
    outTag(-1),
    outTagState(No)
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

      inTag = outTag = tag;
    }
    else {
      cerr << "Unknown option " << pArg << endl;
      Invalidate();
    }
  }

  if (inTag < 0) {
    cerr << "IN tag was not set" << endl;
    Invalidate();
  }

  if (outTag < 0) {
    cerr << "OUT tag was not set" << endl;
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
  "Copyright (c) 2008 Vyacheslav Frolov",
  "GNU General Public License",
  "Tag filter",
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
  << "  --tag=<tag>           - set IN and OUT tags to <tag> (number from 0 to 254)." << endl
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
static BOOL CALLBACK InMethod(
    HFILTER hFilter,
    HMASTERPORT hFromPort,
    HUB_MSG *pInMsg,
    HUB_MSG **DEBUG_PARAM(ppEchoMsg))
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(hFromPort != NULL);
  _ASSERTE(pInMsg != NULL);
  _ASSERTE(ppEchoMsg != NULL);
  _ASSERTE(*ppEchoMsg == NULL);

  switch (pInMsg->type) {
    case HUB_MSG_TYPE_LINE_DATA: {
      _ASSERTE(pInMsg->u.buf.pBuf != NULL || pInMsg->u.buf.size == 0);

      DWORD len = pInMsg->u.buf.size;

      if (len == 0)
        break;

      BYTE tag = (BYTE)((Filter *)hFilter)->inTag;
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
    HMASTERPORT DEBUG_PARAM(hFromPort),
    HMASTERPORT hToPort,
    HUB_MSG *pOutMsg)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(hFromPort != NULL);
  _ASSERTE(hToPort != NULL);
  _ASSERTE(pOutMsg != NULL);

  switch (pOutMsg->type) {
    case HUB_MSG_TYPE_LINE_DATA: {
      _ASSERTE(pOutMsg->u.buf.pBuf != NULL || pOutMsg->u.buf.size == 0);

      DWORD len = pOutMsg->u.buf.size;

      if (len == 0)
        break;

      BYTE tag = (BYTE)((Filter *)hFilter)->outTag;
      Filter::OutTagState tagState = ((Filter *)hFilter)->outTagState;
      basic_string<BYTE> line_data;

      for (const BYTE *pBuf = pOutMsg->u.buf.pBuf ; len ; len--) {
        BYTE ch = *pBuf++;

        switch (tagState) {
          case Filter::No:
            tagState = (ch == tag ? Filter::My : Filter::Alien);
            break;
          case Filter::My:
            line_data.append(&ch, 1);
          case Filter::Alien:
            tagState = Filter::No;
            break;
        }
      }

      ((Filter *)hFilter)->outTagState = tagState;

      if (!pMsgReplaceBuf(pOutMsg, HUB_MSG_TYPE_LINE_DATA, line_data.data(), (DWORD)line_data.size()))
        return FALSE;

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
  InMethod,
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
  if (!ROUTINE_IS_VALID(pHubRoutines, pMsgReplaceBuf))
    return NULL;

  pMsgReplaceBuf = pHubRoutines->pMsgReplaceBuf;

  return plugins;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
