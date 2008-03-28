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
 * Revision 1.2  2008/03/28 16:01:13  vfrolov
 * Fixed Help
 *
 * Revision 1.1  2008/03/27 17:19:18  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "comparams.h"
#include "comport.h"
#include "import.h"
#include "../../utils.h"

///////////////////////////////////////////////////////////////
static PLUGIN_TYPE CALLBACK GetPluginType()
{
  return PLUGIN_TYPE_PORT;
}
///////////////////////////////////////////////////////////////
static const PLUGIN_ABOUT_A about = {
  sizeof(PLUGIN_ABOUT_A),
  "tcp",
  "Copyright (c) 2008 Vyacheslav Frolov",
  "GNU General Public License",
  "TCP port driver",
};

static const PLUGIN_ABOUT_A * CALLBACK GetPluginAbout()
{
  return &about;
}
///////////////////////////////////////////////////////////////
static void CALLBACK Help(const char *pProgPath)
{
  cerr
  << "Usage  (client mode):" << endl
  << "  " << pProgPath << " ... [--use-port-module=" << GetPluginAbout()->pName << "] [*]<host addr>:<host port> ..." << endl
  << "Usage  (server mode):" << endl
  << "  " << pProgPath << " ... [--use-port-module=" << GetPluginAbout()->pName << "] [*]<listen port> ..." << endl
  << endl
  << "  The sign * above means that connection shold be permanent as it's possible." << endl
  << "  In client mode it will force connection to remote host on start or on" << endl
  << "  disconnect." << endl
  << endl
  << "Options:" << endl
  << "  --interface=<if>         - use interface <if>." << endl
  << endl
  << "Output data stream description:" << endl
  << "  LINE_DATA(<data>) - send <data> to remote host." << endl
  << "  CONNECT(TRUE) - increment connection counter." << endl
  << "  CONNECT(FALSE) - decrement connection counter." << endl
  << endl
  << "In client mode if there is not connection to remote host the incrementing of" << endl
  << "the connection counter will force connection to remote host." << endl
  << "If sign * is not used and there is connection to remote host the decrementing" << endl
  << "of the connection counter to 0 will force disconnection from remote host." << endl
  << endl
  << "Input data stream description:" << endl
  << "  LINE_DATA(<data>) - received <data> from remote host." << endl
  << "  CONNECT(TRUE) - connected to remote host." << endl
  << "  CONNECT(FALSE) - disconnected." << endl
  << endl
  << "Examples:" << endl
  << "  " << pProgPath << " --use-port-module=" << GetPluginAbout()->pName << " 1111 222.22.22.22:2222" << endl
  << "    - listen TCP port 1111 and on incoming connection connect to" << endl
  << "      222.22.22.22:2222, receive data from 1111 and send it to" << endl
  << "      222.22.22.22:2222, receive data from 222.22.22.22:2222 and send it to" << endl
  << "      1111, on disconnecting any connection disconnect paired connection." << endl
  << "  " << pProgPath << " --use-port-module=" << GetPluginAbout()->pName << " *111.11.11.11:1111 *222.22.22.22:2222" << endl
  << "    - connect to 111.11.11.11:1111 and connect to 222.22.22.22:2222," << endl
  << "      receive data from 111.11.11.11:1111 and send it to 222.22.22.22:2222," << endl
  << "      receive data from 222.22.22.22:2222 and send it to 111.11.11.11:1111," << endl
  << "      on disconnecting any connection reconnect it." << endl
  << "  " << pProgPath << " --route=All:All --use-port-module=" << GetPluginAbout()->pName << " *1111 *1111 *1111" << endl
  << "    - up to 3 clients can connect to port 2222 and talk each others." << endl
  ;
}
///////////////////////////////////////////////////////////////
static HCONFIG CALLBACK ConfigStart()
{
  ComParams *pComParams = new ComParams;

  if (!pComParams) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  return (HCONFIG)pComParams;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK Config(
    HCONFIG hConfig,
    const char *pArg)
{
  _ASSERTE(hConfig != NULL);

  ComParams &comParams = *(ComParams *)hConfig;

  const char *pParam;

  if ((pParam = GetParam(pArg, "--interface=")) != NULL) {
    comParams.SetIF(pParam);
  } else {
    return FALSE;
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
static void CALLBACK ConfigStop(
    HCONFIG hConfig)
{
  _ASSERTE(hConfig != NULL);

  delete (ComParams *)hConfig;
}
///////////////////////////////////////////////////////////////
static vector<Listener *> *pListeners = NULL;

static HPORT CALLBACK Create(
    HCONFIG hConfig,
    const char *pPath)
{
  _ASSERTE(hConfig != NULL);

  if (!pListeners)
    pListeners = new vector<Listener *>;

  if (!pListeners)
    return NULL;

  ComPort *pPort = new ComPort(*pListeners, *(const ComParams *)hConfig, pPath);

  if (!pPort)
    return NULL;

  return (HPORT)pPort;
}
///////////////////////////////////////////////////////////////
static const char *CALLBACK GetPortName(
    HPORT hPort)
{
  _ASSERTE(hPort != NULL);

  return ((ComPort *)hPort)->Name().c_str();
}
///////////////////////////////////////////////////////////////
static void CALLBACK SetPortName(
    HPORT hPort,
    const char *pName)
{
  _ASSERTE(hPort != NULL);
  _ASSERTE(pName != NULL);

  ((ComPort *)hPort)->Name(pName);
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK Init(
    HPORT hPort,
    HMASTERPORT hMasterPort,
    HHUB hHub)
{
  _ASSERTE(hPort != NULL);
  _ASSERTE(hMasterPort != NULL);
  _ASSERTE(hHub != NULL);

  return ((ComPort *)hPort)->Init(hMasterPort, hHub);
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK Start(HPORT hPort)
{
  _ASSERTE(hPort != NULL);

  return ((ComPort *)hPort)->Start();
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK Write(
    HPORT hPort,
    HUB_MSG *pMsg)
{
  _ASSERTE(hPort != NULL);
  _ASSERTE(pMsg != NULL);

  return ((ComPort *)hPort)->Write(pMsg);
}
///////////////////////////////////////////////////////////////
static void CALLBACK AddXoff(
    HPORT hPort)
{
  _ASSERTE(hPort != NULL);

  ((ComPort *)hPort)->AddXoff(1);
}
///////////////////////////////////////////////////////////////
static void CALLBACK AddXon(
    HPORT hPort)
{
  _ASSERTE(hPort != NULL);

  ((ComPort *)hPort)->AddXoff(-1);
}
///////////////////////////////////////////////////////////////
static void CALLBACK LostReport(
    HPORT hPort)
{
  _ASSERTE(hPort != NULL);

  ((ComPort *)hPort)->LostReport();
}
///////////////////////////////////////////////////////////////
static const PORT_ROUTINES_A routines = {
  sizeof(PORT_ROUTINES_A),
  GetPluginType,
  GetPluginAbout,
  Help,
  ConfigStart,
  Config,
  ConfigStop,
  Create,
  GetPortName,
  SetPortName,
  Init,
  Start,
  Write,
  AddXoff,
  AddXon,
  LostReport,
};

static const PLUGIN_ROUTINES_A *const plugins[] = {
  (const PLUGIN_ROUTINES_A *)&routines,
  NULL
};
///////////////////////////////////////////////////////////////
ROUTINE_BUF_ALLOC *pBufAlloc;
ROUTINE_BUF_FREE *pBufFree;
ROUTINE_ON_XOFF *pOnXoff;
ROUTINE_ON_XON *pOnXon;
ROUTINE_ON_READ *pOnRead;
///////////////////////////////////////////////////////////////
PLUGIN_INIT_A InitA;
const PLUGIN_ROUTINES_A *const * CALLBACK InitA(
    const HUB_ROUTINES_A * pHubRoutines)
{
  if (!ROUTINE_IS_VALID(pHubRoutines, pBufAlloc) ||
      !ROUTINE_IS_VALID(pHubRoutines, pBufFree) ||
      !ROUTINE_IS_VALID(pHubRoutines, pOnXoff) ||
      !ROUTINE_IS_VALID(pHubRoutines, pOnXon) ||
      !ROUTINE_IS_VALID(pHubRoutines, pOnRead))
  {
    return NULL;
  }

  pBufAlloc = pHubRoutines->pBufAlloc;
  pBufFree = pHubRoutines->pBufFree;
  pOnXoff = pHubRoutines->pOnXoff;
  pOnXon = pHubRoutines->pOnXon;
  pOnRead = pHubRoutines->pOnRead;

  WSADATA wsaData;

  WSAStartup(MAKEWORD(1, 1), &wsaData);

  return plugins;
}
///////////////////////////////////////////////////////////////
