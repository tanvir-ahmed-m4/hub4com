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
 * Revision 1.2  2008/04/14 07:32:04  vfrolov
 * Renamed option --use-port-module to --use-driver
 *
 * Revision 1.1  2008/03/28 16:05:44  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "../plugins_api.h"
#include "telnet.h"

///////////////////////////////////////////////////////////////
static ROUTINE_MSG_REPLACE_BUF *pMsgReplaceBuf = NULL;
static ROUTINE_MSG_INSERT_BUF *pMsgInsertBuf = NULL;
static ROUTINE_PORT_NAME_A *pPortName = NULL;
///////////////////////////////////////////////////////////////
const char *GetParam(const char *pArg, const char *pPattern)
{
  size_t lenPattern = strlen(pPattern);

  if (_strnicmp(pArg, pPattern, lenPattern) != 0)
    return NULL;

  return pArg + lenPattern;
}
///////////////////////////////////////////////////////////////
typedef map<int, TelnetProtocol*> PortsMap;
typedef pair<int, TelnetProtocol*> PortPair;

class Filter {
  public:
    Filter(int argc, const char *const argv[]);
    void SetHub(HHUB _hHub) { hHub = _hHub; }
    TelnetProtocol *GetProtocol(int nPort);
    void DelProtocol(int nPort);

  private:
    HHUB hHub;
    string terminalType;
    PortsMap portsMap;
};

Filter::Filter(int argc, const char *const argv[])
  : hHub(NULL),
    terminalType("UNKNOWN")
{
  for (const char *const *pArgs = &argv[1] ; argc > 1 ; pArgs++, argc--) {
    const char *pArg = GetParam(*pArgs, "--");

    if (!pArg) {
      cerr << "Unknown option " << *pArgs << endl;
      continue;
    }

    const char *pParam;

    if ((pParam = GetParam(pArg, "terminal=")) != NULL) {
      terminalType = pParam;
    } else {
      cerr << "Unknown option " << pArg << endl;
    }
  }
}

TelnetProtocol *Filter::GetProtocol(int nPort)
{
  PortsMap::iterator iPair = portsMap.find(nPort);

  if (iPair == portsMap.end()) {
      portsMap.insert(PortPair(nPort, NULL));

      iPair = portsMap.find(nPort);

      if (iPair == portsMap.end())
        return NULL;
  }

  if (!iPair->second) {
    iPair->second = new TelnetProtocol(pPortName(hHub, nPort));

    if (iPair->second)
      iPair->second->SetTerminalType(terminalType.c_str());
  }

  return iPair->second;
}

void Filter::DelProtocol(int nPort)
{
  PortsMap::iterator iPair = portsMap.find(nPort);

  if (iPair != portsMap.end()) {
    if (iPair->second)
      delete iPair->second;

    iPair->second = NULL;
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
  "telnet",
  "Copyright (c) 2008 Vyacheslav Frolov",
  "GNU General Public License",
  "Telnet protocol filter",
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
  << "  --terminal=<t>           - use terminal type <t> (RFC 1091)." << endl
  << endl
  << "IN method input data stream description:" << endl
  << "  LINE_DATA(<data>) - <data> is the telnet protocol wrapped raw bytes." << endl
  << "  CONNECT(TRUE) - start telnet protocol engine if it's not started." << endl
  << "  CONNECT(FALSE) - stop telnet protocol engine if it's started." << endl
  << endl
  << "IN method output data stream description:" << endl
  << "  LINE_DATA(<data>) - <data> is the raw bytes." << endl
  << endl
  << "IN method echo data stream description:" << endl
  << "  LINE_DATA(<data>) - <data> is the telnet protocol wrapped raw bytes." << endl
  << endl
  << "OUT method input data stream description:" << endl
  << "  LINE_DATA(<data>) - <data> is the raw bytes." << endl
  << endl
  << "OUT method output data stream description:" << endl
  << "  LINE_DATA(<data>) - <data> is the telnet protocol wrapped raw bytes." << endl
  << endl
  << "Examples:" << endl
  << "  " << pProgPath << " --create-filter=" << GetPluginAbout()->pName << ":\"--terminal=ANSI\" --add-filters=1:" << GetPluginAbout()->pName << " COM1 --use-driver=tcp *your.telnet.server:telnet" << endl
  << "    - use the ANSI terminal connected to the port COM1 for working on the" << endl
  << "      telnet server your.telnet.server." << endl
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

  if (pInMsg->type == HUB_MSG_TYPE_LINE_DATA) {
    _ASSERTE(pInMsg->u.buf.pBuf != NULL || pInMsg->u.buf.size == 0);

    if (pInMsg->u.buf.size == 0)
      return TRUE;

    TelnetProtocol *pTelnetProtocol = ((Filter *)hFilter)->GetProtocol(nFromPort);

    if (!pTelnetProtocol)
      return FALSE;

    pTelnetProtocol->Write(pInMsg->u.buf.pBuf, pInMsg->u.buf.size);

    pInMsg = pMsgReplaceBuf(pInMsg,
                            HUB_MSG_TYPE_LINE_DATA,
                            pTelnetProtocol->RecvData(),
                            pTelnetProtocol->RecvDataLength());
    pTelnetProtocol->RecvDataClear();

    if (!pInMsg)
      return FALSE;

    if (pTelnetProtocol->ReadDataLength()) {
      *ppEchoMsg = pMsgInsertBuf(NULL,
                                 HUB_MSG_TYPE_LINE_DATA,
                                 pTelnetProtocol->ReadData(),
                                 pTelnetProtocol->ReadDataLength());

      pTelnetProtocol->ReadDataClear();

      if (!*ppEchoMsg)
        return FALSE;
    }
  }
  else
  if (pInMsg->type == HUB_MSG_TYPE_CONNECT) {
    if (pInMsg->u.val)
      ((Filter *)hFilter)->GetProtocol(nFromPort);
    else
      ((Filter *)hFilter)->DelProtocol(nFromPort);
  }
  return TRUE;
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

  if (pOutMsg->type == HUB_MSG_TYPE_LINE_DATA) {
    _ASSERTE(pOutMsg->u.buf.pBuf != NULL || pOutMsg->u.buf.size == 0);

    if (pOutMsg->u.buf.size == 0)
      return TRUE;

    TelnetProtocol *pTelnetProtocol = ((Filter *)hFilter)->GetProtocol(nToPort);

    if (!pTelnetProtocol)
      return FALSE;

    pTelnetProtocol->Send(pOutMsg->u.buf.pBuf, pOutMsg->u.buf.size);

    pOutMsg = pMsgReplaceBuf(pOutMsg,
                             HUB_MSG_TYPE_LINE_DATA,
                             pTelnetProtocol->ReadData(),
                             pTelnetProtocol->ReadDataLength());
    pTelnetProtocol->ReadDataClear();

    if (!pOutMsg)
      return FALSE;
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
  Init,
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
  if (!ROUTINE_IS_VALID(pHubRoutines, pMsgReplaceBuf) ||
      !ROUTINE_IS_VALID(pHubRoutines, pMsgInsertBuf) ||
      !ROUTINE_IS_VALID(pHubRoutines, pPortName))
  {
    return NULL;
  }

  pMsgReplaceBuf = pHubRoutines->pMsgReplaceBuf;
  pMsgInsertBuf = pHubRoutines->pMsgInsertBuf;
  pPortName = pHubRoutines->pPortName;

  return plugins;
}
///////////////////////////////////////////////////////////////
