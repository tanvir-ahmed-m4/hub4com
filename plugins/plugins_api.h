/*
 * plugins_api.h
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
 */

#ifndef _PLUGINS_API_H
#define _PLUGINS_API_H

#ifdef  __cplusplus
extern "C" {
#endif

/*******************************************************************/
#define HUB_MSG_UNION_TYPE_MASK    0xF000
#define HUB_MSG_UNION_TYPE_NONE    0x0000
#define HUB_MSG_UNION_TYPE_BUF     0x1000
#define HUB_MSG_UNION_TYPE_VAL     0x2000
#define HUB_MSG_UNION_TYPE_PVAL    0x3000
/*******************************************************************/
#define HUB_MSG_VAL_TYPES_MASK     0x0F00
#define HUB_MSG_VAL_TYPE_MASK_VAL  0x0100
#define   VAL2MASK(v)              ((DWORD)(WORD)(v) << 16)
#define   MASK2VAL(m)              ((WORD)((m) >> 16))
/*******************************************************************/
#define HUB_MSG_TYPE_EMPTY         (0   | HUB_MSG_UNION_TYPE_NONE)
#define HUB_MSG_TYPE_LINE_DATA     (1   | HUB_MSG_UNION_TYPE_BUF)
#define HUB_MSG_TYPE_CONNECT       (2   | HUB_MSG_UNION_TYPE_VAL)
#define HUB_MSG_TYPE_MODEM_STATUS  (3   | HUB_MSG_UNION_TYPE_VAL | HUB_MSG_VAL_TYPE_MASK_VAL)
#define   MODEM_STATUS_DCTS        0x01
#define   MODEM_STATUS_DDSR        0x02
#define   MODEM_STATUS_TERI        0x04
#define   MODEM_STATUS_DDCD        0x08
#define   MODEM_STATUS_CTS         0x10
#define   MODEM_STATUS_DSR         0x20
#define   MODEM_STATUS_RI          0x40
#define   MODEM_STATUS_DCD         0x80
#define HUB_MSG_TYPE_LINE_STATUS   (4   | HUB_MSG_UNION_TYPE_VAL | HUB_MSG_VAL_TYPE_MASK_VAL)
#define   LINE_STATUS_DR           0x01
#define   LINE_STATUS_OE           0x02
#define   LINE_STATUS_PE           0x04
#define   LINE_STATUS_FE           0x08
#define   LINE_STATUS_BI           0x10
#define   LINE_STATUS_THRE         0x20
#define   LINE_STATUS_TEMT         0x40
#define   LINE_STATUS_FIFOERR      0x80
#define HUB_MSG_TYPE_SET_PIN_STATE (5   | HUB_MSG_UNION_TYPE_VAL | HUB_MSG_VAL_TYPE_MASK_VAL)
#define   PIN_STATE_RTS            0x01
#define   PIN_STATE_DTR            0x02
#define   PIN_STATE_OUT1           0x04
#define   PIN_STATE_OUT2           0x08
#define   PIN_STATE_BREAK          0x10
#define HUB_MSG_TYPE_GET_IN_OPTS   (6   | HUB_MSG_UNION_TYPE_PVAL)
#define   GO_O2V_MODEM_STATUS(o)   ((BYTE)(o))
#define   GO_V2O_MODEM_STATUS(v)   ((DWORD)(BYTE)(v))
#define   GO_O2V_LINE_STATUS(o)    ((BYTE)((o) >> 8))
#define   GO_V2O_LINE_STATUS(v)    ((DWORD)(BYTE)(v) << 8)
#define   GO_ESCAPE_MODE           0x00010000
#define   GO_RBR_STATUS            0x00020000
#define   GO_RLC_STATUS            0x00040000
#define HUB_MSG_TYPE_SET_OUT_OPTS  (7   | HUB_MSG_UNION_TYPE_VAL)
#define   SO_O2V_PIN_STATE(o)      ((BYTE)(o))
#define   SO_V2O_PIN_STATE(v)      ((DWORD)(BYTE)(v))
#define HUB_MSG_TYPE_FAIL_IN_OPTS  (8   | HUB_MSG_UNION_TYPE_VAL)
#define HUB_MSG_TYPE_RBR_STATUS    (9   | HUB_MSG_UNION_TYPE_VAL)
#define HUB_MSG_TYPE_RLC_STATUS    (10  | HUB_MSG_UNION_TYPE_VAL)
#define HUB_MSG_TYPE_COUNT_REPEATS (11  | HUB_MSG_UNION_TYPE_PVAL)
/*******************************************************************/
typedef struct _HUB_MSG {
  WORD type;
  union {
    struct {
      BYTE *pBuf;
      DWORD size;
    } buf;
    struct {
      DWORD *pVal;
      DWORD val;
    } pv;
    DWORD val;
  } u;
} HUB_MSG;
/*******************************************************************/
DECLARE_HANDLE(HHUB);
DECLARE_HANDLE(HMASTERPORT);
DECLARE_HANDLE(HFILTER);
/*******************************************************************/
typedef BYTE *(CALLBACK ROUTINE_BUF_ALLOC)(
        DWORD size);
typedef VOID (CALLBACK ROUTINE_BUF_FREE)(
        BYTE *pBuf);
typedef HUB_MSG *(CALLBACK ROUTINE_MSG_REPLACE_BUF)(
        HUB_MSG *pMsg,
        WORD type,
        const BYTE *pSrc,
        DWORD sizeSrc);
typedef HUB_MSG *(CALLBACK ROUTINE_MSG_INSERT_BUF)(
        HUB_MSG *pPrevMsg,
        WORD type,
        const BYTE *pSrc,
        DWORD sizeSrc);
typedef HUB_MSG *(CALLBACK ROUTINE_MSG_REPLACE_VAL)(
        HUB_MSG *pMsg,
        WORD type,
        DWORD val);
typedef HUB_MSG *(CALLBACK ROUTINE_MSG_INSERT_VAL)(
        HUB_MSG *pMsg,
        WORD type,
        DWORD val);
typedef HUB_MSG *(CALLBACK ROUTINE_MSG_REPLACE_NONE)(
        HUB_MSG *pMsg,
        WORD type);
typedef HUB_MSG *(CALLBACK ROUTINE_MSG_INSERT_NONE)(
        HUB_MSG *pMsg,
        WORD type);
typedef int (CALLBACK ROUTINE_NUM_PORTS)(
        HHUB hHub);
typedef const char *(CALLBACK ROUTINE_PORT_NAME_A)(
        HHUB hHub,
        int n);
typedef const char *(CALLBACK ROUTINE_FILTER_NAME_A)(
        HHUB hHub,
        HFILTER hFilter);
typedef void (CALLBACK ROUTINE_ON_XOFF)(
        HHUB hHub,
        HMASTERPORT hMasterPort);
typedef void (CALLBACK ROUTINE_ON_XON)(
        HHUB hHub,
        HMASTERPORT hMasterPort);
typedef void (CALLBACK ROUTINE_ON_READ)(
        HHUB hHub,
        HMASTERPORT hMasterPort,
        HUB_MSG *pMsg);
/*******************************************************************/
typedef struct _HUB_ROUTINES_A {
  size_t size;
  ROUTINE_BUF_ALLOC *pBufAlloc;
  ROUTINE_BUF_FREE *pBufFree;
  ROUTINE_MSG_REPLACE_BUF *pMsgReplaceBuf;
  ROUTINE_MSG_INSERT_BUF *pMsgInsertBuf;
  ROUTINE_MSG_REPLACE_VAL *pMsgReplaceVal;
  ROUTINE_MSG_INSERT_VAL *pMsgInsertVal;
  ROUTINE_MSG_REPLACE_NONE *pMsgReplaceNone;
  ROUTINE_MSG_INSERT_NONE *pMsgInsertNone;
  ROUTINE_NUM_PORTS *pNumPorts;
  ROUTINE_PORT_NAME_A *pPortName;
  ROUTINE_FILTER_NAME_A *pFilterName;
  ROUTINE_ON_XOFF *pOnXoff;
  ROUTINE_ON_XON *pOnXon;
  ROUTINE_ON_READ *pOnRead;
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
typedef HFILTER (CALLBACK FILTER_CREATE_A)(
        HCONFIG hConfig,
        int argc,
        const char *const argv[]);
typedef BOOL (CALLBACK FILTER_INIT)(
        HFILTER hFilter,
        HHUB hHub);
typedef BOOL (CALLBACK FILTER_IN_METHOD)(
        HFILTER hFilter,
        int nFromPort,
        HUB_MSG *pInMsg,
        HUB_MSG **ppEchoMsg);
typedef BOOL (CALLBACK FILTER_OUT_METHOD)(
        HFILTER hFilter,
        int nFromPort,
        int nToPort,
        HUB_MSG *pOutMsg);
/*******************************************************************/
typedef struct _FILTER_ROUTINES_A {
  COMMON_PLUGIN_ROUTINES_A
  FILTER_CREATE_A *pCreate;
  FILTER_INIT *pInit;
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
        HMASTERPORT hMasterPort,
        HHUB hHub);
typedef BOOL (CALLBACK PORT_START)(
        HPORT hPort);
typedef BOOL (CALLBACK PORT_FAKE_READ_FILTER)(
        HPORT hPort,
        HUB_MSG *pInMsg);
typedef BOOL (CALLBACK PORT_WRITE)(
        HPORT hPort,
        HUB_MSG *pMsg);
typedef void (CALLBACK PORT_ADD_XOFF)(
        HPORT hPort);
typedef void (CALLBACK PORT_ADD_XON)(
        HPORT hPort);
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
  PORT_ADD_XOFF *pAddXoff;
  PORT_ADD_XON *pAddXon;
  PORT_LOST_REPORT *pLostReport;
} PORT_ROUTINES_A;
/*******************************************************************/
#define ROUTINE_GET(pStruct, pRoutine) \
        ((((BYTE *)(&(pStruct)->pRoutine + 1)) <= ((BYTE *)(pStruct) + (pStruct)->size)) \
          ? (pStruct)->pRoutine \
          : NULL)

#define ROUTINE_IS_VALID(pStruct, pRoutine) \
        (ROUTINE_GET(pStruct, pRoutine) != NULL)
/*******************************************************************/

#ifdef  __cplusplus
}
#endif

#endif  /* _PLUGINS_API_H */
