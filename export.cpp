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
 * Revision 1.12  2009/02/02 15:21:42  vfrolov
 * Optimized filter's API
 *
 * Revision 1.11  2009/01/26 14:55:29  vfrolov
 * Added signature checking for Timer
 *
 * Revision 1.10  2009/01/23 16:48:49  vfrolov
 * Exported timer routines
 *
 * Revision 1.9  2008/12/19 18:23:05  vfrolov
 * Changed message type to 32 bit
 *
 * Revision 1.8  2008/11/27 16:19:16  vfrolov
 * Added pBufAppend()
 *
 * Revision 1.7  2008/11/24 16:30:56  vfrolov
 * Removed pOnXoffXon
 *
 * Revision 1.6  2008/11/24 12:46:16  vfrolov
 * Changed plugin API
 *
 * Revision 1.5  2008/11/13 08:07:40  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.4  2008/10/16 09:24:23  vfrolov
 * Changed return type of ROUTINE_MSG_REPLACE_*() to BOOL
 *
 * Revision 1.3  2008/08/20 09:06:48  vfrolov
 * Added HUB_ROUTINES_A::pFilterName
 *
 * Revision 1.2  2008/08/19 16:45:27  vfrolov
 * Added missing size setting to msg_insert_buf()
 *
 * Revision 1.1  2008/03/26 08:35:03  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "plugins/plugins_api.h"

#include "export.h"
#include "port.h"
#include "comhub.h"
#include "bufutils.h"
#include "hubmsg.h"
#include "filter.h"
#include "timer.h"

///////////////////////////////////////////////////////////////
static BYTE * CALLBACK buf_alloc(DWORD size)
{
  return BufAlloc(size);
}
///////////////////////////////////////////////////////////////
static VOID CALLBACK buf_free(BYTE *pBuf)
{
  BufFree(pBuf);
}
///////////////////////////////////////////////////////////////
static VOID CALLBACK buf_append(BYTE **ppBuf, DWORD offset, const BYTE *pSrc, DWORD sizeSrc)
{
  BufAppend(ppBuf, offset, pSrc, sizeSrc);
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK msg_replace_buf(HUB_MSG *pMsg, DWORD type, const BYTE *pSrc, DWORD sizeSrc)
{
  _ASSERTE((type & HUB_MSG_UNION_TYPES_MASK) == HUB_MSG_UNION_TYPE_BUF);

  if (!pMsg)
    return FALSE;

  if ((pMsg->type & HUB_MSG_UNION_TYPES_MASK) != HUB_MSG_UNION_TYPE_BUF)
    ((HubMsg *)pMsg)->Clean();

  BufAppend(&pMsg->u.buf.pBuf, 0, pSrc, sizeSrc);

  if (!pMsg->u.buf.pBuf && sizeSrc) {
    ((HubMsg *)pMsg)->Clean();
    return FALSE;
  }

  pMsg->u.buf.size = sizeSrc;
  pMsg->type = type;

  return TRUE;
}
///////////////////////////////////////////////////////////////
static HUB_MSG *CALLBACK msg_insert_buf(HUB_MSG *pPrevMsg, DWORD type, const BYTE *pSrc, DWORD sizeSrc)
{
  _ASSERTE((type & HUB_MSG_UNION_TYPES_MASK) == HUB_MSG_UNION_TYPE_BUF);

  if (pPrevMsg && pPrevMsg->type == type) {
    BufAppend(&pPrevMsg->u.buf.pBuf, pPrevMsg->u.buf.size, pSrc, sizeSrc);
    pPrevMsg->u.buf.size += sizeSrc;
    return pPrevMsg;
  }

  HubMsg *pMsg = new HubMsg();

  if (!pMsg) {
    cerr << "No enough memory." << endl;
    return NULL;
  }

  if (sizeSrc) {
    BufAppend(&pMsg->u.buf.pBuf, 0, pSrc, sizeSrc);

    if (!pMsg->u.buf.pBuf) {
      delete pMsg;
      return NULL;
    }
  }

  pMsg->u.buf.size = sizeSrc;
  pMsg->type = type;

  if (pPrevMsg)
    pMsg->Insert((HubMsg *)pPrevMsg);

  return pMsg;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK msg_replace_val(HUB_MSG *pMsg, DWORD type, DWORD val)
{
  _ASSERTE((type & HUB_MSG_UNION_TYPES_MASK) == HUB_MSG_UNION_TYPE_VAL);

  if (!pMsg)
    return FALSE;

  ((HubMsg *)pMsg)->Clean();

  pMsg->u.val = val;
  pMsg->type = type;

  return TRUE;
}
///////////////////////////////////////////////////////////////
static HUB_MSG *CALLBACK msg_insert_val(HUB_MSG *pPrevMsg, DWORD type, DWORD val)
{
  _ASSERTE((type & HUB_MSG_UNION_TYPES_MASK) == HUB_MSG_UNION_TYPE_VAL);

  HubMsg *pMsg = new HubMsg();

  if (!pMsg) {
    cerr << "No enough memory." << endl;
    return NULL;
  }

  pMsg->u.val = val;
  pMsg->type = type;

  if (pPrevMsg)
    pMsg->Insert((HubMsg *)pPrevMsg);

  return pMsg;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK msg_replace_none(HUB_MSG *pMsg, DWORD type)
{
  _ASSERTE((type & HUB_MSG_UNION_TYPES_MASK) == HUB_MSG_UNION_TYPE_NONE);

  if (!pMsg)
    return FALSE;

  ((HubMsg *)pMsg)->Clean();

  pMsg->type = type;

  return TRUE;
}
///////////////////////////////////////////////////////////////
static HUB_MSG *CALLBACK msg_insert_none(HUB_MSG *pPrevMsg, DWORD type)
{
  _ASSERTE((type & HUB_MSG_UNION_TYPES_MASK) == HUB_MSG_UNION_TYPE_NONE);

  HubMsg *pMsg = new HubMsg();

  if (!pMsg) {
    cerr << "No enough memory." << endl;
    return NULL;
  }

  pMsg->type = type;

  if (pPrevMsg)
    pMsg->Insert((HubMsg *)pPrevMsg);

  return pMsg;
}
///////////////////////////////////////////////////////////////
static const char * CALLBACK port_name(HMASTERPORT hMasterPort)
{
  _ASSERTE(hMasterPort != NULL);
  _ASSERTE(((Port *)hMasterPort)->IsValid());

  return ((Port *)hMasterPort)->Name().c_str();
}
///////////////////////////////////////////////////////////////
static const char * CALLBACK filter_name(HMASTERFILTER hMasterFilter)
{
  _ASSERTE(hMasterFilter != NULL);
  _ASSERTE(((Filter *)hMasterFilter)->IsValid());

  return ((Filter *)hMasterFilter)->Name().c_str();
}
///////////////////////////////////////////////////////////////
static void CALLBACK on_read(HMASTERPORT hMasterPort, HUB_MSG *pMsg)
{
  _ASSERTE(hMasterPort != NULL);
  _ASSERTE(((Port *)hMasterPort)->IsValid());

  HubMsg msg;

  *(HUB_MSG *)&msg = *pMsg;
  ::memset(pMsg, 0, sizeof(*pMsg));

  ((Port *)hMasterPort)->hub.OnRead((Port *)hMasterPort, &msg);
}
///////////////////////////////////////////////////////////////
static HMASTERTIMER CALLBACK timer_create()
{
  Timer *pTimer = new Timer();

  if (!pTimer)
    cerr << "No enough memory." << endl;

  return (HMASTERTIMER)pTimer;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK timer_set(
  HMASTERTIMER hMasterTimer,
  HMASTERPORT hMasterPort,
  const LARGE_INTEGER *pDueTime,
  LONG period)
{
  _ASSERTE(hMasterTimer != NULL);
  _ASSERTE(((Timer *)hMasterTimer)->IsValid());
  _ASSERTE(hMasterPort != NULL);
  _ASSERTE(((Port *)hMasterPort)->IsValid());

  return ((Timer *)hMasterTimer)->Set((Port *)hMasterPort, pDueTime, period);
}
///////////////////////////////////////////////////////////////
static void CALLBACK timer_cancel(HMASTERTIMER hMasterTimer)
{
  _ASSERTE(hMasterTimer != NULL);
  _ASSERTE(((Timer *)hMasterTimer)->IsValid());

  ((Timer *)hMasterTimer)->Cancel();
}
///////////////////////////////////////////////////////////////
static void CALLBACK timer_delete(HMASTERTIMER hMasterTimer)
{
  _ASSERTE(hMasterTimer != NULL);
  _ASSERTE(((Timer *)hMasterTimer)->IsValid());

  delete ((Timer *)hMasterTimer);
}
///////////////////////////////////////////////////////////////
static HMASTERPORT CALLBACK filter_port(HMASTERFILTERINSTANCE hMasterFilterInstance)
{
  _ASSERTE(hMasterFilterInstance != NULL);
  _ASSERTE(((FilterInstance *)hMasterFilterInstance)->IsValid());

  return (HMASTERPORT)&((FilterInstance *)hMasterFilterInstance)->port;
}
///////////////////////////////////////////////////////////////
HUB_ROUTINES_A hubRoutines = {
  sizeof(HUB_ROUTINES_A),
  buf_alloc,
  buf_free,
  buf_append,
  msg_replace_buf,
  msg_insert_buf,
  msg_replace_val,
  msg_insert_val,
  msg_replace_none,
  msg_insert_none,
  port_name,
  filter_name,
  on_read,
  timer_create,
  timer_set,
  timer_cancel,
  timer_delete,
  filter_port,
};
///////////////////////////////////////////////////////////////
