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
#include "comparams.h"
#include "comhub.h"
#include "utils.h"

///////////////////////////////////////////////////////////////
static void Usage(const char *pProgName)
{
  cerr
  << "Usage:" << endl
  << "  " << pProgName << " [options] \\\\.\\<port0> [options] [\\\\.\\<port1> ...]" << endl
  << endl
  << "Common options:" << endl
  << "  --help                   - show this help." << endl
  << endl
  << "COM port options:" << endl
  << "  --baud=<b>               - set baud rate to <b> (" << ComParams().BaudRateStr() << " by default)," << endl
  << "                             where <b> is " << ComParams::BaudRateLst() << "." << endl
  << "  --data=<d>               - set data bits to <d> (" << ComParams().ByteSizeStr() << " by default)," << endl
  << "                             where <d> is " << ComParams::ByteSizeLst() << "." << endl
  << "  --parity=<p>             - set parity to <p> (" << ComParams().ParityStr() << " by default)," << endl
  << "                             where <p> is" << endl
  << "                             " << ComParams::ParityLst() << "." << endl
  << "  --stop=<s>               - set stop bits to <s> (" << ComParams().StopBitsStr() << " by default)," << endl
  << "                             where <s> is " << ComParams::StopBitsLst() << "." << endl
  << "  --octs=<c>               - set CTS handshaking on output to <c>" << endl
  << "                             (" << ComParams().OutCtsStr() << " by default), where <c> is" << endl
  << "                             " << ComParams::OutCtsLst() << "." << endl
  << "  --odsr=<c>               - set DSR handshaking on output to <c>" << endl
  << "                             (" << ComParams().OutDsrStr() << " by default), where <c> is" << endl
  << "                             " << ComParams::OutDsrLst() << "." << endl
  << "  --ox=<c>                 - set XON/XOFF handshaking on output to <c>" << endl
  << "                             (" << ComParams().OutXStr() << " by default), where <c> is" << endl
  << "                             " << ComParams::OutXLst() << "." << endl
  << "  --ix=<c>                 - set XON/XOFF handshaking on input to <c>" << endl
  << "                             (" << ComParams().InXStr() << " by default), where <c> is" << endl
  << "                             " << ComParams::InXLst() << "." << endl
  << "                             If XON/XOFF handshaking on input is enabled for" << endl
  << "                             the port then XON/XOFF characters will be" << endl
  << "                             discarded from output to this port." << endl
  << "  --idsr=<c>               - set DSR sensitivity on input to <c>" << endl
  << "                             (" << ComParams().InDsrStr() << " by default), where <c> is" << endl
  << "                             " << ComParams::InDsrLst() << "." << endl
  << "  --ito=<t>                - set read interval timeout to <t> (" << ComParams().IntervalTimeoutStr() << " by default)," << endl
  << "                             where <t> is " << ComParams::IntervalTimeoutLst() << "." << endl
  << endl
  << "  The value c[urrent] above means to use current COM port settings." << endl
  << endl
  << "Route options:" << endl
  << "  --route=<LstR>:<LstL>    - send data received from any port from <LstR> to" << endl
  << "                             all ports (except this port) from <LstL>." << endl
  << "  --bi-route=<LstR>:<LstL> - send data received from any port from <LstR> to" << endl
  << "                             all ports (except this port) from <LstL> and vice" << endl
  << "                             versa." << endl
  << "  --echo-route=<Lst>       - send data received from any port from <Lst> back" << endl
  << "                             to this port." << endl
  << "  --no-route=<LstR>:<LstL> - do not send data received from any port from" << endl
  << "                             <LstR> to ports from <LstL>." << endl
  << endl
  << "  The syntax of <LstR>, <LstL> and <Lst> above: <P1>[,<P2>...]" << endl
  << "  The <Pn> above is a zero based position number of port or All." << endl
  << "  If no any route option specified, then the options --route=0:All --route=1:0" << endl
  << "  used by default (route data from first port to all ports and from second" << endl
  << "  port to first port)." << endl
  << endl
  << "Examples:" << endl
  << "  " << pProgName << " \\\\.\\COM1 \\\\.\\CNCB1 \\\\.\\CNCB2" << endl
  << "  " << pProgName << " --route=All:All \\\\.\\CNCB0 \\\\.\\CNCB1 \\\\.\\CNCB2" << endl
  << "  " << pProgName << " --baud=9600 \\\\.\\COM1 --baud=19200 \\\\.\\CNCB1" << endl
  << "  " << pProgName << " --echo-route=0 \\\\.\\COM2" << endl
  ;
  exit(1);
}
///////////////////////////////////////////////////////////////
static BOOL EchoRoute(ComHub &hub, const char *pList)
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
      i = -1;
    } else if (!StrToInt(p, &i) || i < 0 || i >= hub.NumPorts()) {
      res = FALSE;
      break;
    }

    hub.RouteData(i, i, FALSE, FALSE);
  }

  free(pTmpList);

  return res;
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

      hub.RouteData(iFrom, iTo, noRoute, TRUE);
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

  ComHub hub;

  for (i = 1 ; i < argc ; i++) {
    if (!GetParam(argv[i], "--")) {
      if (!hub.Add(argv[i]))
        return 1;
    }
  }

  BOOL defaultRouteData = TRUE;
  BOOL defaultRouteFlowControl = TRUE;
  int plugged = 0;

  char **pArgs;
  ComParams comParams;

  for (pArgs = &argv[1] ; argc > 1 ; pArgs++, argc--) {
    const char *pArg = GetParam(*pArgs, "--");

    if (!pArg) {
      if (!hub.PlugIn(plugged++, *pArgs, comParams))
        exit(1);

      continue;
    }

    const char *pParam;

    if ((pParam = GetParam(pArg, "help")) != NULL && *pParam == 0) {
      Usage(argv[0]);
    } else
    if ((pParam = GetParam(pArg, "baud=")) != NULL) {
      if (!comParams.SetBaudRate(pParam)) {
        cerr << "Unknown baud rate value " << pParam << endl;
        exit(1);
      }
    } else
    if ((pParam = GetParam(pArg, "data=")) != NULL) {
      if (!comParams.SetByteSize(pParam)) {
        cerr << "Unknown data bits value " << pParam << endl;
        exit(1);
      }
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
    if ((pParam = GetParam(pArg, "octs=")) != NULL) {
      if (!comParams.SetOutCts(pParam)) {
        cerr << "Unknown CTS handshaking on output value " << pParam << endl;
        exit(1);
      }
    } else
    if ((pParam = GetParam(pArg, "odsr=")) != NULL) {
      if (!comParams.SetOutDsr(pParam)) {
        cerr << "Unknown DSR handshaking on output value " << pParam << endl;
        exit(1);
      }
    } else
    if ((pParam = GetParam(pArg, "ox=")) != NULL) {
      if (!comParams.SetOutX(pParam)) {
        cerr << "Unknown XON/XOFF handshaking on output value " << pParam << endl;
        exit(1);
      }
    } else
    if ((pParam = GetParam(pArg, "ix=")) != NULL) {
      if (!comParams.SetInX(pParam)) {
        cerr << "Unknown XON/XOFF handshaking on input value " << pParam << endl;
        exit(1);
      }
    } else
    if ((pParam = GetParam(pArg, "idsr=")) != NULL) {
      if (!comParams.SetInDsr(pParam)) {
        cerr << "Unknown DSR sensitivity value " << pParam << endl;
        exit(1);
      }
    } else
    if ((pParam = GetParam(pArg, "ito=")) != NULL) {
      if (!comParams.SetIntervalTimeout(pParam)) {
        cerr << "Unknown read interval timeout value " << pParam << endl;
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
    } else
    if ((pParam = GetParam(pArg, "echo-route=")) != NULL) {
      defaultRouteData = FALSE;
      EchoRoute(hub, pParam);
    } else {
      cerr << "Unknown option " << pArg << endl;
      exit(1);
    }
  }

  if (plugged < 2) {
    if (plugged < 1)
      Usage(argv[0]);
  }
  else
  if (defaultRouteData) {
    hub.RouteData(0, -1, FALSE, TRUE);
    hub.RouteData(1, 0, FALSE, TRUE);
  }

  if (defaultRouteFlowControl) {
    hub.RouteFlowControl(FALSE);
  } else {
  }

  hub.RouteReport();

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
