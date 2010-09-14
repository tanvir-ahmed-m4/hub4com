/*
 * $Id$
 *
 * Copyright (c) 2008-2010 Vyacheslav Frolov
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
 * Revision 1.15  2010/09/14 16:31:50  vfrolov
 * Implemented --write-limit=0 to disable writing to the port
 *
 * Revision 1.14  2009/02/20 18:32:35  vfrolov
 * Added info about location of options
 *
 * Revision 1.13  2008/11/27 16:25:08  vfrolov
 * Improved write buffering
 *
 * Revision 1.12  2008/11/27 13:44:52  vfrolov
 * Added --write-limit option
 *
 * Revision 1.11  2008/11/24 16:30:56  vfrolov
 * Removed pOnXoffXon
 *
 * Revision 1.10  2008/11/24 12:37:00  vfrolov
 * Changed plugin API
 *
 * Revision 1.9  2008/11/13 07:35:10  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.8  2008/08/28 10:24:35  vfrolov
 * Removed linking with ....utils.h and ....utils.cpp
 *
 * Revision 1.7  2008/08/20 14:30:19  vfrolov
 * Redesigned serial port options
 *
 * Revision 1.6  2008/08/15 12:44:59  vfrolov
 * Added fake read filter method to ports
 *
 * Revision 1.5  2008/08/13 14:33:18  vfrolov
 * Fixed Help
 *
 * Revision 1.4  2008/04/14 07:32:04  vfrolov
 * Renamed option --use-port-module to --use-driver
 *
 * Revision 1.3  2008/04/07 12:28:03  vfrolov
 * Replaced --rt-events option by SET_RT_EVENTS message
 *
 * Revision 1.2  2008/03/28 15:55:09  vfrolov
 * Fixed help
 *
 * Revision 1.1  2008/03/26 08:41:18  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "../plugins_api.h"
///////////////////////////////////////////////////////////////
namespace PortSerial {
///////////////////////////////////////////////////////////////
#include "comparams.h"
#include "comport.h"
#include "import.h"
///////////////////////////////////////////////////////////////
static ROUTINE_GET_ARG_INFO_A *pGetArgInfo;
///////////////////////////////////////////////////////////////
static const char *GetParam(const char *pArg, const char *pPattern)
{
  size_t lenPattern = strlen(pPattern);

  if (_strnicmp(pArg, pPattern, lenPattern) != 0)
    return NULL;

  return pArg + lenPattern;
}
///////////////////////////////////////////////////////////////
static void Diag(const char *pPref, const char *pArg)
{
  cerr << pPref << "'" << pArg << "'";

  string pref(" (");
  string suff;

  const ARG_INFO_A *pInfo = pGetArgInfo(pArg);

  if (ITEM_IS_VALID(pInfo, pFile) && pInfo->pFile != NULL) {
    cerr << pref << "file " << pInfo->pFile;
    pref = ", ";
    suff = ")";
  }

  if (ITEM_IS_VALID(pInfo, iLine) && pInfo->iLine >= 0) {
    cerr << pref << "line " << (pInfo->iLine + 1);
    pref = ", ";
    suff = ")";
  }

  cerr << suff;

  if (ITEM_IS_VALID(pInfo, pReference) && pInfo->pReference != NULL)
    cerr << "," << endl << pInfo->pReference;

  cerr << endl;
}
///////////////////////////////////////////////////////////////
static PLUGIN_TYPE CALLBACK GetPluginType()
{
  return PLUGIN_TYPE_DRIVER;
}
///////////////////////////////////////////////////////////////
static const PLUGIN_ABOUT_A about = {
  sizeof(PLUGIN_ABOUT_A),
  "serial",
  "Copyright (c) 2008 Vyacheslav Frolov",
  "GNU General Public License",
  "Serial port driver",
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
  << "  " << pProgPath << " ... [--use-driver=" << GetPluginAbout()->pName << "] <com port> ..." << endl
  << endl
  << "Options:" << endl
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
  << "  --write-limit=<s>        - set write queue limit to <s> (" << ComParams().WriteQueueLimitStr() << " by default)," << endl
  << "                             where <s> is " << ComParams::WriteQueueLimitLst() << ". The queue" << endl
  << "                             will be purged with data lost on overruning." << endl
  << "                             The value 0 will disable writing to the port." << endl
  << endl
  << "  The value c[urrent] above means to use current COM port settings." << endl
  << endl
  << "Output data stream description:" << endl
  << "  SET_OUT_OPTS(<opts>)     - or'e <opts> with the output data stream options." << endl
  << "  LINE_DATA(<data>)        - write <data> to serial port." << endl
  << "  SET_PIN_STATE(<state>)   - set serial port pins to required state." << endl
  << endl
  << "Input data stream description:" << endl
  << "  LINE_DATA(<data>)        - readed <data> from serial port." << endl
  << "  CONNECT(TRUE)            - serial port started." << endl
  << "  LINE_STATUS(<val>)       - current state of line." << endl
  << "  MODEM_STATUS(<val>)      - current state of modem." << endl
  << endl
  << "Fake read filter input data stream description:" << endl
  << "  GET_IN_OPTS(<pOpts>,<mask>)" << endl
  << "                           - the port removes bits from <mask> in this message" << endl
  << "                             for locally supported input data stream options." << endl
  << endl
  << "Fake read filter output data stream description:" << endl
  << "  GET_IN_OPTS(<pOpts>,<mask>)" << endl
  << "                           - the port adds this message at detecting the" << endl
  << "                             GET_IN_OPTS message in the stream to get the" << endl
  << "                             required input data stream options." << endl
  << endl
  << "Examples:" << endl
  << "  " << pProgPath << " COM1 \\\\.\\CNCB1 \\\\.\\CNCB2" << endl
  << "    - receive data from COM1 and send it to CNCB1 and CNCB2," << endl
  << "      receive data from CNCB1 and send it to COM1." << endl
  << "  " << pProgPath << " --baud=9600 COM1 --baud=19200 COM2" << endl
  << "    - receive data at speed 9600 from COM1 and send it to COM2 at speed 19200," << endl
  << "      receive data at speed 19200 from COM2 and send it to COM1 at speed 9600." << endl
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
      Diag("Invalid baud rate value in ", pArg);
      exit(1);
    }
  } else
  if ((pParam = GetParam(pArg, "--data=")) != NULL) {
    if (!comParams.SetByteSize(pParam)) {
      Diag("Invalid data bits value in ", pArg);
      exit(1);
    }
  } else
  if ((pParam = GetParam(pArg, "--parity=")) != NULL) {
    if (!comParams.SetParity(pParam)) {
      Diag("Invalid parity value in ", pArg);
      exit(1);
    }
  } else
  if ((pParam = GetParam(pArg, "--stop=")) != NULL) {
    if (!comParams.SetStopBits(pParam)) {
      Diag("Invalid stop bits value in ", pArg);
      exit(1);
    }
  } else
  if ((pParam = GetParam(pArg, "--octs=")) != NULL) {
    if (!comParams.SetOutCts(pParam)) {
      Diag("Invalid CTS handshaking on output value in ", pArg);
      exit(1);
    }
  } else
  if ((pParam = GetParam(pArg, "--odsr=")) != NULL) {
    if (!comParams.SetOutDsr(pParam)) {
      Diag("Invalid DSR handshaking on output value in ", pArg);
      exit(1);
    }
  } else
  if ((pParam = GetParam(pArg, "--ox=")) != NULL) {
    if (!comParams.SetOutX(pParam)) {
      Diag("Invalid XON/XOFF handshaking on output value in ", pArg);
      exit(1);
    }
  } else
  if ((pParam = GetParam(pArg, "--ix=")) != NULL) {
    if (!comParams.SetInX(pParam)) {
      Diag("Invalid XON/XOFF handshaking on input value in ", pArg);
      exit(1);
    }
  } else
  if ((pParam = GetParam(pArg, "--idsr=")) != NULL) {
    if (!comParams.SetInDsr(pParam)) {
      Diag("Invalid DSR sensitivity value in ", pArg);
      exit(1);
    }
  } else
  if ((pParam = GetParam(pArg, "--ito=")) != NULL) {
    if (!comParams.SetIntervalTimeout(pParam)) {
      Diag("Invalid read interval timeout value in ", pArg);
      exit(1);
    }
  } else
  if ((pParam = GetParam(pArg, "--write-limit=")) != NULL) {
    if (!comParams.SetWriteQueueLimit(pParam)) {
      Diag("Invalid write limit value in ", pArg);
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
    HMASTERPORT hMasterPort)
{
  _ASSERTE(hPort != NULL);
  _ASSERTE(hMasterPort != NULL);

  return ((ComPort *)hPort)->Init(hMasterPort);
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK Start(HPORT hPort)
{
  _ASSERTE(hPort != NULL);

  return ((ComPort *)hPort)->Start();
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
  FakeReadFilter,
  Write,
  LostReport,
};

static const PLUGIN_ROUTINES_A *const plugins[] = {
  (const PLUGIN_ROUTINES_A *)&routines,
  NULL
};
///////////////////////////////////////////////////////////////
ROUTINE_BUF_ALLOC *pBufAlloc;
ROUTINE_BUF_FREE *pBufFree;
ROUTINE_BUF_APPEND *pBufAppend;
ROUTINE_MSG_INSERT_NONE *pMsgInsertNone;
ROUTINE_ON_READ *pOnRead;
///////////////////////////////////////////////////////////////
PLUGIN_INIT_A InitA;
const PLUGIN_ROUTINES_A *const * CALLBACK InitA(
    const HUB_ROUTINES_A * pHubRoutines)
{
  if (!ROUTINE_IS_VALID(pHubRoutines, pBufAlloc) ||
      !ROUTINE_IS_VALID(pHubRoutines, pBufFree) ||
      !ROUTINE_IS_VALID(pHubRoutines, pBufAppend) ||
      !ROUTINE_IS_VALID(pHubRoutines, pMsgInsertNone) ||
      !ROUTINE_IS_VALID(pHubRoutines, pOnRead) ||
      !ROUTINE_IS_VALID(pHubRoutines, pGetArgInfo))
  {
    return NULL;
  }

  pBufAlloc = pHubRoutines->pBufAlloc;
  pBufFree = pHubRoutines->pBufFree;
  pBufAppend = pHubRoutines->pBufAppend;
  pMsgInsertNone = pHubRoutines->pMsgInsertNone;
  pOnRead = pHubRoutines->pOnRead;
  pGetArgInfo = pHubRoutines->pGetArgInfo;

  return plugins;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
