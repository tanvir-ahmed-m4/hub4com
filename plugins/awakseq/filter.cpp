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
 * Revision 1.13  2009/02/04 15:41:16  vfrolov
 * Added pGetFilter()
 *
 * Revision 1.12  2009/02/02 15:21:42  vfrolov
 * Optimized filter's API
 *
 * Revision 1.11  2008/12/22 09:40:45  vfrolov
 * Optimized message switching
 *
 * Revision 1.10  2008/11/25 16:40:40  vfrolov
 * Added assert for port handle
 *
 * Revision 1.9  2008/11/24 12:36:59  vfrolov
 * Changed plugin API
 *
 * Revision 1.8  2008/11/13 07:45:07  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.7  2008/10/16 09:24:23  vfrolov
 * Changed return type of ROUTINE_MSG_REPLACE_*() to BOOL
 *
 * Revision 1.6  2008/08/27 11:38:29  vfrolov
 * Fixed CONNECT(FALSE) losing
 *
 * Revision 1.5  2008/08/20 09:26:40  vfrolov
 * Fixed typo
 *
 * Revision 1.4  2008/04/14 07:32:03  vfrolov
 * Renamed option --use-port-module to --use-driver
 *
 * Revision 1.3  2008/04/07 12:29:11  vfrolov
 * Replaced --rt-events option by SET_RT_EVENTS message
 *
 * Revision 1.2  2008/04/02 10:39:54  vfrolov
 * Added discarding CONNECT(FALSE) from the input stream
 *
 * Revision 1.1  2008/04/01 14:52:46  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "../plugins_api.h"
///////////////////////////////////////////////////////////////
namespace FilterAwakSeq {
///////////////////////////////////////////////////////////////
#ifndef _DEBUG
  #define DEBUG_PARAM(par)
#else   /* _DEBUG */
  #define DEBUG_PARAM(par) par
#endif  /* _DEBUG */
///////////////////////////////////////////////////////////////
static ROUTINE_MSG_REPLACE_VAL *pMsgReplaceVal;
static ROUTINE_MSG_REPLACE_NONE *pMsgReplaceNone;
static ROUTINE_MSG_INSERT_NONE *pMsgInsertNone;
static ROUTINE_GET_FILTER *pGetFilter;
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
    State(const BYTE *pAwakSeq)
      : connectSent(FALSE),
        connectionCounter(0)
    {
      StartAwakSeq(pAwakSeq);
    }

    void StartAwakSeq(const BYTE *pAwakSeq) {
      waitAwakSeq = TRUE;
      pAwakSeqNext = pAwakSeq;
    }

    BOOL waitAwakSeq;
    const BYTE *pAwakSeqNext;
    BOOL connectSent;
    int connectionCounter;
};
///////////////////////////////////////////////////////////////
class Filter : public Valid {
  public:
    Filter(int argc, const char *const argv[]);
    ~Filter() {
      if (pAwakSeq)
        free((void *)pAwakSeq);
    }

    const BYTE *pAwakSeq;
};

Filter::Filter(int argc, const char *const argv[])
  : pAwakSeq(NULL)
{
  for (const char *const *pArgs = &argv[1] ; argc > 1 ; pArgs++, argc--) {
    const char *pArg = GetParam(*pArgs, "--");

    if (!pArg) {
      cerr << "ERROR: Unknown option " << *pArgs << endl;
      Invalidate();
      continue;
    }

    const char *pParam;

    if ((pParam = GetParam(pArg, "awak-seq=")) != NULL) {
      if (pAwakSeq) {
        cerr << "ERROR: The awakening sequence was set twice" << endl;
        Invalidate();
        continue;
      }

      pAwakSeq = (BYTE *)_strdup(pParam);

      if (!pAwakSeq) {
        cerr << "No enough memory." << endl;
        exit(2);
      }
    } else {
      cerr << "ERROR: Unknown option " << *pArgs << endl;
      Invalidate();
    }
  }

  if (IsValid() && !pAwakSeq) {
    pAwakSeq = (BYTE *)_strdup("");

    if (!pAwakSeq) {
      cerr << "No enough memory." << endl;
      exit(2);
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
  "awakseq",
  "Copyright (c) 2008 Vyacheslav Frolov",
  "GNU General Public License",
  "Connect on awakening sequence filter",
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
  << "  --awak-seq=<s>    - set awakening sequence to <s>." << endl
  << endl
  << "IN method input data stream description:" << endl
  << "  LINE_DATA(<data>) - <data> is the raw bytes." << endl
  << "  CONNECT(TRUE)     - it will be discarded from stream." << endl
  << "  CONNECT(FALSE)    - it will be discarded from stream if CONNECT(TRUE) was not" << endl
  << "                      added yet. Start awakening sequence waiting." << endl
  << endl
  << "IN method output data stream description:" << endl
  << "  LINE_DATA(<data>) - <data> is the raw bytes w/o awakening sequence and all" << endl
  << "                      data before it." << endl
  << "  CONNECT(TRUE)     - will be added when awakening sequence found." << endl
  << endl
  << "OUT method input data stream description:" << endl
  << "  CONNECT(TRUE)     - increment connection counter." << endl
  << "  CONNECT(FALSE)    - decrement connection counter. The decrementing of the" << endl
  << "                      connection counter to 0 will start awakening sequence" << endl
  << "                      waiting." << endl
  << endl
  << "Examples:" << endl
  << "  " << pProgPath << " --create-filter=" << GetPluginAbout()->pName << " --add-filters=0:" << GetPluginAbout()->pName << " COM1 --use-driver=tcp 111.11.11.11:1111" << endl
  << "    - wait first byte from COM1 and then establish connection to" << endl
  << "      111.11.11.11:1111." << endl
  << "  " << pProgPath << " --create-filter=pin2con --create-filter=" << GetPluginAbout()->pName << ":\"--awak-seq=aaa\" --add-filters=0:pin2con," << GetPluginAbout()->pName << " COM1 --use-driver=tcp 111.11.11.11:1111" << endl
  << "    - wait \"aaa\" from COM1 and then establish connection to 111.11.11.11:1111." << endl
  << "      and disconnect on DSR OFF." << endl
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

  Filter *pFilter = (Filter *)pGetFilter(hMasterFilterInstance);

  _ASSERTE(pFilter != NULL);

  return (HFILTERINSTANCE)new State(pFilter->pAwakSeq);
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

    DWORD size = pInMsg->u.buf.size;

    if (size == 0)
      return TRUE;

    if (!((State *)hFilterInstance)->waitAwakSeq)
      return TRUE;

    BYTE *pBuf = pInMsg->u.buf.pBuf;
    const BYTE *pAwakSeqNext = ((State *)hFilterInstance)->pAwakSeqNext;

    for ( ; *pAwakSeqNext && size ; size--) {
      if (*pAwakSeqNext == *pBuf++)
        pAwakSeqNext++;
      else
        pAwakSeqNext = ((Filter *)hFilter)->pAwakSeq;
    }

    if (!*pAwakSeqNext) {
      ((State *)hFilterInstance)->waitAwakSeq = FALSE;

      if (size) {
        // insert CONNECT(TRUE) before rest of data

        if (pBuf != pInMsg->u.buf.pBuf) {
          memmove(pInMsg->u.buf.pBuf, pBuf, size);
          pBuf = pInMsg->u.buf.pBuf;
        }

        pInMsg->type = HUB_MSG_TYPE_CONNECT;
        pInMsg->u.val = TRUE;

        pInMsg = pMsgInsertNone(pInMsg, HUB_MSG_TYPE_EMPTY);

        if (pInMsg) {
          pInMsg->type = HUB_MSG_TYPE_LINE_DATA;
          pInMsg->u.buf.pBuf = pBuf;
          pInMsg->u.buf.size = size;
        }
      } else {
        // insert CONNECT(TRUE) instead data

        if(!pMsgReplaceVal(pInMsg, HUB_MSG_TYPE_CONNECT, TRUE))
          return FALSE;
      }

      ((State *)hFilterInstance)->connectSent = TRUE;
    } else {
      pInMsg->u.buf.size = 0;
      ((State *)hFilterInstance)->pAwakSeqNext = pAwakSeqNext;
    }
    break;
  }
  case HUB_MSG_T2N(HUB_MSG_TYPE_CONNECT):
    if (pInMsg->u.val) {
      // discard CONNECT(TRUE) from the input stream
      if (!pMsgReplaceNone(pInMsg, HUB_MSG_TYPE_EMPTY))
        return FALSE;
    } else {
      // discard CONNECT(FALSE) from the input stream
      // if CONNECT(TRUE) was not added yet
      if (((State *)hFilterInstance)->connectSent) {
        ((State *)hFilterInstance)->connectSent = FALSE;
      } else {
        if (!pMsgReplaceNone(pInMsg, HUB_MSG_TYPE_EMPTY))
          return FALSE;
      }

      // start awakening sequence waiting
      ((State *)hFilterInstance)->StartAwakSeq(((Filter *)hFilter)->pAwakSeq);
    }
    break;
  }

  return pInMsg != NULL;
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
    case HUB_MSG_T2N(HUB_MSG_TYPE_CONNECT): {
      if (pOutMsg->u.val) {
        ((State *)hFilterInstance)->connectionCounter++;

        _ASSERTE(((State *)hFilterInstance)->connectionCounter > 0);
      } else {
        _ASSERTE(((State *)hFilterInstance)->connectionCounter > 0);

        if (--((State *)hFilterInstance)->connectionCounter <= 0)
          ((State *)hFilterInstance)->StartAwakSeq(((Filter *)hFilter)->pAwakSeq);
      }
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

static const PLUGIN_ROUTINES_A *const plugins[] = {
  (const PLUGIN_ROUTINES_A *)&routines,
  NULL
};
///////////////////////////////////////////////////////////////
PLUGIN_INIT_A InitA;
const PLUGIN_ROUTINES_A *const * CALLBACK InitA(
    const HUB_ROUTINES_A * pHubRoutines)
{
  if (!ROUTINE_IS_VALID(pHubRoutines, pMsgReplaceVal) ||
      !ROUTINE_IS_VALID(pHubRoutines, pMsgReplaceNone) ||
      !ROUTINE_IS_VALID(pHubRoutines, pMsgInsertNone) ||
      !ROUTINE_IS_VALID(pHubRoutines, pGetFilter))
  {
    return NULL;
  }

  pMsgReplaceVal = pHubRoutines->pMsgReplaceVal;
  pMsgReplaceNone = pHubRoutines->pMsgReplaceNone;
  pMsgInsertNone = pHubRoutines->pMsgInsertNone;
  pGetFilter = pHubRoutines->pGetFilter;

  return plugins;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
