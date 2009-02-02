/*
 * $Id$
 *
 * Copyright (c) 2008-2009 Vyacheslav Frolov
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
 * Revision 1.9  2009/02/02 15:21:42  vfrolov
 * Optimized filter's API
 *
 * Revision 1.8  2008/12/22 09:40:45  vfrolov
 * Optimized message switching
 *
 * Revision 1.7  2008/11/25 16:40:40  vfrolov
 * Added assert for port handle
 *
 * Revision 1.6  2008/11/24 12:37:00  vfrolov
 * Changed plugin API
 *
 * Revision 1.5  2008/11/13 07:46:58  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.4  2008/10/22 08:27:26  vfrolov
 * Added ability to set bytesize, parity and stopbits separately
 *
 * Revision 1.3  2008/10/16 09:24:23  vfrolov
 * Changed return type of ROUTINE_MSG_REPLACE_*() to BOOL
 *
 * Revision 1.2  2008/10/16 07:46:04  vfrolov
 * Redesigned escinsert filter to accept SET instead STATUS messages
 *
 * Revision 1.1  2008/09/30 08:32:38  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "../plugins_api.h"
#include "../cncext.h"
///////////////////////////////////////////////////////////////
namespace FilterEscInsert {
///////////////////////////////////////////////////////////////
static ROUTINE_MSG_INSERT_BUF *pMsgInsertBuf;
static ROUTINE_MSG_REPLACE_BUF *pMsgReplaceBuf;
static ROUTINE_MSG_REPLACE_NONE *pMsgReplaceNone;
///////////////////////////////////////////////////////////////
#ifndef _DEBUG
  #define DEBUG_PARAM(par)
#else   /* _DEBUG */
  #define DEBUG_PARAM(par) par
#endif  /* _DEBUG */
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
class Filter;

class State {
  public:
    State()
    : isConnected(FALSE),
      soMask(0),
      pinState(0),
      br(0),
      lc(0)
    {}

    BOOL isConnected;
    DWORD soMask;
    WORD pinState;
    DWORD br;
    DWORD lc;
};
///////////////////////////////////////////////////////////////
class Filter : public Valid {
  public:
    Filter(int argc, const char *const argv[]);

    DWORD soMask;
    BYTE escapeChar;
};

Filter::Filter(int argc, const char *const argv[])
  : soMask(SO_V2O_PIN_STATE(SPS_V2P_MST(-1)|PIN_STATE_BREAK) |
           SO_V2O_LINE_STATUS(-1) |
           SO_SET_BR|SO_SET_LC),
    escapeChar(0xFF)
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
  << "      --create-filter=pinmap:--cts=cts --dsr=dsr --ring=ring --dcd=dcd --break=break" << endl
  << "      --create-filter=lsrmap" << endl
  << "      --create-filter=linectl" << endl
  << "      --add-filters=1:escinsert,pinmap,lsrmap,linectl" << endl
  << "      --use-driver=tcp" << endl
  << "      222.22.22.22:2222" << endl
  << "      _END_" << endl
  << "    - transfer data, signals, baudrate and line control info from serial port" << endl
  << "      CNCB0 to TCP port 222.22.22.22:2222 via escaped datastream." << endl
  ;
}
///////////////////////////////////////////////////////////////
static HFILTER CALLBACK Create(
    HMASTERFILTER DEBUG_PARAM(hMasterFilter),
    HCONFIG /*hConfig*/,
    int argc,
    const char *const argv[])
{
  _ASSERTE(hMasterFilter != NULL);

  Filter *pFilter = new Filter(argc, argv);

  if (!pFilter) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  if (!pFilter->IsValid()) {
    delete pFilter;
    return NULL;
  }

  return (HFILTER)pFilter;
}
///////////////////////////////////////////////////////////////
static void CALLBACK Delete(
    HFILTER hFilter)
{
  _ASSERTE(hFilter != NULL);

  delete (Filter *)hFilter;
}
///////////////////////////////////////////////////////////////
static HFILTERINSTANCE CALLBACK CreateInstance(
    HMASTERFILTERINSTANCE DEBUG_PARAM(hMasterFilterInstance))
{
  _ASSERTE(hMasterFilterInstance != NULL);

  return (HFILTERINSTANCE)new State();
}
///////////////////////////////////////////////////////////////
static void CALLBACK DeleteInstance(
    HFILTERINSTANCE hFilterInstance)
{
  _ASSERTE(hFilterInstance != NULL);

  delete (State *)hFilterInstance;
}
///////////////////////////////////////////////////////////////
static void InsertStatus(
    BYTE escapeChar,
    BYTE code,
    BYTE val,
    HUB_MSG **ppMsg)
{
  BYTE insert[3];

  insert[0] = escapeChar;
  insert[1] = code;
  insert[2] = val;

  *ppMsg = pMsgInsertBuf(*ppMsg,
                         HUB_MSG_TYPE_LINE_DATA,
                         insert,
                         sizeof(insert));
}
///////////////////////////////////////////////////////////////
static void InsertRBR(
    BYTE escapeChar,
    ULONG rbr,
    HUB_MSG **ppMsg)
{
  BYTE insert[2 + sizeof(ULONG)];

  insert[0] = escapeChar;
  insert[1] = C0CE_INSERT_RBR;
  *(ULONG *)&insert[2] = rbr;

  *ppMsg = pMsgInsertBuf(*ppMsg,
                         HUB_MSG_TYPE_LINE_DATA,
                         insert,
                         sizeof(insert));
}
///////////////////////////////////////////////////////////////
static void InsertRLC(
    BYTE escapeChar,
    DWORD rlc,
    HUB_MSG **ppMsg)
{
  BYTE insert[2 + 3];

  insert[0] = escapeChar;
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
    HFILTERINSTANCE hFilterInstance,
    HUB_MSG *pInMsg,
    HUB_MSG **ppEchoMsg)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(hFilterInstance != NULL);
  _ASSERTE(pInMsg != NULL);
  _ASSERTE(ppEchoMsg != NULL);
  _ASSERTE(*ppEchoMsg == NULL);

  switch (HUB_MSG_T2N(pInMsg->type)) {
    case HUB_MSG_T2N(HUB_MSG_TYPE_CONNECT): {
      if (pInMsg->u.val) {
        _ASSERTE(((State *)hFilterInstance)->isConnected == FALSE);
        ((State *)hFilterInstance)->isConnected = TRUE;
      } else {
        _ASSERTE(((State *)hFilterInstance)->isConnected == TRUE);
        ((State *)hFilterInstance)->isConnected = FALSE;
        break;
      }

      // Insert current state

      if ((((State *)hFilterInstance)->soMask & SO_SET_BR) && ((State *)hFilterInstance)->br != 0)
        InsertRBR(((Filter *)hFilter)->escapeChar, ((State *)hFilterInstance)->br, ppEchoMsg);

      if ((((State *)hFilterInstance)->soMask & SO_SET_LC) &&
          (((State *)hFilterInstance)->lc & (LC_MASK_BYTESIZE|LC_MASK_PARITY|LC_MASK_STOPBITS)) == (LC_MASK_BYTESIZE|LC_MASK_PARITY|LC_MASK_STOPBITS))
      {
        InsertRLC(((Filter *)hFilter)->escapeChar, ((State *)hFilterInstance)->lc, ppEchoMsg);
      }

      if (((State *)hFilterInstance)->soMask & SO_V2O_PIN_STATE(SPS_V2P_MST(-1)))
        InsertStatus(((Filter *)hFilter)->escapeChar, SERIAL_LSRMST_MST, SPS_P2V_MST(((State *)hFilterInstance)->pinState), ppEchoMsg);

      if (((State *)hFilterInstance)->soMask & (SO_V2O_LINE_STATUS(-1) | SO_V2O_PIN_STATE(PIN_STATE_BREAK))) {
        BYTE brk = (((State *)hFilterInstance)->pinState & PIN_STATE_BREAK) ? LINE_STATUS_BI : 0;

        InsertStatus(((Filter *)hFilter)->escapeChar, SERIAL_LSRMST_LSR_NODATA, brk, ppEchoMsg);
      }

      break;
    }
  }

  return pInMsg != NULL;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK OutMethod(
    HFILTER hFilter,
    HFILTERINSTANCE hFilterInstance,
    HMASTERPORT DEBUG_PARAM(hFromPort),
    HUB_MSG *pOutMsg)
{
  _ASSERTE(hFilterInstance != NULL);
  _ASSERTE(hFilterInstance != NULL);
  _ASSERTE(hFromPort != NULL);
  _ASSERTE(pOutMsg != NULL);

  switch (HUB_MSG_T2N(pOutMsg->type)) {
    case HUB_MSG_T2N(HUB_MSG_TYPE_SET_OUT_OPTS): {
      DWORD soMask = (pOutMsg->u.val & ((Filter *)hFilter)->soMask);

      // discard supported options
      pOutMsg->u.val &= ~((Filter *)hFilter)->soMask;

      ((State *)hFilterInstance)->soMask |= soMask;

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_SET_BR): {
      _ASSERTE(((((State *)hFilterInstance)->soMask | ~((Filter *)hFilter)->soMask) & SO_SET_BR) != 0);

      if (((State *)hFilterInstance)->soMask & SO_SET_BR) {
        DWORD val = pOutMsg->u.val;

        // discard messages for supported options
        if (!pMsgReplaceNone(pOutMsg, HUB_MSG_TYPE_EMPTY))
          return FALSE;

        if (((State *)hFilterInstance)->br != val) {
          ((State *)hFilterInstance)->br = val;

          if (((State *)hFilterInstance)->isConnected)
            InsertRBR(((Filter *)hFilter)->escapeChar, ((State *)hFilterInstance)->br, &pOutMsg);
        }
      }

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_SET_LC): {
      _ASSERTE((pOutMsg->u.val & ~(VAL2LC_BYTESIZE(-1)|LC_MASK_BYTESIZE
                                  |VAL2LC_PARITY(-1)|LC_MASK_PARITY
                                  |VAL2LC_STOPBITS(-1)|LC_MASK_STOPBITS)) == 0);

      _ASSERTE(((((State *)hFilterInstance)->soMask | ~((Filter *)hFilter)->soMask) & SO_SET_LC) != 0);

      if (((State *)hFilterInstance)->soMask & SO_SET_LC) {
        DWORD val = pOutMsg->u.val;

        // discard messages for supported options
        if (!pMsgReplaceNone(pOutMsg, HUB_MSG_TYPE_EMPTY))
          return FALSE;

        if ((val & LC_MASK_BYTESIZE) == 0) {
          _ASSERTE((val & VAL2LC_BYTESIZE(-1)) == 0);
          val |= (((State *)hFilterInstance)->lc & (VAL2LC_BYTESIZE(-1)|LC_MASK_BYTESIZE));
        }

        if ((val & LC_MASK_PARITY) == 0) {
          _ASSERTE((val & VAL2LC_PARITY(-1)) == 0);
          val |= (((State *)hFilterInstance)->lc & (VAL2LC_PARITY(-1)|LC_MASK_PARITY));
        }

        if ((val & LC_MASK_STOPBITS) == 0) {
          _ASSERTE((val & VAL2LC_STOPBITS(-1)) == 0);
          val |= (((State *)hFilterInstance)->lc & (VAL2LC_STOPBITS(-1)|LC_MASK_STOPBITS));
        }

        if (((State *)hFilterInstance)->lc != val) {
          ((State *)hFilterInstance)->lc = val;

          if (((State *)hFilterInstance)->isConnected)
            InsertRLC(((Filter *)hFilter)->escapeChar, ((State *)hFilterInstance)->lc, &pOutMsg);
        }
      }

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_SET_PIN_STATE): {
      _ASSERTE(((((State *)hFilterInstance)->soMask | ~((Filter *)hFilter)->soMask) & SO_V2O_PIN_STATE(-1)) != 0);

      WORD mask = MASK2VAL(pOutMsg->u.val) & SO_O2V_PIN_STATE(((State *)hFilterInstance)->soMask);

      if (!mask)
        break;

      WORD pinState = (WORD)pOutMsg->u.val;

      // discard settings for supported options
      pOutMsg->u.val &= ~VAL2MASK(mask);

      pinState = ((pinState & mask) | (((State *)hFilterInstance)->pinState & ~mask));
      mask = ((State *)hFilterInstance)->pinState ^ pinState;

      if (mask) {
        if (((State *)hFilterInstance)->isConnected) {
          if (SPS_P2V_MST(mask))
            InsertStatus(((Filter *)hFilter)->escapeChar, SERIAL_LSRMST_MST, SPS_P2V_MST(pinState), &pOutMsg);

          if (mask & PIN_STATE_BREAK) {
            BYTE brk = (pinState & PIN_STATE_BREAK) ? LINE_STATUS_BI : 0;

            if (!brk || (((State *)hFilterInstance)->soMask & SO_V2O_LINE_STATUS(LINE_STATUS_BI)) == 0)
              InsertStatus(((Filter *)hFilter)->escapeChar, SERIAL_LSRMST_LSR_NODATA, brk, &pOutMsg);
          }
        }

        ((State *)hFilterInstance)->pinState = pinState;
      }

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_SET_LSR): {
      _ASSERTE(((((State *)hFilterInstance)->soMask | ~((Filter *)hFilter)->soMask) & SO_V2O_LINE_STATUS(-1)) != 0);

      BYTE lsr;

      lsr = (BYTE)pOutMsg->u.val & (BYTE)MASK2VAL(pOutMsg->u.val) & SO_O2V_LINE_STATUS(((State *)hFilterInstance)->soMask);

      // discard settings for supported options
      pOutMsg->u.val &= ~VAL2MASK(SO_O2V_LINE_STATUS(((State *)hFilterInstance)->soMask));

      if (!lsr)
        break;

      if (((State *)hFilterInstance)->isConnected)
        InsertStatus(((Filter *)hFilter)->escapeChar, SERIAL_LSRMST_LSR_NODATA, lsr, &pOutMsg);

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_LINE_DATA): {
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

      if (!pMsgReplaceBuf(pOutMsg, HUB_MSG_TYPE_LINE_DATA, line_data.data(), (DWORD)line_data.size()))
        return FALSE;

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
  Delete,
  CreateInstance,
  DeleteInstance,
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
      !ROUTINE_IS_VALID(pHubRoutines, pMsgReplaceNone))
  {
    return NULL;
  }

  pMsgInsertBuf = pHubRoutines->pMsgInsertBuf;
  pMsgReplaceBuf = pHubRoutines->pMsgReplaceBuf;
  pMsgReplaceNone = pHubRoutines->pMsgReplaceNone;

  return plugins;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
