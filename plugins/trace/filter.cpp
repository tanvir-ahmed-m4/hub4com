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
 * Revision 1.9  2008/11/21 08:16:56  vfrolov
 * Added HUB_MSG_TYPE_LOOP_TEST
 *
 * Revision 1.8  2008/11/13 07:52:20  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.7  2008/11/12 08:46:39  vfrolov
 * Fixed TYPE_LC and SET_LSR tracing
 *
 * Revision 1.6  2008/10/16 16:02:34  vfrolov
 * Added LBR_STATUS and LLC_STATUS
 *
 * Revision 1.5  2008/10/16 06:46:13  vfrolov
 * Added PIN_STATE_* for DCE
 * Added HUB_MSG_TYPE_SET_LSR
 *
 * Revision 1.4  2008/10/02 14:01:43  vfrolov
 * Added help
 *
 * Revision 1.3  2008/09/30 08:28:32  vfrolov
 * Added ability to control OUT1 and OUT2 pins
 * Added ability to get remote baud rate and line control settings
 * Added ability to set baud rate and line control
 * Added fallback to non escape mode
 *
 * Revision 1.2  2008/08/29 15:17:07  vfrolov
 * Added printing command line and config
 *
 * Revision 1.1  2008/08/29 13:13:04  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "../plugins_api.h"
///////////////////////////////////////////////////////////////
namespace FilterTrace {
///////////////////////////////////////////////////////////////
#ifndef _DEBUG
  #define DEBUG_PARAM(par)
#else   /* _DEBUG */
  #define DEBUG_PARAM(par) par
#endif  /* _DEBUG */
///////////////////////////////////////////////////////////////
static ROUTINE_PORT_NAME_A *pPortName = NULL;
static ROUTINE_FILTER_NAME_A *pFilterName = NULL;
///////////////////////////////////////////////////////////////
static void PrintTime(ostream &tout);
///////////////////////////////////////////////////////////////
const char *GetParam(const char *pArg, const char *pPattern)
{
  size_t lenPattern = strlen(pPattern);

  if (_strnicmp(pArg, pPattern, lenPattern) != 0)
    return NULL;

  return pArg + lenPattern;
}
///////////////////////////////////////////////////////////////
class TraceConfig {
  public:
    TraceConfig() : pTraceStream(NULL) {}

    void SetTracePath(const char *pPath);
    ostream *GetTraceStream();
    void PrintToAllTraceStreams(const char *pStr);
    stringstream buf;

  private:
    string path;
    ostream *pTraceStream;

    typedef set<ostream*> Streams;

    Streams traceStreams;
};

void TraceConfig::SetTracePath(const char *pPath) {
  path = pPath;
  pTraceStream = NULL;
}

ostream *TraceConfig::GetTraceStream() {
  if (pTraceStream)
    return pTraceStream;

  if (path.empty()) {
    pTraceStream = &cout;
  } else {
    ofstream *pStream = new ofstream(path.c_str());

    if (!pStream) {
      cerr << "No enough memory." << endl;
      exit(2);
    }

    if (!pStream->is_open()) {
      cerr << "Can't open " << path << endl;
      exit(2);
    }

    pTraceStream = pStream;
  }

  traceStreams.insert(pTraceStream);

  return pTraceStream;
}

void TraceConfig::PrintToAllTraceStreams(const char *pStr)
{
  for (Streams::const_iterator iS = traceStreams.begin() ; iS != traceStreams.end() ; iS++)
    (**iS) << pStr;
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
class Filter : public Valid {
  public:
    Filter(TraceConfig &config, int argc, const char *const argv[]);
    void SetHub(HHUB _hHub) { hHub = _hHub; pName = pFilterName(hHub, (HFILTER)this); }
    const char *PortName(int nPort) const { return pPortName(hHub, nPort); }
    const char *FilterName() const { return pName; }

    ostream *pTraceStream;
  private:
    HHUB hHub;
    const char *pName;
};

Filter::Filter(TraceConfig &config, int argc, const char *const argv[])
  : pTraceStream(NULL),
    hHub(NULL),
    pName(NULL)
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

  if (!pTraceStream)
    pTraceStream = config.GetTraceStream();
}
///////////////////////////////////////////////////////////////
static PLUGIN_TYPE CALLBACK GetPluginType()
{
  return PLUGIN_TYPE_FILTER;
}
///////////////////////////////////////////////////////////////
static const PLUGIN_ABOUT_A about = {
  sizeof(PLUGIN_ABOUT_A),
  "trace",
  "Copyright (c) 2008 Vyacheslav Frolov",
  "GNU General Public License",
  "Trace filter",
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
  << "  " << pProgPath << " ... [<global options>] --create-filter=" << GetPluginAbout()->pName << "[,<FID>][:<options>] ... --add-filters=<ports>:[...,]<FID>[,...] ..." << endl
  << endl
  << "Global options:" << endl
  << "  --trace-file=<path>   - redirect trace to <path>. Cancel redirection if" << endl
  << "                          <path> is empty." << endl
  << endl
  << "Options:" << endl
  << endl
  << "Examples:" << endl
  << "  " << pProgPath << " --load=,,_END_" << endl
  << "      COM4" << endl
  << endl
  << "      --trace-file=com.log" << endl
  << "      --create-filter=trace,com,com" << endl
  << "      --create-filter=pin2con,com,pin2con" << endl
  << "      --trace-file=p2c.log" << endl
  << "      --create-filter=trace,com,p2c" << endl
  << "      --create-filter=awakseq,com,awakseq:--awak-seq=aaa" << endl
  << "      --trace-file=awk.log" << endl
  << "      --create-filter=trace,com,awk" << endl
  << "      --add-filters=0:com" << endl
  << endl
  << "      --use-driver=tcp" << endl
  << "      1.1.1.1:23" << endl
  << endl
  << "      --trace-file=tcp.log" << endl
  << "      --create-filter=trace,tcp,tcp" << endl
  << "      --create-filter=telnet,tcp,telnet" << endl
  << "      --trace-file=tel.log" << endl
  << "      --create-filter=trace,tcp,tel" << endl
  << "      --add-filters=1:tcp" << endl
  << endl
  << "      _END_" << endl
  << "    - Trace data to different files by this way:" << endl
  << "      COM4 <---> pin2con <---> awakseq <--->   <---> telnet <---> 1.1.1.1:23" << endl
  << "             |             |             |       |            |" << endl
  << "          com.log       p2c.log       awk.log tel.log      tcp.log" << endl
  ;
}
///////////////////////////////////////////////////////////////
static HCONFIG CALLBACK ConfigStart()
{
  TraceConfig *pConfig = new TraceConfig;

  if (!pConfig) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  PrintTime(pConfig->buf);
  pConfig->buf << "Command Line:" << endl
               << "  {" << GetCommandLine() << "}" << endl;

  PrintTime(pConfig->buf);
  pConfig->buf << "Config: {";

  return (HCONFIG)pConfig;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK Config(
    HCONFIG hConfig,
    const char *pArg)
{
  _ASSERTE(hConfig != NULL);

  ((TraceConfig *)hConfig)->buf << endl << "  {" << pArg << "}";

  const char *pParam;

  if ((pParam = GetParam(pArg, "--trace-file=")) != NULL) {
    ((TraceConfig *)hConfig)->SetTracePath(pParam);
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

  ((TraceConfig *)hConfig)->buf << endl << "}" << endl;
  ((TraceConfig *)hConfig)->PrintToAllTraceStreams(((TraceConfig *)hConfig)->buf.str().c_str());

  delete (TraceConfig *)hConfig;
}
///////////////////////////////////////////////////////////////
static HFILTER CALLBACK Create(
    HCONFIG hConfig,
    int argc,
    const char *const argv[])
{
  _ASSERTE(hConfig != NULL);

  Filter *pFilter = new Filter(*(TraceConfig *)hConfig, argc, argv);

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
static void PrintTime(ostream &tout)
{
  char f = tout.fill('0');

  SYSTEMTIME time;

  ::GetLocalTime(&time);

  tout << setw(4) << time.wYear << "/"
       << setw(2) << time.wMonth << "/"
       << setw(2) << time.wDay << " "
       << setw(2) << time.wHour << ":"
       << setw(2) << time.wMinute << ":"
       << setw(2) << time.wSecond << "."
       << setw(3) << time.wMilliseconds << " ";

  tout.fill(f);
}
///////////////////////////////////////////////////////////////
struct CODE2NAME {
  DWORD code;
  const char *name;
};
#define TOCODE2NAME(p, s) { (ULONG)p##s, #s }

static void PrintCode(ostream &tout, const CODE2NAME *pTable, DWORD code)
{
  if (pTable) {
    while (pTable->name) {
      if (pTable->code == code) {
        tout << pTable->name;
        return;
      }
      pTable++;
    }
  }

  tout << "0x" << hex << code << dec;
}
///////////////////////////////////////////////////////////////
struct FIELD2NAME {
  DWORD field;
  DWORD mask;
  const char *name;
};
#define TOFIELD2NAME2(p, s) { (ULONG)p##s, (ULONG)p##s, #s }

static BOOL PrintFields(
    ostream &tout,
    const FIELD2NAME *pTable,
    DWORD fields,
    BOOL delimitNext = FALSE,
    const char *pUnknownPrefix = "",
    const char *pDelimiter = "|")
{
  if (pTable) {
    while (pTable->name) {
      DWORD field = (fields & pTable->mask);

      if (field == pTable->field) {
        fields &= ~pTable->mask;
        if (delimitNext)
          tout << pDelimiter;
        else
          delimitNext = TRUE;

        tout << pTable->name;
      }
      pTable++;
    }
  }

  if (fields) {
    if (delimitNext)
      tout << pDelimiter;
    else
      delimitNext = TRUE;

    tout << pUnknownPrefix << "0x" << hex << fields << dec;
  }

  return delimitNext;
}
///////////////////////////////////////////////////////////////
static const CODE2NAME codeNameTableHubMsg[] = {
  TOCODE2NAME(HUB_MSG_TYPE_, EMPTY),
  TOCODE2NAME(HUB_MSG_TYPE_, LINE_DATA),
  TOCODE2NAME(HUB_MSG_TYPE_, CONNECT),
  TOCODE2NAME(HUB_MSG_TYPE_, MODEM_STATUS),
  TOCODE2NAME(HUB_MSG_TYPE_, LINE_STATUS),
  TOCODE2NAME(HUB_MSG_TYPE_, SET_PIN_STATE),
  TOCODE2NAME(HUB_MSG_TYPE_, GET_IN_OPTS),
  TOCODE2NAME(HUB_MSG_TYPE_, SET_OUT_OPTS),
  TOCODE2NAME(HUB_MSG_TYPE_, FAIL_IN_OPTS),
  TOCODE2NAME(HUB_MSG_TYPE_, RBR_STATUS),
  TOCODE2NAME(HUB_MSG_TYPE_, RLC_STATUS),
  TOCODE2NAME(HUB_MSG_TYPE_, COUNT_REPEATS),
  TOCODE2NAME(HUB_MSG_TYPE_, GET_ESC_OPTS),
  TOCODE2NAME(HUB_MSG_TYPE_, FAIL_ESC_OPTS),
  TOCODE2NAME(HUB_MSG_TYPE_, BREAK_STATUS),
  TOCODE2NAME(HUB_MSG_TYPE_, SET_BR),
  TOCODE2NAME(HUB_MSG_TYPE_, SET_LC),
  TOCODE2NAME(HUB_MSG_TYPE_, SET_LSR),
  TOCODE2NAME(HUB_MSG_TYPE_, LBR_STATUS),
  TOCODE2NAME(HUB_MSG_TYPE_, LLC_STATUS),
  TOCODE2NAME(HUB_MSG_TYPE_, LOOP_TEST),
  {0, NULL}
};
///////////////////////////////////////////////////////////////
static const CODE2NAME codeNameTableParity[] = {
  NOPARITY,    "N",
  ODDPARITY,   "O",
  EVENPARITY,  "E",
  MARKPARITY,  "M",
  SPACEPARITY, "S",
  {0, NULL}
};
///////////////////////////////////////////////////////////////
static const CODE2NAME codeNameTableStopBits
[] = {
  ONESTOPBIT,   "1",
  ONE5STOPBITS, "1.5",
  TWOSTOPBITS,  "2",
  {0, NULL}
};
///////////////////////////////////////////////////////////////
static const FIELD2NAME fieldNameTableModemStatus[] = {
  TOFIELD2NAME2(MODEM_STATUS_, DCTS),
  TOFIELD2NAME2(MODEM_STATUS_, DDSR),
  TOFIELD2NAME2(MODEM_STATUS_, TERI),
  TOFIELD2NAME2(MODEM_STATUS_, DDCD),
  TOFIELD2NAME2(MODEM_STATUS_, CTS),
  TOFIELD2NAME2(MODEM_STATUS_, DSR),
  TOFIELD2NAME2(MODEM_STATUS_, RI),
  TOFIELD2NAME2(MODEM_STATUS_, DCD),
  {0, 0, NULL}
};
///////////////////////////////////////////////////////////////
static const FIELD2NAME fieldNameTableLineStatus[] = {
  TOFIELD2NAME2(LINE_STATUS_, DR),
  TOFIELD2NAME2(LINE_STATUS_, OE),
  TOFIELD2NAME2(LINE_STATUS_, PE),
  TOFIELD2NAME2(LINE_STATUS_, FE),
  TOFIELD2NAME2(LINE_STATUS_, BI),
  TOFIELD2NAME2(LINE_STATUS_, THRE),
  TOFIELD2NAME2(LINE_STATUS_, TEMT),
  TOFIELD2NAME2(LINE_STATUS_, FIFOERR),
  {0, 0, NULL}
};
///////////////////////////////////////////////////////////////
static const FIELD2NAME fieldNameTableGoOptions[] = {
  TOFIELD2NAME2(GO_, RBR_STATUS),
  TOFIELD2NAME2(GO_, RLC_STATUS),
  TOFIELD2NAME2(GO_, LBR_STATUS),
  TOFIELD2NAME2(GO_, LLC_STATUS),
  TOFIELD2NAME2(GO_, BREAK_STATUS),
  TOFIELD2NAME2(GO_, ESCAPE_MODE),
  {0, 0, NULL}
};
///////////////////////////////////////////////////////////////
static const FIELD2NAME codeNameTableSetPinState[] = {
  TOFIELD2NAME2(PIN_STATE_, RTS),
  TOFIELD2NAME2(PIN_STATE_, DTR),
  TOFIELD2NAME2(PIN_STATE_, OUT1),
  TOFIELD2NAME2(PIN_STATE_, OUT2),
  TOFIELD2NAME2(PIN_STATE_, CTS),
  TOFIELD2NAME2(PIN_STATE_, DSR),
  TOFIELD2NAME2(PIN_STATE_, RI),
  TOFIELD2NAME2(PIN_STATE_, DCD),
  TOFIELD2NAME2(PIN_STATE_, BREAK),
  {0, 0, NULL}
};
///////////////////////////////////////////////////////////////
static const FIELD2NAME fieldNameTableSoOptions[] = {
  TOFIELD2NAME2(SO_, SET_BR),
  TOFIELD2NAME2(SO_, SET_LC),
  {0, 0, NULL}
};
///////////////////////////////////////////////////////////////
static BOOL PrintGoOptions(
    ostream &tout,
    DWORD fields,
    BOOL delimitNext = FALSE,
    const char *pUnknownPrefix = "")
{
  delimitNext = PrintFields(tout, fieldNameTableModemStatus, GO_O2V_MODEM_STATUS(fields), delimitNext, "MST_");
  delimitNext = PrintFields(tout, fieldNameTableLineStatus, GO_O2V_LINE_STATUS(fields), delimitNext, "LSR_");
  delimitNext = PrintFields(tout, fieldNameTableGoOptions, fields & ~(GO_V2O_MODEM_STATUS(-1) | GO_V2O_LINE_STATUS(-1)), delimitNext, pUnknownPrefix);

  return delimitNext;
}
///////////////////////////////////////////////////////////////
static BOOL PrintEscOptions(
    ostream &tout,
    DWORD fields,
    BOOL delimitNext = FALSE)
{
  PrintGoOptions(tout, ESC_OPTS_MAP_EO2GO(fields), delimitNext, "GO_");
  PrintFields(tout, NULL, fields & ~ESC_OPTS_MAP_GO2EO(-1), delimitNext);

  return delimitNext;
}
///////////////////////////////////////////////////////////////
static void PrintMaskedFields(ostream &tout, const FIELD2NAME *pTable, DWORD maskedFields)
{
    WORD mask = MASK2VAL(maskedFields);

    tout << "SET[";
    PrintFields(tout, pTable, maskedFields & mask);
    tout << "] CLR[";
    PrintFields(tout, pTable, ~maskedFields & mask);
    tout << "]";
}
///////////////////////////////////////////////////////////////
static void PrintBuf(ostream &tout, const BYTE *pData, DWORD size)
{
  tout << "[" << size << "]:";

  ios_base::fmtflags b = tout.setf(ios_base::hex, ios_base::basefield);
  char f = tout.fill('0');

  while (size) {
    tout << endl << "  ";

    stringstream buf;

    int i = 0;

    for ( ; i < 16 && size ; i++, size--) {
      BYTE ch = *pData++;

      tout << setw(2) << (unsigned)ch << " ";
      buf << (char)((ch >= 0x20 && ch < 0x7F) ? ch : '.');
    }

    for ( ; i < 16 ; i++) {
      tout << "   ";
      buf << " ";
    }

    tout << " * " << buf.str() << " *";
  }

  tout.fill(f);
  tout.setf(b, ios_base::basefield);
}
///////////////////////////////////////////////////////////////
static void PrintMsgType(ostream &tout, DWORD msgType)
{
  tout << "MSG_";
  PrintCode(tout, codeNameTableHubMsg, msgType);
}
///////////////////////////////////////////////////////////////
static void PrintVal(ostream &tout, DWORD msgType, DWORD val)
{
  switch (msgType & HUB_MSG_VAL_TYPES_MASK) {
    case HUB_MSG_VAL_TYPE_BOOL:
      tout << (val ? "true" : "false");
      break;
    case HUB_MSG_VAL_TYPE_MSG_TYPE:
      PrintMsgType(tout, val);
      break;
    case HUB_MSG_VAL_TYPE_UINT:
      tout << val;
      break;
    case HUB_MSG_VAL_TYPE_LC:
      if (val & LC_MASK_BYTESIZE)
        tout << (unsigned)LC2VAL_BYTESIZE(val);
      else
        tout << 'x';

      tout << '-';

      if (val & LC_MASK_PARITY)
        PrintCode(tout, codeNameTableParity, LC2VAL_PARITY(val));
      else
        tout << 'x';

      tout << '-';

      if (val & LC_MASK_STOPBITS)
        PrintCode(tout, codeNameTableStopBits, LC2VAL_STOPBITS(val));
      else
        tout << 'x';
      break;
    default:
      tout << "0x" << hex << val << dec;
  }
}
///////////////////////////////////////////////////////////////
static void PrintMsgBody(ostream &tout, HUB_MSG *pMsg)
{
  switch (pMsg->type & HUB_MSG_UNION_TYPE_MASK) {
    case HUB_MSG_UNION_TYPE_NONE:
      break;
    case HUB_MSG_UNION_TYPE_BUF:
      PrintBuf(tout, pMsg->u.buf.pBuf, pMsg->u.buf.size);
      break;
    case HUB_MSG_UNION_TYPE_VAL:
      PrintVal(tout, pMsg->type, pMsg->u.val);
      break;
    case HUB_MSG_UNION_TYPE_PVAL:
      tout << hex << "&" << pMsg->u.pv.pVal << "[0x" << *pMsg->u.pv.pVal << dec << "] ";
      PrintVal(tout, pMsg->type, pMsg->u.pv.val);
      break;
    case HUB_MSG_UNION_TYPE_HVAL:
      tout << pMsg->u.hVal;
      break;
    default:
      tout  << "???";
  }
}
///////////////////////////////////////////////////////////////
static void PrintMsg(ostream &tout, HUB_MSG *pMsg)
{
  PrintMsgType(tout, pMsg->type);

  tout  << " {";

  switch (pMsg->type) {
    case HUB_MSG_TYPE_MODEM_STATUS:
      PrintMaskedFields(tout, fieldNameTableModemStatus, pMsg->u.val);
      break;
    case HUB_MSG_TYPE_LINE_STATUS:
    case HUB_MSG_TYPE_SET_LSR:
      PrintMaskedFields(tout, fieldNameTableLineStatus, pMsg->u.val);
      break;
    case HUB_MSG_TYPE_SET_PIN_STATE:
      PrintMaskedFields(tout, codeNameTableSetPinState, pMsg->u.val);
      break;
    case HUB_MSG_TYPE_SET_OUT_OPTS: {
      tout << "[";
      BOOL delimitNext = FALSE;
      delimitNext = PrintFields(tout, codeNameTableSetPinState, SO_O2V_PIN_STATE(pMsg->u.val), delimitNext, "SET_");
      PrintFields(tout, fieldNameTableSoOptions, pMsg->u.val & ~SO_V2O_PIN_STATE(-1), delimitNext);
      tout << "]";
      break;
    }
    case HUB_MSG_TYPE_GET_IN_OPTS: {
      tout << hex << "&" << pMsg->u.pv.pVal << "[" << dec;
      PrintGoOptions(tout, *pMsg->u.pv.pVal);
      tout << "] [";
      PrintGoOptions(tout, pMsg->u.pv.val);
      tout << "]";
      break;
    }
    case HUB_MSG_TYPE_FAIL_IN_OPTS: {
      tout << "[";
      PrintGoOptions(tout, pMsg->u.val);
      tout << "]";
      break;
    }
    case HUB_MSG_TYPE_GET_ESC_OPTS: {
      tout << hex << "&" << pMsg->u.pv.pVal << "[" << dec;
      tout << "CHAR_0x" << hex << (unsigned)ESC_OPTS_O2V_ESCCHAR(*pMsg->u.pv.pVal) << dec;
      PrintEscOptions(tout, *pMsg->u.pv.pVal & ~ESC_OPTS_V2O_ESCCHAR(-1), TRUE);
      tout << "]";
      break;
    }
    case HUB_MSG_TYPE_FAIL_ESC_OPTS: {
      tout << hex << "&" << pMsg->u.pv.pVal << "[" << dec;
      PrintGoOptions(tout, *pMsg->u.pv.pVal);
      tout << "] [";
      PrintEscOptions(tout, pMsg->u.pv.val & ~ESC_OPTS_V2O_ESCCHAR(-1));
      tout << "]";
      break;
    }
    default:
      PrintMsgBody(tout, pMsg);
  }

  tout  << "}" << endl;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK InMethod(
    HFILTER hFilter,
    int nFromPort,
    HUB_MSG *pInMsg,
    HUB_MSG **DEBUG_PARAM(ppEchoMsg))
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(pInMsg != NULL);
  _ASSERTE(ppEchoMsg != NULL);
  _ASSERTE(*ppEchoMsg == NULL);

  _ASSERTE(((Filter *)hFilter)->pTraceStream != NULL);
  ostream &tout = *((Filter *)hFilter)->pTraceStream;

  PrintTime(tout);

  tout << ((Filter *)hFilter)->PortName(nFromPort) << "-("
       << ((Filter *)hFilter)->FilterName() << ")->: ";

  PrintMsg(tout, pInMsg);

  return TRUE;
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

  _ASSERTE(((Filter *)hFilter)->pTraceStream != NULL);
  ostream &tout = *((Filter *)hFilter)->pTraceStream;

  PrintTime(tout);

  tout << ((Filter *)hFilter)->PortName(nToPort) << "<-("
       << ((Filter *)hFilter)->FilterName() << ")-"
       << ((Filter *)hFilter)->PortName(nFromPort) << ": ";

  PrintMsg(tout, pOutMsg);

  return TRUE;
}
///////////////////////////////////////////////////////////////
static const FILTER_ROUTINES_A routines = {
  sizeof(FILTER_ROUTINES_A),
  GetPluginType,
  GetPluginAbout,
  Help,
  ConfigStart,
  Config,
  ConfigStop,
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
  if (!ROUTINE_IS_VALID(pHubRoutines, pPortName) ||
      !ROUTINE_IS_VALID(pHubRoutines, pFilterName))
  {
    return NULL;
  }

  pPortName = pHubRoutines->pPortName;
  pFilterName = pHubRoutines->pFilterName;

  return plugins;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
