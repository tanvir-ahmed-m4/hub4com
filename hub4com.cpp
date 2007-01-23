/*
 * $Id$
 *
 * Copyright (c) 2006-2007 Vyacheslav Frolov
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
 * Revision 1.1  2007/01/23 09:13:10  vfrolov
 * Initial revision
 *
 *
 */

#include "precomp.h"
#include "comparams.h"
#include "comhub.h"
#include "utils.h"

///////////////////////////////////////////////////////////////
static void Usage(const char *pProgName)
{
  cerr
  << "Usage:" << endl
  << "  " << pProgName << " [options] \\\\.\\<port0> \\\\.\\<port1> ..." << endl
  << endl
  << "Common options:" << endl
  << "  --help                   - show this help." << endl
  << endl
  << "COM port options:" << endl
  << "  --baud=<b>               - set baud rate to <b> (default is " << ComParams().BaudRate() << ")," << endl
  << "                             where <b> is " << ComParams::BaudRateLst() << "." << endl
  << "  --data=<d>               - set data bits to <d> (default is " << ComParams().ByteSize() << "), where <d> is" << endl
  << "                             " << ComParams::ByteSizeLst() << "." << endl
  << "  --parity=<p>             - set parity to <p> (default is " << ComParams::ParityStr(ComParams().Parity()) << "), where <p> is" << endl
  << "                             " << ComParams::ParityLst() << "." << endl
  << "  --stop=<s>               - set stop bits to <s> (default is " << ComParams::StopBitsStr(ComParams().StopBits()) << "), where <s> is" << endl
  << "                             " << ComParams::StopBitsLst() << "." << endl
  << "  The value d[efault] above means to use current COM port settings." << endl
  << endl
  << "Route options:" << endl
  << "  --route=<LstR>:<LstL>    - send data received from any ports from <LstR> to" << endl
  << "                             all ports from <LstL>." << endl
  << "  --bi-route=<LstR>:<LstL> - send data received from any ports from <LstR> to" << endl
  << "                             all ports from <LstL> and vice versa." << endl
  << "  --no-route=<LstR>:<LstL> - do not send data received from any ports from" << endl
  << "                             <LstR> to ports from <LstL>." << endl
  << "  The syntax of <LstR> and <LstL> above: <P1>[,<P2>...]" << endl
  << "  The <Pn> above is a zero based position number of port or All." << endl
  << "  If no any route option specified, then the options --route=0:All --route=1:0" << endl
  << "  used by default (route data from first port to all ports and from second" << endl
  << "  port to first port)." << endl
  << endl
  << "Examples:" << endl
  << "  " << pProgName << " \\\\.\\COM1 \\\\.\\CNCB1 \\\\.\\CNCB2" << endl
  << "  " << pProgName << " --route=All:All \\\\.\\CNCB0 \\\\.\\CNCB1 \\\\.\\CNCB2" << endl
  ;
  exit(1);
}
///////////////////////////////////////////////////////////////
static BOOL Route(ComHub &hub, const char *pListFrom, const char *pListTo, BOOL noRoute)
{
  char *pTmpListFrom = _strdup(pListFrom);
  char *pTmpListTo = _strdup(pListTo);

  if (!pTmpListFrom || !pTmpListTo) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  BOOL res = TRUE;
  char *pSave1;

  for (char *pFrom = STRTOK_R(pTmpListFrom, ",", &pSave1) ; pFrom ; pFrom = STRTOK_R(NULL, ",", &pSave1)) {
    int iFrom;

    if (_stricmp(pFrom, "All") == 0) {
      iFrom = -1;
    } else if (!StrToInt(pFrom, &iFrom) || iFrom < 0 || iFrom >= hub.NumPorts()) {
      res = FALSE;
      break;
    }

    char *pSave2;

    for (char *pTo = STRTOK_R(pTmpListTo, ",", &pSave2) ; pTo ; pTo = STRTOK_R(NULL, ",", &pSave2)) {
      int iTo;

      if (_stricmp(pTo, "All") == 0) {
        iTo = -1;
      } else if (!StrToInt(pTo, &iTo) || iTo < 0 || iTo >= hub.NumPorts()) {
        res = FALSE;
        break;
      }

      hub.RouteData(iFrom, iTo, noRoute);
    }
  }

  free(pTmpListTo);
  free(pTmpListFrom);

  return res;
}
///////////////////////////////////////////////////////////////
static void Route(ComHub &hub, const char *pParam, BOOL biDirection, BOOL noRoute)
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
      !Route(hub, pListR, pListL, noRoute) ||
      (biDirection && !Route(hub, pListL, pListR, noRoute)))
  {
    cerr << "Invalid route " << pParam << endl;
    exit(1);
  }

  free(pTmp);
}
///////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
  int i;

  for (i = 1 ; i < argc ; i++) {
    if (GetParam(argv[i], "--") == NULL)
      break;
  }

  ComHub hub(argc - i);
  BOOL defaultRouteData = TRUE;

  char **pArgs = &argv[1];
  ComParams comParams;

  while (argc > 1) {
    const char *pArg = GetParam(*pArgs, "--");

    if (!pArg)
      break;

    const char *pParam;

    if ((pParam = GetParam(pArg, "help")) != NULL && *pParam == 0) {
      Usage(argv[0]);
    } else
    if ((pParam = GetParam(pArg, "baud=")) != NULL) {
      comParams.SetBaudRate(pParam);
    } else
    if ((pParam = GetParam(pArg, "data=")) != NULL) {
      comParams.SetByteSize(pParam);
    } else
    if ((pParam = GetParam(pArg, "parity=")) != NULL) {
      if (!comParams.SetParity(pParam)) {
        cerr << "Unknown parity value " << pParam << endl;
        exit(1);
      }
    } else
    if ((pParam = GetParam(pArg, "stop=")) != NULL) {
      if (!comParams.SetStopBits(pParam)) {
        cerr << "Unknown stop bits value " << pParam << endl;
        exit(1);
      }
    } else
    if ((pParam = GetParam(pArg, "route=")) != NULL) {
      defaultRouteData = FALSE;
      Route(hub, pParam, FALSE, FALSE);
    } else
    if ((pParam = GetParam(pArg, "bi-route=")) != NULL) {
      defaultRouteData = FALSE;
      Route(hub, pParam, TRUE, FALSE);
    } else
    if ((pParam = GetParam(pArg, "no-route=")) != NULL) {
      defaultRouteData = FALSE;
      Route(hub, pParam, FALSE, TRUE);
    } else {
      cerr << "Unknown option " << pArg << endl;
      exit(1);
    }

    pArgs++;
    argc--;
  }

  if (argc < 2)
    Usage(argv[0]);

  if (defaultRouteData) {
    hub.RouteData(0, -1, FALSE);
    hub.RouteData(1, 0, FALSE);
  }

  for (i = 1 ; i < argc ; i++) {
    if (!hub.PlugIn(i - 1, pArgs[i - 1], comParams))
      return 1;
  }

  hub.RouteDataReport();

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
