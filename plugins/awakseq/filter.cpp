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
static ROUTINE_MSG_REPLACE_VAL *pMsgReplaceVal = NULL;
static ROUTINE_MSG_REPLACE_NONE *pMsgReplaceNone = NULL;
static ROUTINE_MSG_INSERT_NONE *pMsgInsertNone = NULL;
///////////////////////////////////////////////////////////////
const char *GetParam(const char *pArg, const char *pPattern)
{
  size_t lenPattern = strlen(pPattern);

  if (_strnicmp(pArg, pPattern, lenPattern) != 0)
    return NULL;

  return pArg + lenPattern;
}
///////////////////////////////////////////////////////////////
class State {
  public:
    State(const BYTE *pAwakSeq)
      : connectSent(FALSE), connectionCounter(0) { StartAwakSeq(pAwakSeq); }

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
class Filter {
  public:
    Filter(int argc, const char *const argv[]);
    State *GetState(int nPort);

    const BYTE *pAwakSeq;

  private:

    typedef map<int, State*> PortsMap;
    typedef pair<int, State*> PortPair;

    PortsMap portsMap;
};

Filter::Filter(int argc, const char *const argv[])
  : pAwakSeq((const BYTE *)"")
{
  for (const char *const *pArgs = &argv[1] ; argc > 1 ; pArgs++, argc--) {
    const char *pArg = GetParam(*pArgs, "--");

    if (!pArg) {
      cerr << "Unknown option " << *pArgs << endl;
      continue;
    }

    const char *pParam;

    if ((pParam = GetParam(pArg, "awak-seq=")) != NULL) {
      pAwakSeq = (const BYTE *)_strdup(pParam);
    } else {
      cerr << "Unknown option " << pArg << endl;
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
    iPair->second = new State(pAwakSeq);

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
    HCONFIG /*hConfig*/,
    int argc,
    const char *const argv[])
{
  return (HFILTER)new Filter(argc, argv);
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK InMethod(
    HFILTER hFilter,
    int nFromPort,
    HUB_MSG *pInMsg,
    HUB_MSG **DEBUG_PARAM(ppEchoMsg))
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(pInMsg != NULL);
  _ASSERTE(ppEchoMsg != NULL);
  _ASSERTE(*ppEchoMsg == NULL);

  if (pInMsg->type == HUB_MSG_TYPE_LINE_DATA) {
    _ASSERTE(pInMsg->u.buf.pBuf != NULL || pInMsg->u.buf.size == 0);

    DWORD size = pInMsg->u.buf.size;

    if (size == 0)
      return TRUE;

    State *pState = ((Filter *)hFilter)->GetState(nFromPort);

    if (!pState)
      return FALSE;

    if (!pState->waitAwakSeq)
      return TRUE;

    BYTE *pBuf = pInMsg->u.buf.pBuf;
    const BYTE *pAwakSeqNext = pState->pAwakSeqNext;

    for ( ; *pAwakSeqNext && size ; size--) {
      if (*pAwakSeqNext == *pBuf++)
        pAwakSeqNext++;
      else
        pAwakSeqNext = ((Filter *)hFilter)->pAwakSeq;
    }

    if (!*pAwakSeqNext) {
      pState->waitAwakSeq = FALSE;

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

      pState->connectSent = TRUE;
    } else {
      pInMsg->u.buf.size = 0;
      pState->pAwakSeqNext = pAwakSeqNext;
    }
  }
  else
  if (pInMsg->type == HUB_MSG_TYPE_CONNECT) {
    if (pInMsg->u.val) {
      // discard CONNECT(TRUE) from the input stream
      if (!pMsgReplaceNone(pInMsg, HUB_MSG_TYPE_EMPTY))
        return FALSE;
    } else {
      State *pState = ((Filter *)hFilter)->GetState(nFromPort);

      if (!pState)
        return FALSE;

      // discard CONNECT(FALSE) from the input stream
      // if CONNECT(TRUE) was not added yet
      if (pState->connectSent) {
        pState->connectSent = FALSE;
      } else {
        if (!pMsgReplaceNone(pInMsg, HUB_MSG_TYPE_EMPTY))
          return FALSE;
      }

      // start awakening sequence waiting
      pState->StartAwakSeq(((Filter *)hFilter)->pAwakSeq);
    }
  }

  return pInMsg != NULL;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK OutMethod(
    HFILTER hFilter,
    int /*nFromPort*/,
    int nToPort,
    HUB_MSG *pOutMsg)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(pOutMsg != NULL);


  if (pOutMsg->type == HUB_MSG_TYPE_CONNECT) {
    State *pState = ((Filter *)hFilter)->GetState(nToPort);

    if (!pState)
      return FALSE;

    if (pOutMsg->u.val) {
      pState->connectionCounter++;

      _ASSERTE(pState->connectionCounter > 0);
    } else {
      _ASSERTE(pState->connectionCounter > 0);

      if (--pState->connectionCounter <= 0)
        pState->StartAwakSeq(((Filter *)hFilter)->pAwakSeq);
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
  if (!ROUTINE_IS_VALID(pHubRoutines, pMsgReplaceVal) ||
      !ROUTINE_IS_VALID(pHubRoutines, pMsgReplaceNone) ||
      !ROUTINE_IS_VALID(pHubRoutines, pMsgInsertNone))
  {
    return NULL;
  }

  pMsgReplaceVal = pHubRoutines->pMsgReplaceVal;
  pMsgReplaceNone = pHubRoutines->pMsgReplaceNone;
  pMsgInsertNone = pHubRoutines->pMsgInsertNone;

  return plugins;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
