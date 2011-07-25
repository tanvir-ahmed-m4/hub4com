/*
 * plugins_api.h
 *
 * Copyright (c) 2008-2011 Vyacheslav Frolov
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
 */

#ifndef _PLUGINS_API_H
#define _PLUGINS_API_H

#ifdef  __cplusplus
extern "C" {
#endif

/*******************************************************************/
#define HUB_MSG_UNION_TYPES_MASK   ((DWORD)0xFF000000)
#define HUB_MSG_UNION_TYPE_NONE    ((DWORD)0x00000000)
#define HUB_MSG_UNION_TYPE_BUF     ((DWORD)0x01000000)
#define HUB_MSG_UNION_TYPE_VAL     ((DWORD)0x02000000)
#define HUB_MSG_UNION_TYPE_PVAL    ((DWORD)0x03000000)
#define HUB_MSG_UNION_TYPE_HVAL    ((DWORD)0x04000000)
#define HUB_MSG_UNION_TYPE_HVAL2   ((DWORD)0x05000000)
/*******************************************************************/
#define HUB_MSG_VAL_TYPES_MASK     ((DWORD)0x00FF0000)
#define HUB_MSG_VAL_TYPE_MASK_VAL  ((DWORD)0x00010000)
#define   VAL2MASK(v)              ((DWORD)(WORD)(v) << 16)
#define   MASK2VAL(m)              ((WORD)((m) >> 16))
#define HUB_MSG_VAL_TYPE_BOOL      ((DWORD)0x00020000)
#define HUB_MSG_VAL_TYPE_MSG_TYPE  ((DWORD)0x00030000)
#define HUB_MSG_VAL_TYPE_UINT      ((DWORD)0x00040000)
#define HUB_MSG_VAL_TYPE_LC        ((DWORD)0x00050000)
#define   LC_MASK_BYTESIZE         ((DWORD)1 << 24)
#define   LC_MASK_PARITY           ((DWORD)1 << 25)
#define   LC_MASK_STOPBITS         ((DWORD)1 << 26)
#define   LC2VAL_BYTESIZE(t)       ((BYTE)((t)))
#define   VAL2LC_BYTESIZE(v)       ((DWORD)(BYTE)(v))
#define   LC2VAL_PARITY(t)         ((BYTE)((t) >> 8))
#define   VAL2LC_PARITY(v)         ((DWORD)(BYTE)(v) << 8)
#define   LC2VAL_STOPBITS(t)       ((BYTE)((t) >> 16))
#define   VAL2LC_STOPBITS(v)       ((DWORD)(BYTE)(v) << 16)
/*******************************************************************/
#define HUB_MSG_ROUTE_FLOW_CONTROL ((DWORD)0x00008000)
/*******************************************************************/
#define HUB_MSG_T2N(t)             ((BYTE)((t) & 0xFF))
/*******************************************************************/
#define HUB_MSG_TYPE_EMPTY         (0   | HUB_MSG_UNION_TYPE_NONE)
#define HUB_MSG_TYPE_LINE_DATA     (1   | HUB_MSG_UNION_TYPE_BUF)
#define HUB_MSG_TYPE_CONNECT       (2   | HUB_MSG_UNION_TYPE_VAL | HUB_MSG_VAL_TYPE_BOOL)
/*************/
#define HUB_MSG_TYPE_MODEM_STATUS  (3   | HUB_MSG_UNION_TYPE_VAL | HUB_MSG_VAL_TYPE_MASK_VAL)
#define   MODEM_STATUS_DCTS        0x01
#define   MODEM_STATUS_DDSR        0x02
#define   MODEM_STATUS_TERI        0x04
#define   MODEM_STATUS_DDCD        0x08
#define   MODEM_STATUS_CTS         0x10
#define   MODEM_STATUS_DSR         0x20
#define   MODEM_STATUS_RI          0x40
#define   MODEM_STATUS_DCD         0x80
/*************/
#define HUB_MSG_TYPE_LINE_STATUS   (4   | HUB_MSG_UNION_TYPE_VAL | HUB_MSG_VAL_TYPE_MASK_VAL)
#define   LINE_STATUS_DR           0x01
#define   LINE_STATUS_OE           0x02
#define   LINE_STATUS_PE           0x04
#define   LINE_STATUS_FE           0x08
#define   LINE_STATUS_BI           0x10
#define   LINE_STATUS_THRE         0x20
#define   LINE_STATUS_TEMT         0x40
#define   LINE_STATUS_FIFOERR      0x80
/*************/
#define HUB_MSG_TYPE_SET_PIN_STATE (5   | HUB_MSG_UNION_TYPE_VAL | HUB_MSG_VAL_TYPE_MASK_VAL)
#define   SPS_P2V_MCR(p)           ((BYTE)(p))
#define   SPS_V2P_MCR(v)           ((WORD)(BYTE)(v))
#define   PIN_STATE_DTR            0x0001
#define   PIN_STATE_RTS            0x0002
#define   PIN_STATE_OUT1           0x0004
#define   PIN_STATE_OUT2           0x0008
#define   SPS_P2V_MST(p)           ((BYTE)(((p) & 0xF000) >> 8))
#define   SPS_V2P_MST(v)           (((WORD)(BYTE)(v) << 8) & 0xF000)
#define   PIN_STATE_CTS            SPS_V2P_MST(MODEM_STATUS_CTS)
#define   PIN_STATE_DSR            SPS_V2P_MST(MODEM_STATUS_DSR)
#define   PIN_STATE_RI             SPS_V2P_MST(MODEM_STATUS_RI)
#define   PIN_STATE_DCD            SPS_V2P_MST(MODEM_STATUS_DCD)
#define   PIN_STATE_BREAK          0x0100
/*************/
#define HUB_MSG_TYPE_GET_IN_OPTS   (6   | HUB_MSG_UNION_TYPE_PVAL)
/*
 *      Input
 *        HUB_MSG.u.pv.val   - the set and set-ID of input-options that can be requested
 *      Output
 *       *HUB_MSG.u.pv.pVal  - the cumulative set of requested input-options
 *                             (suppose set-ID is from Input, set-ID field reserved for future)
 */
#define   GO_O2I(o)                ((DWORD)(o) >> 30)      /* unpack set-ID      */
#define   GO_I2O(i)                ((DWORD)i << 30)        /* pack set-ID        */
#define   GO0_ESCAPE_MODE          ((DWORD)1 << 0)
#define   GO0_LBR_STATUS           ((DWORD)1 << 1)
#define   GO0_LLC_STATUS           ((DWORD)1 << 2)
#define   GO1_O2V_MODEM_STATUS(o)  ((BYTE)(o))
#define   GO1_V2O_MODEM_STATUS(v)  ((DWORD)(BYTE)(v))
#define   GO1_O2V_LINE_STATUS(o)   ((BYTE)((o) >> 8))
#define   GO1_V2O_LINE_STATUS(v)   ((DWORD)(BYTE)(v) << 8)
#define   GO1_RBR_STATUS           ((DWORD)1 << 16)
#define   GO1_RLC_STATUS           ((DWORD)1 << 17)
#define   GO1_BREAK_STATUS         ((DWORD)1 << 18)
#define   GO1_PURGE_TX_IN          ((DWORD)1 << 19)
/*************/
#define HUB_MSG_TYPE_FAIL_IN_OPTS  (7   | HUB_MSG_UNION_TYPE_VAL)
/*
 *      Input
 *        HUB_MSG.u.val      - the set and set-ID of rejected input-options
 */
/*************/
#define HUB_MSG_TYPE_SET_OUT_OPTS  (8   | HUB_MSG_UNION_TYPE_VAL)
/*
 *      Input
 *        HUB_MSG.u.val      - the cumulative set of requested output-options
 */
#define   SO_O2V_PIN_STATE(o)      ((WORD)(o))
#define   SO_V2O_PIN_STATE(v)      ((DWORD)(WORD)(v))
#define   SO_O2V_LINE_STATUS(o)    ((BYTE)((o) >> 16))
#define   SO_V2O_LINE_STATUS(v)    ((DWORD)(BYTE)(v) << 16)
#define   SO_SET_BR                ((DWORD)1 << 24)
#define   SO_SET_LC                ((DWORD)1 << 25)
#define   SO_PURGE_TX              ((DWORD)1 << 26)
/*************/
#define HUB_MSG_TYPE_RBR_STATUS    (9   | HUB_MSG_UNION_TYPE_VAL | HUB_MSG_VAL_TYPE_UINT)
#define HUB_MSG_TYPE_RLC_STATUS    (10  | HUB_MSG_UNION_TYPE_VAL | HUB_MSG_VAL_TYPE_LC)
/*************/
#define HUB_MSG_TYPE_COUNT_REPEATS (11  | HUB_MSG_UNION_TYPE_PVAL | HUB_MSG_VAL_TYPE_MSG_TYPE)
/*
 *      Input
 *        HUB_MSG.u.pv.val   - the message-type that can be repeated multiple times
 *      Output
 *       *HUB_MSG.u.pv.pVal  - the cumulative number of requested repeats
 */
/*************/
#define HUB_MSG_TYPE_GET_ESC_OPTS  (12  | HUB_MSG_UNION_TYPE_PVAL)
/*
 *      Input
 *        HUB_MSG.u.pv.val   - the set of escape-mode-input-options that can be requested
 *      Output
 *       *HUB_MSG.u.pv.pVal  - the cumulative set of requested escape-mode-input-options
 */
#define   ESC_OPTS_MAP_EO_2_GO1(e) ((DWORD)(e) & 0x00FFFFFF)
#define   ESC_OPTS_MAP_GO1_2_EO(g) ((DWORD)(g) & 0x00FFFFFF)
#define   ESC_OPTS_O2V_ESCCHAR(o)  ((BYTE)(o >> 24))
#define   ESC_OPTS_V2O_ESCCHAR(v)  ((DWORD)(BYTE)(v) << 24)
/*************/
#define HUB_MSG_TYPE_FAIL_ESC_OPTS (13  | HUB_MSG_UNION_TYPE_PVAL)
/*
 *      Input
 *        HUB_MSG.u.val      - the set of rejected escape-mode-input-options
 *      Output
 *       *HUB_MSG.u.pv.pVal  - the cumulative set of requested input-options
 *                             (suppose set-ID is 1, set-ID field reserved for future)
 */
/*************/
#define HUB_MSG_TYPE_BREAK_STATUS  (14  | HUB_MSG_UNION_TYPE_VAL | HUB_MSG_VAL_TYPE_BOOL)
#define HUB_MSG_TYPE_SET_BR        (15  | HUB_MSG_UNION_TYPE_VAL | HUB_MSG_VAL_TYPE_UINT)
#define HUB_MSG_TYPE_SET_LC        (16  | HUB_MSG_UNION_TYPE_VAL | HUB_MSG_VAL_TYPE_LC)
#define HUB_MSG_TYPE_SET_LSR       (17  | HUB_MSG_UNION_TYPE_VAL | HUB_MSG_VAL_TYPE_MASK_VAL)
#define HUB_MSG_TYPE_LBR_STATUS    (18  | HUB_MSG_UNION_TYPE_VAL | HUB_MSG_VAL_TYPE_UINT)
#define HUB_MSG_TYPE_LLC_STATUS    (19  | HUB_MSG_UNION_TYPE_VAL | HUB_MSG_VAL_TYPE_LC)
#define HUB_MSG_TYPE_LOOP_TEST     (20  | HUB_MSG_UNION_TYPE_HVAL)
#define HUB_MSG_TYPE_ADD_XOFF_XON  (21  | HUB_MSG_ROUTE_FLOW_CONTROL | HUB_MSG_UNION_TYPE_VAL | HUB_MSG_VAL_TYPE_BOOL)
#define HUB_MSG_TYPE_PURGE_TX_IN   (22  | HUB_MSG_UNION_TYPE_NONE)
#define HUB_MSG_TYPE_PURGE_TX      (23  | HUB_MSG_UNION_TYPE_NONE)
#define HUB_MSG_TYPE_TICK          (24  | HUB_MSG_UNION_TYPE_HVAL2)
/*******************************************************************/
typedef struct _HUB_MSG {
  DWORD type;
  union {
    struct {
      BYTE *pBuf;
      DWORD size;
    } buf;
    struct {
      DWORD *pVal;
      DWORD val;
    } pv;
    struct {
      HANDLE hVal0;
      HANDLE hVal1;
    } hv2;
    DWORD val;
    HANDLE hVal;
  } u;
} HUB_MSG;
/*******************************************************************/
DECLARE_HANDLE(HMASTERPORT);
DECLARE_HANDLE(HMASTERFILTER);
DECLARE_HANDLE(HMASTERFILTERINSTANCE);
DECLARE_HANDLE(HMASTERTIMER);
DECLARE_HANDLE(HTIMEROWNER);
DECLARE_HANDLE(HTIMERPARAM);
DECLARE_HANDLE(HFILTER);
/*******************************************************************/
typedef struct _ARG_INFO_A {
  size_t size;
  const char *pFile;
  int iLine;
  const char *pReference;
} ARG_INFO_A;
/*******************************************************************/
typedef BYTE *(CALLBACK ROUTINE_BUF_ALLOC)(
        DWORD size);
typedef VOID (CALLBACK ROUTINE_BUF_FREE)(
        BYTE *pBuf);
typedef VOID (CALLBACK ROUTINE_BUF_APPEND)(
        BYTE **ppBuf,
        DWORD offset,
        const BYTE *pSrc,
        DWORD sizeSrc);
typedef BOOL (CALLBACK ROUTINE_MSG_REPLACE_BUF)(
        HUB_MSG *pMsg,
        DWORD type,
        const BYTE *pSrc,
        DWORD sizeSrc);
typedef HUB_MSG *(CALLBACK ROUTINE_MSG_INSERT_BUF)(
        HUB_MSG *pPrevMsg,
        DWORD type,
        const BYTE *pSrc,
        DWORD sizeSrc);
typedef BOOL (CALLBACK ROUTINE_MSG_REPLACE_VAL)(
        HUB_MSG *pMsg,
        DWORD type,
        DWORD val);
typedef HUB_MSG *(CALLBACK ROUTINE_MSG_INSERT_VAL)(
        HUB_MSG *pMsg,
        DWORD type,
        DWORD val);
typedef BOOL (CALLBACK ROUTINE_MSG_REPLACE_NONE)(
        HUB_MSG *pMsg,
        DWORD type);
typedef HUB_MSG *(CALLBACK ROUTINE_MSG_INSERT_NONE)(
        HUB_MSG *pMsg,
        DWORD type);
typedef const char *(CALLBACK ROUTINE_PORT_NAME_A)(
        HMASTERPORT hMasterPort);
typedef const char *(CALLBACK ROUTINE_FILTER_NAME_A)(
        HMASTERFILTER hMasterFilter);
typedef void (CALLBACK ROUTINE_ON_READ)(
        HMASTERPORT hMasterPort,
        HUB_MSG *pMsg);
typedef HMASTERTIMER (CALLBACK ROUTINE_TIMER_CREATE)(
        HTIMEROWNER hTimerOwner);
typedef BOOL (CALLBACK ROUTINE_TIMER_SET)(
        HMASTERTIMER hMasterTimer,
        HMASTERPORT hMasterPort,
        const LARGE_INTEGER *pDueTime,
        LONG period,
        HTIMERPARAM hTimerParam);
typedef void (CALLBACK ROUTINE_TIMER_CANCEL)(
        HMASTERTIMER hMasterTimer);
typedef void (CALLBACK ROUTINE_TIMER_DELETE)(
        HMASTERTIMER hMasterTimer);
typedef HMASTERPORT (CALLBACK ROUTINE_FILTERPORT)(
        HMASTERFILTERINSTANCE hMasterFilterInstance);
typedef HFILTER (CALLBACK ROUTINE_GET_FILTER)(
        HMASTERFILTERINSTANCE hMasterFilterInstance);
typedef const ARG_INFO_A *(CALLBACK ROUTINE_GET_ARG_INFO_A)(
        const char *pArg);
/*******************************************************************/
typedef struct _HUB_ROUTINES_A {
  size_t size;
  ROUTINE_BUF_ALLOC *pBufAlloc;
  ROUTINE_BUF_FREE *pBufFree;
  ROUTINE_BUF_APPEND *pBufAppend;
  ROUTINE_MSG_REPLACE_BUF *pMsgReplaceBuf;
  ROUTINE_MSG_INSERT_BUF *pMsgInsertBuf;
  ROUTINE_MSG_REPLACE_VAL *pMsgReplaceVal;
  ROUTINE_MSG_INSERT_VAL *pMsgInsertVal;
  ROUTINE_MSG_REPLACE_NONE *pMsgReplaceNone;
  ROUTINE_MSG_INSERT_NONE *pMsgInsertNone;
  ROUTINE_PORT_NAME_A *pPortName;
  ROUTINE_FILTER_NAME_A *pFilterName;
  ROUTINE_ON_READ *pOnRead;
  ROUTINE_TIMER_CREATE *pTimerCreate;
  ROUTINE_TIMER_SET *pTimerSet;
  ROUTINE_TIMER_CANCEL *pTimerCancel;
  ROUTINE_TIMER_DELETE *pTimerDelete;
  ROUTINE_FILTERPORT *pFilterPort;
  ROUTINE_GET_FILTER *pGetFilter;
  ROUTINE_GET_ARG_INFO_A *pGetArgInfo;
} HUB_ROUTINES_A;
/*******************************************************************/
typedef enum _PLUGIN_TYPE {
  PLUGIN_TYPE_INVALID,
  PLUGIN_TYPE_FILTER,
  PLUGIN_TYPE_DRIVER,
} PLUGIN_TYPE;
/*******************************************************************/
DECLARE_HANDLE(HCONFIG);
/*******************************************************************/
typedef struct _PLUGIN_ABOUT_A {
  size_t size;
  const char *pName;
  const char *pCopyright;
  const char *pLicense;
  const char *pDescription;
} PLUGIN_ABOUT_A;
/*******************************************************************/
typedef PLUGIN_TYPE (CALLBACK PLUGIN_GET_TYPE)();
typedef const PLUGIN_ABOUT_A *(CALLBACK PLUGIN_GET_ABOUT_A)();
typedef void (CALLBACK PLUGIN_HELP_A)(
        const char *pProgPath);
typedef HCONFIG (CALLBACK PLUGIN_CONFIG_START)();
typedef BOOL (CALLBACK PLUGIN_CONFIG_A)(
        HCONFIG hConfig,
        const char *pArg);
typedef void (CALLBACK PLUGIN_CONFIG_STOP)(
        HCONFIG hConfig);
/*******************************************************************/
#define COMMON_PLUGIN_ROUTINES_A \
  size_t size; \
  PLUGIN_GET_TYPE *pGetPluginType; \
  PLUGIN_GET_ABOUT_A *pGetPluginAbout; \
  PLUGIN_HELP_A *pHelp; \
  PLUGIN_CONFIG_START *pConfigStart; \
  PLUGIN_CONFIG_A *pConfig; \
  PLUGIN_CONFIG_STOP *pConfigStop; \
/*******************************************************************/
typedef struct _PLUGIN_ROUTINES_A {
  COMMON_PLUGIN_ROUTINES_A
} PLUGIN_ROUTINES_A;
/*******************************************************************/
#define PLUGIN_INIT_PROC_NAME "Init"
#define PLUGIN_INIT_PROC_NAME_A PLUGIN_INIT_PROC_NAME "A"
/*******************************************************************/
typedef const PLUGIN_ROUTINES_A *const *(CALLBACK PLUGIN_INIT_A)(
        const HUB_ROUTINES_A *pHubRoutines);
/*******************************************************************/
DECLARE_HANDLE(HFILTERINSTANCE);
/*******************************************************************/
typedef HFILTER (CALLBACK FILTER_CREATE_A)(
        HMASTERFILTER hMasterFilter,
        HCONFIG hConfig,
        int argc,
        const char *const argv[]);
typedef void (CALLBACK FILTER_DELETE)(
        HFILTER hFilter);
typedef HFILTERINSTANCE (CALLBACK FILTER_CREATE_INSTANCE)(
        HMASTERFILTERINSTANCE hMasterFilterInstance);
typedef void (CALLBACK FILTER_DELETE_INSTANCE)(
        HFILTERINSTANCE hFilterInstance);
typedef BOOL (CALLBACK FILTER_IN_METHOD)(
        HFILTER hFilter,
        HFILTERINSTANCE hFilterInstance,
        HUB_MSG *pInMsg,
        HUB_MSG **ppEchoMsg);
typedef BOOL (CALLBACK FILTER_OUT_METHOD)(
        HFILTER hFilter,
        HFILTERINSTANCE hFilterInstance,
        HMASTERPORT hFromPort,
        HUB_MSG *pOutMsg);
/*******************************************************************/
typedef struct _FILTER_ROUTINES_A {
  COMMON_PLUGIN_ROUTINES_A
  FILTER_CREATE_A *pCreate;
  FILTER_DELETE *pDelete;
  FILTER_CREATE_INSTANCE *pCreateInstance;
  FILTER_DELETE_INSTANCE *pDeleteInstance;
  FILTER_IN_METHOD *pInMethod;
  FILTER_OUT_METHOD *pOutMethod;
} FILTER_ROUTINES_A;
/*******************************************************************/
DECLARE_HANDLE(HPORT);
/*******************************************************************/
typedef HPORT (CALLBACK PORT_CREATE_A)(
        HCONFIG hConfig,
        const char *pPath);
typedef const char *(CALLBACK PORT_GET_NAME_A)(
        HPORT hPort);
typedef void (CALLBACK PORT_SET_NAME_A)(
        HPORT hPort,
        const char *pName);
typedef BOOL (CALLBACK PORT_INIT)(
        HPORT hPort,
        HMASTERPORT hMasterPort);
typedef BOOL (CALLBACK PORT_START)(
        HPORT hPort);
typedef BOOL (CALLBACK PORT_FAKE_READ_FILTER)(
        HPORT hPort,
        HUB_MSG *pInMsg);
typedef BOOL (CALLBACK PORT_WRITE)(
        HPORT hPort,
        HUB_MSG *pMsg);
typedef void (CALLBACK PORT_LOST_REPORT)(
        HPORT hPort);
/*******************************************************************/
typedef struct _PORT_ROUTINES_A {
  COMMON_PLUGIN_ROUTINES_A
  PORT_CREATE_A *pCreate;
  PORT_GET_NAME_A *pGetPortName;
  PORT_SET_NAME_A *pSetPortName;
  PORT_INIT *pInit;
  PORT_START *pStart;
  PORT_FAKE_READ_FILTER *pFakeReadFilter;
  PORT_WRITE *pWrite;
  PORT_LOST_REPORT *pLostReport;
} PORT_ROUTINES_A;
/*******************************************************************/
#define ITEM_IS_VALID(pStruct, item) \
        (((BYTE *)(&(pStruct)->item + 1)) <= ((BYTE *)(pStruct) + (pStruct)->size))

#define ROUTINE_GET(pStruct, pRoutine) \
        (ITEM_IS_VALID(pStruct, pRoutine) \
          ? (pStruct)->pRoutine \
          : NULL)

#define ROUTINE_IS_VALID(pStruct, pRoutine) \
        (ROUTINE_GET(pStruct, pRoutine) != NULL)
/*******************************************************************/

#ifdef  __cplusplus
}
#endif

#endif  /* _PLUGINS_API_H */
