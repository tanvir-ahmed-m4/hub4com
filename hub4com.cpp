/*
 * $Id$
 *
 * Copyright (c) 2006-2008 Vyacheslav Frolov
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
 * Revision 1.8  2008/03/26 08:48:18  vfrolov
 * Initial revision
 *
 * Revision 1.7  2008/02/04 10:08:49  vfrolov
 * Fixed <LstR>:<LstL> parsing bug
 *
 * Revision 1.6  2007/12/19 13:46:36  vfrolov
 * Added ability to send data received from port to the same port
 *
 * Revision 1.5  2007/05/14 12:06:37  vfrolov
 * Added read interval timeout option
 *
 * Revision 1.4  2007/02/06 11:53:33  vfrolov
 * Added options --odsr, --ox, --ix and --idsr
 * Added communications error reporting
 *
 * Revision 1.3  2007/02/05 09:33:20  vfrolov
 * Implemented internal flow control
 *
 * Revision 1.2  2007/02/01 12:14:59  vfrolov
 * Redesigned COM port params
 *
 * Revision 1.1  2007/01/23 09:13:10  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "comhub.h"
#include "filters.h"
#include "utils.h"
#include "plugins.h"
#include "route.h"

///////////////////////////////////////////////////////////////
static void Usage(const char *pProgPath, Plugins &plugins)
{
  cerr
  << "Usage:" << endl
  << "  " << pProgPath << " [options] \\\\.\\<port0> [options] [\\\\.\\<port1> ...]" << endl
  << endl
  << "Common options:" << endl
  << "  --load=<file>[:<prms>]   - load arguments from a file (one argument per line)" << endl
  << "                             and insert them to the command line. The syntax of" << endl
  << "                             <prms> is <PRM1>[,<PRM2>...], where <PRMn> will" << endl
  << "                             replace %%n%%." << endl
  << "  --help                   - show this help." << endl
  << "  --help=*                 - show help for all modules." << endl
  << "  --help=<LstM>            - show help for modules listed in <LstM>." << endl
  << endl
  << "  The syntax of <LstM> above is <MID0>[,<MID1>...], where <MIDn> is a module" << endl
  << "  name." << endl
  << endl
  << "Route options:" << endl
  << "  --route=<LstR>:<LstL>    - send data received from any port listed in <LstR>" << endl
  << "                             to all ports (except itself) listed in <LstL>." << endl
  << "  --bi-route=<LstR>:<LstL> - send data received from any port listed in <LstR>" << endl
  << "                             to all ports (except itself) listed in <LstL> and" << endl
  << "                             vice versa." << endl
  << "  --echo-route=<Lst>       - send data received from any port listed in <Lst>" << endl
  << "                             back to itself via all attached filters." << endl
  << "  --no-route=<LstR>:<LstL> - do not send data received from any port listed in" << endl
  << "                             <LstR> to the ports listed in <LstL>." << endl
  << endl
  << "  If no any route option specified, then the options --route=0:All --route=1:0" << endl
  << "  used by default (route data from first port to all ports and from second" << endl
  << "  port to first port)." << endl
  << endl
  << "Filter options:" << endl
  << "  --create-filter=<MID>[,<FID>][:<Args>]" << endl
  << "                           - by using module with type 'filter' and with name" << endl
  << "                             <MID> create a filter with name <FID> (<FID> is" << endl
  << "                             <MID> by default) and put arguments <Args> (if" << endl
  << "                             any) to the filter." << endl
  << "  --add-filters=<Lst>:<LstF>" << endl
  << "                           - attach the filters listed in <LstF> to the ports" << endl
  << "                             listed in <Lst>. These filters will handle the" << endl
  << "                             data by IN method just after receiving from ports" << endl
  << "                             listed in <Lst> or by OUT method just before" << endl
  << "                             sending to ports listed in <Lst>." << endl
  << endl
  << "  The syntax of <LstF> above is <FID0>[.<Method>][,<FID1>[.<Method>]...], where" << endl
  << "  <FIDn> is a filter name and <Method> is IN or OUT. The <FID> w/o <Method> is" << endl
  << "  equivalent to <FID>.IN,<FID>.OUT" << endl
  << endl
  << "Port options:" << endl
  << "  --use-port-module=<MID>  - use module with type 'port' and with name <MID> to" << endl
  << "                             create the following ports (<MID> is serial by" << endl
  << "                             default)." << endl
  << endl
  << "The syntax of <LstR>, <LstL> and <Lst> above is <P1>[,<P2>...], where <Pn> is a" << endl
  << "zero based position number of port or All." << endl
  ;
  plugins.List(cerr);
  cerr
  << endl
  << "Examples:" << endl
  << "  " << pProgPath << " --route=All:All \\\\.\\CNCB0 \\\\.\\CNCB1 \\\\.\\CNCB2" << endl
  << "  " << pProgPath << " --echo-route=0 \\\\.\\COM2" << endl
  ;
}
///////////////////////////////////////////////////////////////
static BOOL EnumPortList(
    ComHub &hub,
    const char *pList,
    BOOL (*pFunc)(ComHub &hub, int iPort, PVOID p0, PVOID p1, PVOID p2),
    PVOID p0 = NULL,
    PVOID p1 = NULL,
    PVOID p2 = NULL)
{
  char *pTmpList = _strdup(pList);

  if (!pTmpList) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  BOOL res = TRUE;
  char *pSave;

  for (char *p = STRTOK_R(pTmpList, ",", &pSave) ; p ; p = STRTOK_R(NULL, ",", &pSave)) {
    int i;

    if (_stricmp(p, "All") == 0) {
      for (i = 0 ; i < hub.NumPorts() ; i++) {
        if (!pFunc(hub, i, p0, p1, p2))
          res = FALSE;
      }
    } else if (StrToInt(p, &i) && i >= 0 && i < hub.NumPorts()) {
      if (!pFunc(hub, i, p0, p1, p2))
        res = FALSE;
    } else {
      res = FALSE;
    }
  }

  free(pTmpList);

  return res;
}
///////////////////////////////////////////////////////////////
static BOOL EchoRoute(ComHub &/*hub*/, int iPort, PVOID pMap, PVOID /*p1*/, PVOID /*p2*/)
{
  AddRoute(*(PortNumMap *)pMap, iPort, iPort, FALSE, FALSE);
  return TRUE;
}

static void EchoRoute(ComHub &hub, const char *pList, PortNumMap &map)
{
  if (!EnumPortList(hub, pList, EchoRoute, &map)) {
    cerr << "Invalid echo route " << pList << endl;
    exit(1);
  }
}
///////////////////////////////////////////////////////////////
static BOOL Route(ComHub &/*hub*/, int iTo, PVOID pIFrom, PVOID pNoRoute, PVOID pMap)
{
  AddRoute(*(PortNumMap *)pMap, *(int *)pIFrom, iTo, *(BOOL *)pNoRoute, TRUE);
  return TRUE;
}

static BOOL RouteList(ComHub &hub, int iFrom, PVOID pListTo, PVOID pNoRoute, PVOID pMap)
{
  return EnumPortList(hub, (const char *)pListTo, Route, &iFrom, pNoRoute, pMap);
}

static BOOL Route(
    ComHub &hub,
    const char *pListFrom,
    const char *pListTo,
    BOOL noRoute,
    PortNumMap &map)
{
  return EnumPortList(hub, pListFrom, RouteList, (PVOID)pListTo, &noRoute, &map);
}
///////////////////////////////////////////////////////////////
static void Route(
    ComHub &hub,
    const char *pParam,
    BOOL biDirection,
    BOOL noRoute,
    PortNumMap &map)
{
  char *pTmp = _strdup(pParam);

  if (!pTmp) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  char *pSave;
  const char *pListR = STRTOK_R(pTmp, ":", &pSave);
  const char *pListL = STRTOK_R(NULL, ":", &pSave);

  if (!pListR || !pListL ||
      !Route(hub, pListR, pListL, noRoute, map) ||
      (biDirection && !Route(hub, pListL, pListR, noRoute, map)))
  {
    cerr << "Invalid route " << pParam << endl;
    exit(1);
  }

  free(pTmp);
}
///////////////////////////////////////////////////////////////
static void CreateFilter(
    const Plugins &plugins,
    Filters &filter,
    const char *pParam)
{
  char *pTmp = _strdup(pParam);

  if (!pTmp) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  char *pSave;

  char *pPlugin = STRTOK_R(pTmp, ":", &pSave);
  char *pArgs = STRTOK_R(NULL, "", &pSave);

  if (!pPlugin || !*pPlugin) {
    cerr << "No module name." << endl;
    exit(1);
  }

  const char *pPluginName = STRTOK_R(pPlugin, ",", &pSave);

  if (!pPluginName || !*pPluginName) {
    cerr << "No module name." << endl;
    exit(1);
  }

  HCONFIG hConfig;

  const FILTER_ROUTINES_A *pFltRoutines =
      (const FILTER_ROUTINES_A *)plugins.GetRoutines(PLUGIN_TYPE_FILTER, pPluginName, &hConfig);

  if (!pFltRoutines) {
    cerr << "No filter module " << pPluginName << endl;
    exit(1);
  }

  const char *pFilterName = STRTOK_R(NULL, "", &pSave);

  if (!pFilterName || !*pFilterName)
    pFilterName = pPluginName;

  if (!filter.CreateFilter(pFltRoutines, pFilterName, hConfig, pArgs)) {
    cerr << "Invalid filter " << pParam << endl;
    exit(1);
  }

  free(pTmp);
}
///////////////////////////////////////////////////////////////
static BOOL AddFilters(ComHub &/*hub*/, int iPort, PVOID pFilters, PVOID pListFlt, PVOID /*p2*/)
{
  char *pTmpList = _strdup((const char *)pListFlt);

  if (!pTmpList) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  char *pSave;

  for (char *pFilter = STRTOK_R(pTmpList, ",", &pSave) ;
       pFilter ;
       pFilter = STRTOK_R(NULL, ",", &pSave))
  {
    string filter(pFilter);
    string::size_type dot = filter.rfind('.');
    string method(dot != filter.npos ? filter.substr(dot) : "");

    if (method == ".IN") {
      if (!((Filters *)pFilters)->AddFilter(iPort, filter.substr(0, dot).c_str(), TRUE))
        exit(1);
    }
    else
    if (method == ".OUT") {
      if (!((Filters *)pFilters)->AddFilter(iPort, filter.substr(0, dot).c_str(), FALSE))
        exit(1);
    }
    else {
      if (!((Filters *)pFilters)->AddFilter(iPort, filter.c_str(), TRUE))
        exit(1);
      if (!((Filters *)pFilters)->AddFilter(iPort, filter.c_str(), FALSE))
        exit(1);
    }
  }

  free(pTmpList);

  return TRUE;
}

static void AddFilters(ComHub &hub, Filters &filters, const char *pParam)
{
  char *pTmp = _strdup(pParam);

  if (!pTmp) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  char *pSave;
  const char *pList = STRTOK_R(pTmp, ":", &pSave);
  const char *pListFlt = STRTOK_R(NULL, "", &pSave);

  if (!pList || !*pList || !pListFlt || !*pListFlt) {
    cerr << "Invalid filter parameters " << pParam << endl;
    exit(1);
  }

  if (!EnumPortList(hub, pList, AddFilters, &filters, (PVOID)pListFlt)) {
    cerr << "Can't add filters " << pListFlt << " to ports " << pList << endl;
    exit(1);
  }

  free(pTmp);
}
///////////////////////////////////////////////////////////////
static void Init(ComHub &hub, int argc, const char *const argv[])
{
  Args args(argc - 1, argv + 1);

  for (vector<string>::const_iterator i = args.begin() ; i != args.end() ; i++) {
    if (!GetParam(i->c_str(), "--"))
      hub.Add();
  }

  BOOL defaultRouteData = TRUE;
  BOOL defaultRouteFlowControl = TRUE;
  int plugged = 0;
  Plugins *pPlugins = new Plugins();

  if (!pPlugins) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  Filters *pFilters = NULL;

  PortNumMap routeDataMap;
  PortNumMap routeFlowControlMap;

  const char *pUsePortModule = "serial";

  for (vector<string>::const_iterator i = args.begin() ; i != args.end() ; i++) {
    BOOL ok = pPlugins->Config(i->c_str());
    const char *pArg = GetParam(i->c_str(), "--");

    if (!pArg) {
      HCONFIG hConfig;

      const PORT_ROUTINES_A *pPortRoutines =
          (const PORT_ROUTINES_A *)pPlugins->GetRoutines(PLUGIN_TYPE_PORT, pUsePortModule, &hConfig);

      if (!pPortRoutines) {
        cerr << "No port module " << pUsePortModule << endl;
        exit(1);
      }

      if (!hub.CreatePort(pPortRoutines, plugged++, hConfig, i->c_str()))
        exit(1);

      continue;
    }

    const char *pParam;

    if ((pParam = GetParam(pArg, "help")) != NULL && *pParam == 0) {
      Usage(argv[0], *pPlugins);
      exit(0);
    } else
    if ((pParam = GetParam(pArg, "help=")) != NULL) {
      char *pTmpList = _strdup(pParam);

      if (!pTmpList) {
        cerr << "No enough memory." << endl;
        exit(2);
      }

      char *pSave;

      for (char *p = STRTOK_R(pTmpList, ",", &pSave) ; p ; p = STRTOK_R(NULL, ",", &pSave))
        pPlugins->Help(argv[0], p);

      free(pTmpList);
      exit(0);
    } else
    if ((pParam = GetParam(pArg, "route=")) != NULL) {
      defaultRouteData = FALSE;
      Route(hub, pParam, FALSE, FALSE, routeDataMap);
    } else
    if ((pParam = GetParam(pArg, "bi-route=")) != NULL) {
      defaultRouteData = FALSE;
      Route(hub, pParam, TRUE, FALSE, routeDataMap);
    } else
    if ((pParam = GetParam(pArg, "no-route=")) != NULL) {
      defaultRouteData = FALSE;
      Route(hub, pParam, FALSE, TRUE, routeDataMap);
    } else
    if ((pParam = GetParam(pArg, "echo-route=")) != NULL) {
      defaultRouteData = FALSE;
      EchoRoute(hub, pParam, routeDataMap);
    } else
    if ((pParam = GetParam(pArg, "create-filter=")) != NULL) {
      if (!pFilters)
        pFilters = new Filters(hub);

      if (!pFilters) {
        cerr << "No enough memory." << endl;
        exit(2);
      }

      CreateFilter(*pPlugins, *pFilters, pParam);
    } else
    if ((pParam = GetParam(pArg, "add-filters=")) != NULL) {
      if (!pFilters) {
        cerr << "No create-filter option before " << i->c_str() << endl;
        exit(1);
      }

      AddFilters(hub, *pFilters, pParam);
    } else
    if ((pParam = GetParam(pArg, "use-port-module=")) != NULL) {
      pUsePortModule = pParam;
    } else {
      if (!ok) {
        cerr << "Unknown option " << i->c_str() << endl;
        exit(1);
      }
    }
  }

  delete pPlugins;

  if (plugged < 2) {
    if (plugged < 1) {
      Usage(argv[0], *pPlugins);
      exit(1);
    }
  }
  else
  if (defaultRouteData) {
    Route(hub, "0:All", FALSE, FALSE, routeDataMap);
    Route(hub, "1:0", FALSE, FALSE, routeDataMap);
  }

  if (defaultRouteFlowControl) {
    SetFlowControlRoute(routeFlowControlMap, routeDataMap, FALSE);
  } else {
  }

  hub.SetFlowControlRoute(routeFlowControlMap);
  hub.SetDataRoute(routeDataMap);

  hub.SetFilters(pFilters);
  hub.RouteReport();

  if (pFilters)
    pFilters->Report();
}
///////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
  ComHub hub;

  Init(hub, argc, argv);

  if (hub.StartAll()) {
    DWORD nextReportTime = 0;

    for (;;) {
      SleepEx(5000, TRUE);

      DWORD time = GetTickCount();

      if ((nextReportTime - time - 1) > 10000) {
        hub.LostReport();
        nextReportTime = time + 5000;
      }
    }
  }

  return 1;
}
///////////////////////////////////////////////////////////////
