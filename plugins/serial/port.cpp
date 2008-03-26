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
 * Revision 1.1  2008/03/26 08:41:18  vfrolov
 * Initial revision
 *
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
  "serial",
  "Copyright (c) 2008 Vyacheslav Frolov",
  "GNU General Public License",
  "Serial port",
};

static const PLUGIN_ABOUT_A * CALLBACK GetPluginAbout()
{
  return &about;
}
///////////////////////////////////////////////////////////////
static void CALLBACK Help(const char *pProgPath)
{
  cerr
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
  << "  --rt-events=<LstEv>      - set events that should be handled in real-time to" << endl
  << "                             <LstEv> (" << ComParams().EventsStr() << " by default)." << endl
  << endl
  << "  The value c[urrent] above means to use current COM port settings." << endl
  << "  The syntax of <LstEv> above is" << endl
  << "  " << ComParams::EventsLst() << "." << endl
  << endl
  << "Examples:" << endl
  << "  " << pProgPath << " COM1 \\\\.\\CNCB1 \\\\.\\CNCB2" << endl
  << "  " << pProgPath << " --baud=9600 COM1 --baud=19200 \\\\.\\CNCB1" << endl
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

  if ((pParam = GetParam(pArg, "--baud=")) != NULL) {
    if (!comParams.SetBaudRate(pParam)) {
      cerr << "Unknown baud rate value in " << pArg << endl;
      exit(1);
    }
  } else
  if ((pParam = GetParam(pArg, "--data=")) != NULL) {
    if (!comParams.SetByteSize(pParam)) {
      cerr << "Unknown data bits value in " << pArg << endl;
      exit(1);
    }
  } else
  if ((pParam = GetParam(pArg, "--parity=")) != NULL) {
    if (!comParams.SetParity(pParam)) {
      cerr << "Unknown parity value in " << pArg << endl;
      exit(1);
    }
  } else
  if ((pParam = GetParam(pArg, "--stop=")) != NULL) {
    if (!comParams.SetStopBits(pParam)) {
      cerr << "Unknown stop bits value in " << pArg << endl;
      exit(1);
    }
  } else
  if ((pParam = GetParam(pArg, "--octs=")) != NULL) {
    if (!comParams.SetOutCts(pParam)) {
      cerr << "Unknown CTS handshaking on output value in " << pArg << endl;
      exit(1);
    }
  } else
  if ((pParam = GetParam(pArg, "--odsr=")) != NULL) {
    if (!comParams.SetOutDsr(pParam)) {
      cerr << "Unknown DSR handshaking on output value in " << pArg << endl;
      exit(1);
    }
  } else
  if ((pParam = GetParam(pArg, "--ox=")) != NULL) {
    if (!comParams.SetOutX(pParam)) {
      cerr << "Unknown XON/XOFF handshaking on output value in " << pArg << endl;
      exit(1);
    }
  } else
  if ((pParam = GetParam(pArg, "--ix=")) != NULL) {
    if (!comParams.SetInX(pParam)) {
      cerr << "Unknown XON/XOFF handshaking on input value in " << pArg << endl;
      exit(1);
    }
  } else
  if ((pParam = GetParam(pArg, "--idsr=")) != NULL) {
    if (!comParams.SetInDsr(pParam)) {
      cerr << "Unknown DSR sensitivity value in " << pArg << endl;
      exit(1);
    }
  } else
  if ((pParam = GetParam(pArg, "--ito=")) != NULL) {
    if (!comParams.SetIntervalTimeout(pParam)) {
      cerr << "Unknown read interval timeout value in " << pArg << endl;
      exit(1);
    }
  } else
  if ((pParam = GetParam(pArg, "--rt-events=")) != NULL) {
    if (!comParams.SetEvents(pParam)) {
      cerr << "Unknown events in " << pArg << endl;
      exit(1);
    }
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
static HPORT CALLBACK Create(
    HCONFIG hConfig,
    const char *pPath)
{
  _ASSERTE(hConfig != NULL);

  ComPort *pPort = new ComPort(*(const ComParams *)hConfig, pPath);

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

  return plugins;
}
///////////////////////////////////////////////////////////////
