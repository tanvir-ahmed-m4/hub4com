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
static ROUTINE_MSG_INSERT_VAL *pMsgInsertVal = NULL;
static ROUTINE_MSG_REPLACE_NONE *pMsgReplaceNone = NULL;
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
    State() : connect(FALSE) {}

    BOOL connect;
};
///////////////////////////////////////////////////////////////
typedef map<int, State*> PortsMap;
typedef pair<int, State*> PortPair;

class Filter {
  public:
    Filter(int argc, const char *const argv[]);
    State *GetState(int nPort);

    DWORD pin;
    BOOL negative;
    DWORD myEvents;
    DWORD events;

  private:
    PortsMap portsMap;
};

static struct {
  DWORD val;
  const char *pName;
  DWORD events;
} pin_names[] = {
  {MS_CTS_ON,   "cts",  EV_CTS},
  {MS_DSR_ON,   "dsr",  EV_DSR},
  {MS_RLSD_ON,  "dcd",  EV_RLSD},
  {MS_RING_ON,  "ring", EV_RING},
};

Filter::Filter(int argc, const char *const argv[])
  : pin(MS_DSR_ON),
    negative(FALSE)
{
  for (const char *const *pArgs = &argv[1] ; argc > 1 ; pArgs++, argc--) {
    const char *pArg = GetParam(*pArgs, "--");

    if (!pArg) {
      cerr << "Unknown option " << *pArgs << endl;
      continue;
    }

    const char *pParam;

    if ((pParam = GetParam(pArg, "connect=")) != NULL) {
      if (*pParam == '!') {
        negative = TRUE;
        pParam++;
      }
      BOOL found = FALSE;

      for (int i = 0 ; i < sizeof(pin_names)/sizeof(pin_names[0]) ; i++) {
        if (_stricmp(pParam, pin_names[i].pName) == 0) {
          pin = pin_names[i].val;
          found = TRUE;
          break;
        }
      }

      if (!found)
        cerr << "Unknown pin " << pParam << endl;
    } else {
      cerr << "Unknown option " << pArg << endl;
    }
  }

  myEvents = 0;

  for (int i = 0 ; i < sizeof(pin_names)/sizeof(pin_names[0]) ; i++) {
    if ((pin & pin_names[i].val) != 0)
      myEvents |= pin_names[i].events;
  }

  events = myEvents;
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
  "pin2con",
  "Copyright (c) 2008 Vyacheslav Frolov",
  "GNU General Public License",
  "Connect on pin state changing filter",
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
  << "  --connect=[!]<pin>    - <pin> is cts, dsr, dcd or ring (dsr by default)." << endl
  << endl
  << "IN method input data stream description:" << endl
  << "  CONNECT(TRUE)         - it will be discarded from stream." << endl
  << "  CONNECT(FALSE)        - it will be discarded from stream." << endl
  << "  MODEM_STATUS(<value>) - current state of pins" << endl
  << endl
  << "IN method output data stream description:" << endl
  << "  CONNECT(TRUE)         - will be added on appropriate pin state changing." << endl
  << "  CONNECT(FALSE)        - will be added on appropriate pin state changing." << endl
  << endl
  << "IN method echo data stream description:" << endl
  << "  SET_RT_EVENTS(<mask>) - will be added on CONNECT(TRUE) in input data stream." << endl
  << endl
  << "OUT method input data stream description:" << endl
  << "  SET_RT_EVENTS(<mask>) - <mask> is the events mask." << endl
  << endl
  << "OUT method output data stream description:" << endl
  << "  SET_RT_EVENTS(<mask>) - <mask> is the events mask with event required by this" << endl
  << "                          filter." << endl
  << endl
  << "Examples:" << endl
  << "  " << pProgPath << " --create-filter=" << GetPluginAbout()->pName << " --add-filters=0:" << GetPluginAbout()->pName << " COM1 --use-port-module=tcp 111.11.11.11:1111" << endl
  << "    - wait DSR ON from COM1 and then establish connection to 111.11.11.11:1111" << endl
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
    HUB_MSG **ppEchoMsg)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(pInMsg != NULL);
  _ASSERTE(ppEchoMsg != NULL);
  _ASSERTE(*ppEchoMsg == NULL);

  if (pInMsg->type == HUB_MSG_TYPE_CONNECT) {
    if (pInMsg->u.val) {
      *ppEchoMsg = pMsgInsertVal(NULL,
                                 HUB_MSG_TYPE_SET_RT_EVENTS,
                                 ((Filter *)hFilter)->events);

      if (!*ppEchoMsg)
        return FALSE;
    }

    // discard any CONNECT messages from the input stream
    pMsgReplaceNone(pInMsg, HUB_MSG_TYPE_EMPTY);
  }
  else
  if (pInMsg->type == HUB_MSG_TYPE_MODEM_STATUS) {
    BOOL connect = ((pInMsg->u.val & ((Filter *)hFilter)->pin) != 0);

    if (((Filter *)hFilter)->negative)
      connect = !connect;

    State *pState = ((Filter *)hFilter)->GetState(nFromPort);

    if (!pState)
      return FALSE;

    if (pState->connect != connect) {
      pState->connect = connect;

      if (connect) {
        pInMsg = pMsgInsertVal(pInMsg, HUB_MSG_TYPE_CONNECT, TRUE);
      } else {
        pInMsg = pMsgInsertVal(pInMsg, HUB_MSG_TYPE_CONNECT, FALSE);
      }
    }
  }

  return pInMsg != NULL;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK OutMethod(
    HFILTER hFilter,
    int /*nFromPort*/,
    int /*nToPort*/,
    HUB_MSG *pOutMsg)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(pOutMsg != NULL);

  if (pOutMsg->type == HUB_MSG_TYPE_SET_RT_EVENTS) {
    // Add event required by this filter
    pOutMsg->u.val |= ((Filter *)hFilter)->myEvents;
    ((Filter *)hFilter)->events = pOutMsg->u.val;
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
  if (!ROUTINE_IS_VALID(pHubRoutines, pMsgInsertVal) ||
      !ROUTINE_IS_VALID(pHubRoutines, pMsgReplaceNone))
  {
    return NULL;
  }

  pMsgInsertVal = pHubRoutines->pMsgInsertVal;
  pMsgReplaceNone = pHubRoutines->pMsgReplaceNone;

  return plugins;
}
///////////////////////////////////////////////////////////////
