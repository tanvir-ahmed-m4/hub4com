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
namespace PortConnector {
///////////////////////////////////////////////////////////////
#include "comport.h"
#include "import.h"
///////////////////////////////////////////////////////////////
class Params
{
  public:
    void AddPort(ComPort *pPort) { ports.insert(pPort); }
    void Connect(const char *pParam, BOOL biDirection);
    void Connect() const;

  protected:
    typedef set<string> PortNames;
    typedef map<string, PortNames> PortNameMap;
    typedef pair<string, PortNames> PortNamePair;

    void Connect(PortNameMap &portMap, const PortNames &listR, const PortNames &listL);
    void Connect(BOOL flowControl) const;
    ComPort *Find(const string &name) const;

    Ports ports;

    PortNameMap portDataMap;
    PortNameMap portFlowControlMap;
};

ComPort *Params::Find(const string &name) const
{
  for (Ports::const_iterator i = ports.begin() ; i != ports.end() ; i++) {
    if ((*i)->Name() == name)
      return *i;
  }

  return NULL;
}

void Params::Connect() const
{
  Connect(FALSE);
  Connect(TRUE);
}

void Params::Connect(BOOL flowControl) const
{
  const PortNameMap &portMap = (flowControl ? portFlowControlMap : portDataMap);

  for (PortNameMap::const_iterator i = portMap.begin() ; i != portMap.end() ; i++) {
    cout << "Connect fake port " << (flowControl ? "flow control" : "data") << " " << i->first << " -->";

    ComPort *pFrom = Find(i->first);

    if (!pFrom) {
      cout << endl;
      cerr << "Can't find fake port " << i->first << endl;
      exit(1);
    }

    for (PortNames::const_iterator j = i->second.begin() ; j != i->second.end() ; j++) {
      cout << " " << *j;

      ComPort *pTo = Find(*j);

      if (!pTo) {
        cout << endl;
        cerr << "Can't find fake port " << *j << endl;
        exit(1);
      }

      if (flowControl)
        pFrom->ConnectFlowControlPort(pTo);
      else
        pFrom->ConnectDataPort(pTo);
    }

    cout << endl;
  }
}

void Params::Connect(PortNameMap &portMap, const PortNames &listR, const PortNames &listL)
{
  for (PortNames::const_iterator i = listR.begin() ; i != listR.end() ; i++) {
    PortNameMap::iterator iPair = portMap.find(*i);

    if (iPair == portMap.end()) {
      portMap.insert(PortNamePair(*i, PortNames()));

      iPair = portMap.find(*i);

      if (iPair == portMap.end()) {
        cerr << "Can't add port " << *i << endl;
        exit(2);
      }
    }

    for (PortNames::const_iterator j = listL.begin() ; j != listL.end() ; j++)
      iPair->second.insert(*j);
  }
}

void Params::Connect(const char *pParam, BOOL biDirection)
{
  string par(pParam);

  PortNames listR;

  for (;;) {
    string::size_type n = par.find_first_of(",:");

    if (n == 0) {
      cerr << "Empty token in " << pParam << endl;
      exit(1);
    }

    if (n == string::npos) {
      cerr << "Missing colon in " << pParam << endl;
      exit(1);
    }

    listR.insert(par.substr(0, n));

    BOOL last = (par[n] == ':');

    par.erase(0, n + 1);

    if (last)
      break;
  }

  PortNames listL;

  for (;;) {
    string::size_type n = par.find_first_of(",");

    if (n == 0 || par.empty()) {
      cerr << "Empty token in " << pParam << endl;
      exit(1);
    }

    listL.insert(par.substr(0, n));

    if (n == string::npos)
      break;

    par.erase(0, n + 1);
  }

  Connect(portDataMap, listR, listL);
  Connect(portFlowControlMap, listL, listR);

  if (biDirection) {
    Connect(portDataMap, listL, listR);
    Connect(portFlowControlMap, listR, listL);
  }
}
///////////////////////////////////////////////////////////////
static const char *GetParam(const char *pArg, const char *pPattern)
{
  size_t lenPattern = strlen(pPattern);

  if (_strnicmp(pArg, pPattern, lenPattern) != 0)
    return NULL;

  return pArg + lenPattern;
}
///////////////////////////////////////////////////////////////
static PLUGIN_TYPE CALLBACK GetPluginType()
{
  return PLUGIN_TYPE_DRIVER;
}
///////////////////////////////////////////////////////////////
static const PLUGIN_ABOUT_A about = {
  sizeof(PLUGIN_ABOUT_A),
  "connector",
  "Copyright (c) 2008 Vyacheslav Frolov",
  "GNU General Public License",
  "Connectable fake port driver",
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
  << "  " << pProgPath << " ... --use-driver=" << GetPluginAbout()->pName << " <port1> <port2> ..." << endl
  << endl
  << "Connect options:" << endl
  << "  --connect=<LstR>:<LstL>     - handle data sent to any port listed in <LstR>" << endl
  << "                                as data received by all ports listed in <LstL>." << endl
  << "  --bi-connect=<LstR>:<LstL>  - handle data sent to any port listed in <LstR>" << endl
  << "                                as data received by all ports listed in <LstL>" << endl
  << "                                and vice versa." << endl
  << endl
  << "The syntax of <LstR> and <LstL> above is <port1>[,<port2>...], where <portN> is" << endl
  << "a name of connectable fake port." << endl
  << endl
  << "Output data stream description:" << endl
  << "  LINE_DATA(<data>)        - write <data> to serial port." << endl
  << "  CONNECT(TRUE)            - serial port started." << endl
  << endl
  << "Input data stream description:" << endl
  << "  LINE_DATA(<data>)        - readed <data> from serial port." << endl
  << "  CONNECT(TRUE)            - serial port started." << endl
  << endl
  << "Examples:" << endl
  ;
}
///////////////////////////////////////////////////////////////
static HCONFIG CALLBACK ConfigStart()
{
  Params *pParams = new Params;

  if (!pParams) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  return (HCONFIG)pParams;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK Config(
    HCONFIG hConfig,
    const char *pArg)
{
  _ASSERTE(hConfig != NULL);

  const char *pParam;

  if ((pParam = GetParam(pArg, "--connect=")) != NULL)
    ((Params *)hConfig)->Connect(pParam, FALSE);
  else
  if ((pParam = GetParam(pArg, "--bi-connect=")) != NULL)
    ((Params *)hConfig)->Connect(pParam, TRUE);
  else
    return FALSE;

  return TRUE;
}
///////////////////////////////////////////////////////////////
static void CALLBACK ConfigStop(
    HCONFIG hConfig)
{
  _ASSERTE(hConfig != NULL);

  ((Params *)hConfig)->Connect();

  delete (Params *)hConfig;
}
///////////////////////////////////////////////////////////////
static HPORT CALLBACK Create(
    HCONFIG hConfig,
    const char *pPath)
{
  _ASSERTE(hConfig != NULL);

  ComPort *pPort = new ComPort(pPath);

  if (!pPort)
    return NULL;

  ((Params *)hConfig)->AddPort(pPort);

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
static BOOL CALLBACK Init(
    HPORT hPort,
    HMASTERPORT hMasterPort)
{
  _ASSERTE(hPort != NULL);
  _ASSERTE(hMasterPort != NULL);

  return ((ComPort *)hPort)->Init(hMasterPort);
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK FakeReadFilter(
    HPORT hPort,
    HUB_MSG *pMsg)
{
  _ASSERTE(hPort != NULL);
  _ASSERTE(pMsg != NULL);

  return ((ComPort *)hPort)->FakeReadFilter(pMsg);
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
  NULL,           // SetPortName
  Init,
  NULL,           // Start
  FakeReadFilter,
  Write,
  NULL,           // LostReport
};

static const PLUGIN_ROUTINES_A *const plugins[] = {
  (const PLUGIN_ROUTINES_A *)&routines,
  NULL
};
///////////////////////////////////////////////////////////////
ROUTINE_BUF_ALLOC *pBufAlloc;
ROUTINE_ON_READ *pOnRead;
///////////////////////////////////////////////////////////////
PLUGIN_INIT_A InitA;
const PLUGIN_ROUTINES_A *const * CALLBACK InitA(
    const HUB_ROUTINES_A * pHubRoutines)
{
  if (!ROUTINE_IS_VALID(pHubRoutines, pBufAlloc) ||
      !ROUTINE_IS_VALID(pHubRoutines, pOnRead))
  {
    return NULL;
  }

  pBufAlloc = pHubRoutines->pBufAlloc;
  pOnRead = pHubRoutines->pOnRead;

  return plugins;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
