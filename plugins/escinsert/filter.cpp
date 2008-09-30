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
 * Revision 1.1  2008/09/30 08:32:38  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "../plugins_api.h"
#include "../cncext.h"

///////////////////////////////////////////////////////////////
static ROUTINE_MSG_INSERT_BUF *pMsgInsertBuf = NULL;
static ROUTINE_MSG_REPLACE_BUF *pMsgReplaceBuf = NULL;
static ROUTINE_PORT_NAME_A *pPortName = NULL;
static ROUTINE_FILTER_NAME_A *pFilterName = NULL;
///////////////////////////////////////////////////////////////
const char *GetParam(const char *pArg, const char *pPattern)
{
  size_t lenPattern = strlen(pPattern);

  if (_strnicmp(pArg, pPattern, lenPattern) != 0)
    return NULL;

  return pArg + lenPattern;
}
///////////////////////////////////////////////////////////////
class Valid {
  public:
    Valid() : isValid(TRUE) {}
    void Invalidate() { isValid = FALSE; }
    BOOL IsValid() const { return isValid; }
  private:
    BOOL isValid;
};
///////////////////////////////////////////////////////////////
class State {
  public:
    State()
    : isConnected(FALSE),
      mstInVal(0),
      rbr(CBR_19200),
      rlc(VAL2LC_BYTESIZE(8)|VAL2LC_PARITY(NOPARITY)|VAL2LC_STOPBITS(ONESTOPBIT))
    {}

    BOOL isConnected;
    BYTE mstInVal;
    ULONG rbr;
    DWORD rlc;
};
///////////////////////////////////////////////////////////////
class Filter : public Valid {
  public:
    Filter(int argc, const char *const argv[]);
    void SetHub(HHUB _hHub) { hHub = _hHub; }
    State *GetState(int nPort);
    const char *PortName(int nPort) const { return pPortName(hHub, nPort); }
    const char *FilterName() const { return pFilterName(hHub, (HFILTER)this); }

    DWORD goInMask;
    BYTE escapeChar;

  private:
    HHUB hHub;

    typedef map<int, State*> PortsMap;
    typedef pair<int, State*> PortPair;

    PortsMap portsMap;
};

Filter::Filter(int argc, const char *const argv[])
  : goInMask(GO_V2O_MODEM_STATUS(MODEM_STATUS_CTS|MODEM_STATUS_DSR|MODEM_STATUS_DCD|MODEM_STATUS_RI) |
             GO_V2O_LINE_STATUS(LINE_STATUS_OE|LINE_STATUS_PE|LINE_STATUS_FE|LINE_STATUS_BI|LINE_STATUS_FIFOERR) |
             GO_RBR_STATUS|GO_RLC_STATUS|GO_BREAK_STATUS),
    escapeChar(0xFF),
    hHub(NULL)
{
  for (const char *const *pArgs = &argv[1] ; argc > 1 ; pArgs++, argc--) {
    const char *pArg = GetParam(*pArgs, "--");

    if (!pArg) {
      cerr << "Unknown option " << *pArgs << endl;
      Invalidate();
      continue;
    }

    {
      cerr << "Unknown option --" << pArg << endl;
      Invalidate();
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
  "escinsert",
  "Copyright (c) 2008 Vyacheslav Frolov",
  "GNU General Public License",
  "Escaped datastream generating filter",
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
  << endl
  << "Examples:" << endl
  << "  " << pProgPath << " --load=,,_END_" << endl
  << "      --create-filter=escparse" << endl
  << "      --add-filters=0:escparse" << endl
  << "      \\\\.\\CNCB0" << endl
  << "      --create-filter=escinsert" << endl
  << "      --add-filters=1:escinsert" << endl
  << "      --use-driver=tcp" << endl
  << "      222.22.22.22:2222" << endl
  << "      _END_" << endl
  << "    - transfer data, signals, baudrate and line control info from serial port" << endl
  << "      CNCB0 to TCP port 222.22.22.22:2222 via escaped datastream." << endl
  ;
}
///////////////////////////////////////////////////////////////
static HFILTER CALLBACK Create(
    HCONFIG /*hConfig*/,
    int argc,
    const char *const argv[])
{
  Filter *pFilter = new Filter(argc, argv);

  if (!pFilter)
    return NULL;

  if (!pFilter->IsValid()) {
    delete pFilter;
    return NULL;
  }

  return (HFILTER)pFilter;
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
static void InsertStatus(
    Filter &filter,
    BYTE code,
    BYTE val,
    HUB_MSG **ppMsg)
{
  BYTE insert[3];

  insert[0] = filter.escapeChar;
  insert[1] = code;
  insert[2] = val;

  *ppMsg = pMsgInsertBuf(*ppMsg,
                         HUB_MSG_TYPE_LINE_DATA,
                         insert,
                         sizeof(insert));
}
///////////////////////////////////////////////////////////////
static void InsertRBR(
    Filter &filter,
    ULONG rbr,
    HUB_MSG **ppMsg)
{
  BYTE insert[2 + sizeof(ULONG)];

  insert[0] = filter.escapeChar;
  insert[1] = C0CE_INSERT_RBR;
  *(ULONG *)&insert[2] = rbr;

  *ppMsg = pMsgInsertBuf(*ppMsg,
                         HUB_MSG_TYPE_LINE_DATA,
                         insert,
                         sizeof(insert));
}
///////////////////////////////////////////////////////////////
static void InsertRLC(
    Filter &filter,
    DWORD rlc,
    HUB_MSG **ppMsg)
{
  BYTE insert[2 + 3];

  insert[0] = filter.escapeChar;
  insert[1] = C0CE_INSERT_RLC;
  insert[2] = LC2VAL_BYTESIZE(rlc);
  insert[3] = LC2VAL_PARITY(rlc);
  insert[4] = LC2VAL_STOPBITS(rlc);

  *ppMsg = pMsgInsertBuf(*ppMsg,
                         HUB_MSG_TYPE_LINE_DATA,
                         insert,
                         sizeof(insert));
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

  switch (pInMsg->type) {
    case HUB_MSG_TYPE_CONNECT: {
      State *pState = ((Filter *)hFilter)->GetState(nFromPort);

      if (!pState)
        return FALSE;

      if (pInMsg->u.val) {
        pState->isConnected = TRUE;
      } else {
        pState->isConnected = FALSE;
        break;
      }

      // init state
      if (((Filter *)hFilter)->goInMask & GO_O2V_MODEM_STATUS(-1))
        InsertStatus(*(Filter *)hFilter, SERIAL_LSRMST_MST, pState->mstInVal, ppEchoMsg);

      if (((Filter *)hFilter)->goInMask & (GO_V2O_LINE_STATUS(-1) | GO_BREAK_STATUS))
        InsertStatus(*(Filter *)hFilter, SERIAL_LSRMST_LSR_NODATA, 0, ppEchoMsg);

      if (((Filter *)hFilter)->goInMask & GO_RBR_STATUS)
        InsertRBR(*(Filter *)hFilter, pState->rbr, ppEchoMsg);

      if (((Filter *)hFilter)->goInMask & GO_RLC_STATUS)
        InsertRLC(*(Filter *)hFilter, pState->rlc, ppEchoMsg);

      break;
    }
  }

  return pInMsg != NULL;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK OutMethod(
    HFILTER hFilter,
    int nFromPort,
    int nToPort,
    HUB_MSG *pOutMsg)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(pOutMsg != NULL);

  switch (pOutMsg->type) {
    case HUB_MSG_TYPE_GET_IN_OPTS: {
      _ASSERTE(pOutMsg->u.pv.pVal != NULL);

      // or'e with the required mask to get line status and modem status
      *pOutMsg->u.pv.pVal |= (((Filter *)hFilter)->goInMask & pOutMsg->u.pv.val);
      break;
    }
    case HUB_MSG_TYPE_FAIL_IN_OPTS: {
      DWORD fail_options = (pOutMsg->u.val & ((Filter *)hFilter)->goInMask);

      if (fail_options) {
        cerr << ((Filter *)hFilter)->PortName(nFromPort)
             << " WARNING: Requested by filter " << ((Filter *)hFilter)->FilterName()
             << " option(s) 0x" << hex << fail_options << dec
             << " not accepted" << endl;
      }
      break;
    }
    case HUB_MSG_TYPE_RBR_STATUS:
      if (((Filter *)hFilter)->goInMask & GO_RBR_STATUS) {
        State *pState = ((Filter *)hFilter)->GetState(nToPort);

        if (!pState)
          return FALSE;

        if (pState->rbr != pOutMsg->u.val) {
          pState->rbr = pOutMsg->u.val;

          if (pState->isConnected)
            InsertRBR(*(Filter *)hFilter, pState->rbr, &pOutMsg);
        }
      }
      break;
    case HUB_MSG_TYPE_RLC_STATUS:
      _ASSERTE((pOutMsg->u.val & ~(VAL2LC_BYTESIZE(-1)|VAL2LC_PARITY(-1)|VAL2LC_STOPBITS(-1))) == 0);

      if (((Filter *)hFilter)->goInMask & GO_RLC_STATUS) {
        State *pState = ((Filter *)hFilter)->GetState(nToPort);

        if (!pState)
          return FALSE;

        if (pState->rlc != pOutMsg->u.val) {
          pState->rlc = pOutMsg->u.val;

          if (pState->isConnected)
            InsertRLC(*(Filter *)hFilter, pState->rlc, &pOutMsg);
        }
      }
      break;
    case HUB_MSG_TYPE_BREAK_STATUS:
      if (((Filter *)hFilter)->goInMask & GO_BREAK_STATUS) {
        State *pState = ((Filter *)hFilter)->GetState(nToPort);

        if (!pState)
          return FALSE;

        if (!pState->isConnected)
          break;

        if (pOutMsg->u.val) {
          if ((((Filter *)hFilter)->goInMask & GO_V2O_LINE_STATUS(LINE_STATUS_BI)) == 0)
            InsertStatus(*(Filter *)hFilter, SERIAL_LSRMST_LSR_NODATA, LINE_STATUS_BI, &pOutMsg);
        } else {
          InsertStatus(*(Filter *)hFilter, SERIAL_LSRMST_LSR_NODATA, 0, &pOutMsg);
        }
      }
      break;
    case HUB_MSG_TYPE_LINE_STATUS: {
      BYTE lsr;

      lsr = (BYTE)pOutMsg->u.val & (BYTE)MASK2VAL(pOutMsg->u.val) & GO_O2V_LINE_STATUS(((Filter *)hFilter)->goInMask);

      if (lsr) {
        State *pState = ((Filter *)hFilter)->GetState(nToPort);

        if (!pState)
          return FALSE;

        if (pState->isConnected)
          InsertStatus(*(Filter *)hFilter, SERIAL_LSRMST_LSR_NODATA, lsr, &pOutMsg);
      }

      break;
    }
    case HUB_MSG_TYPE_MODEM_STATUS: {
      BYTE mstInMask;

      mstInMask = (BYTE)MASK2VAL(pOutMsg->u.val) & GO_O2V_MODEM_STATUS(((Filter *)hFilter)->goInMask);

      if (mstInMask) {
        State *pState = ((Filter *)hFilter)->GetState(nToPort);

        if (!pState)
          return FALSE;

        BYTE mstInVal;

        mstInVal = (((BYTE)pOutMsg->u.val & mstInMask) | (pState->mstInVal & ~mstInMask));
        mstInMask = pState->mstInVal ^ mstInVal;

        if (!mstInMask)
          break;

        if (pState->isConnected)
          InsertStatus(*(Filter *)hFilter, SERIAL_LSRMST_MST, mstInVal, &pOutMsg);

        pState->mstInVal = mstInVal;
      }
      break;
    }
    case HUB_MSG_TYPE_LINE_DATA: {
      // escape escape characters

      _ASSERTE(pOutMsg->u.buf.pBuf != NULL || pOutMsg->u.buf.size == 0);

      DWORD len = pOutMsg->u.buf.size;

      if (len == 0)
        return TRUE;

      basic_string<BYTE> line_data;
      const BYTE *pBuf = pOutMsg->u.buf.pBuf;
      BYTE escapeChar = ((Filter *)hFilter)->escapeChar;

      for (; len ; len--) {
        BYTE ch = *pBuf++;

        line_data.append(&ch, 1);

        if (ch == escapeChar) {
          BYTE escape = SERIAL_LSRMST_ESCAPE;
          line_data.append(&escape, 1);
        }
      }

      pOutMsg = pMsgReplaceBuf(pOutMsg,
                               HUB_MSG_TYPE_LINE_DATA,
                               line_data.data(),
                               (DWORD)line_data.size());

      break;
    }
  }

  return pOutMsg != NULL;
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
  if (!ROUTINE_IS_VALID(pHubRoutines, pMsgInsertBuf) ||
      !ROUTINE_IS_VALID(pHubRoutines, pMsgReplaceBuf) ||
      !ROUTINE_IS_VALID(pHubRoutines, pPortName) ||
      !ROUTINE_IS_VALID(pHubRoutines, pFilterName))
  {
    return NULL;
  }

  pMsgInsertBuf = pHubRoutines->pMsgInsertBuf;
  pMsgReplaceBuf = pHubRoutines->pMsgReplaceBuf;
  pPortName = pHubRoutines->pPortName;
  pFilterName = pHubRoutines->pFilterName;

  return plugins;
}
///////////////////////////////////////////////////////////////
