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
 * Revision 1.1  2008/03/26 08:35:03  vfrolov
 * Initial revision
 *
 *
 */

#include "precomp.h"
#include "export.h"
#include "port.h"
#include "comhub.h"
#include "bufutils.h"
#include "hubmsg.h"

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
static HUB_MSG *CALLBACK msg_replace_buf(HUB_MSG *pMsg, WORD type, const BYTE *pSrc, DWORD sizeSrc)
{
  _ASSERTE((type & HUB_MSG_UNION_TYPE_MASK) == HUB_MSG_UNION_TYPE_BUF);
  _ASSERTE(pMsg != NULL);

  if ((pMsg->type & HUB_MSG_UNION_TYPE_MASK) != HUB_MSG_UNION_TYPE_BUF)
    ((HubMsg *)pMsg)->Clean();

  BufAppend(&pMsg->u.buf.pBuf, 0, pSrc, sizeSrc);

  if (!pMsg->u.buf.pBuf && sizeSrc) {
    ((HubMsg *)pMsg)->Clean();
    return NULL;
  }

  pMsg->u.buf.size = sizeSrc;
  pMsg->type = type;

  return pMsg;
}
///////////////////////////////////////////////////////////////
static HUB_MSG *CALLBACK msg_insert_buf(HUB_MSG *pPrevMsg, WORD type, const BYTE *pSrc, DWORD sizeSrc)
{
  _ASSERTE((type & HUB_MSG_UNION_TYPE_MASK) == HUB_MSG_UNION_TYPE_BUF);

  if (pPrevMsg && pPrevMsg->type == type) {
    BufAppend(&pPrevMsg->u.buf.pBuf, pPrevMsg->u.buf.size, pSrc, sizeSrc);
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
static HUB_MSG *CALLBACK msg_replace_val(HUB_MSG *pMsg, WORD type, DWORD val)
{
  _ASSERTE((type & HUB_MSG_UNION_TYPE_MASK) == HUB_MSG_UNION_TYPE_VAL);
  _ASSERTE(pMsg != NULL);

  ((HubMsg *)pMsg)->Clean();

  pMsg->u.val = val;
  pMsg->type = type;

  return pMsg;
}
///////////////////////////////////////////////////////////////
static HUB_MSG *CALLBACK msg_insert_val(HUB_MSG *pPrevMsg, WORD type, DWORD val)
{
  _ASSERTE((type & HUB_MSG_UNION_TYPE_MASK) == HUB_MSG_UNION_TYPE_VAL);

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
static HUB_MSG *CALLBACK msg_replace_none(HUB_MSG *pMsg, WORD type)
{
  _ASSERTE((type & HUB_MSG_UNION_TYPE_MASK) == HUB_MSG_UNION_TYPE_NONE);
  _ASSERTE(pMsg != NULL);

  ((HubMsg *)pMsg)->Clean();

  pMsg->type = type;

  return pMsg;
}
///////////////////////////////////////////////////////////////
static HUB_MSG *CALLBACK msg_insert_none(HUB_MSG *pPrevMsg, WORD type)
{
  _ASSERTE((type & HUB_MSG_UNION_TYPE_MASK) == HUB_MSG_UNION_TYPE_NONE);

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
static int CALLBACK num_ports(HHUB hHub)
{
  _ASSERTE(hHub != NULL);
  _ASSERTE(((ComHub *)hHub)->IsValid());

  return ((ComHub *)hHub)->NumPorts();
}
///////////////////////////////////////////////////////////////
static const char * CALLBACK port_name(HHUB hHub, int n)
{
  _ASSERTE(hHub != NULL);
  _ASSERTE(((ComHub *)hHub)->IsValid());

  Port *pPort = ((ComHub *)hHub)->GetPort(n);

  if (!pPort)
    return NULL;

  return pPort->Name().c_str();
}
///////////////////////////////////////////////////////////////
static void CALLBACK on_xoff(HHUB hHub, HMASTERPORT hMasterPort)
{
  _ASSERTE(hHub != NULL);
  _ASSERTE(((ComHub *)hHub)->IsValid());
  _ASSERTE(hMasterPort != NULL);
  _ASSERTE(((Port *)hMasterPort)->IsValid());

  ((ComHub *)hHub)->AddXoff((Port *)hMasterPort);
}
///////////////////////////////////////////////////////////////
static void CALLBACK on_xon(HHUB hHub, HMASTERPORT hMasterPort)
{
  _ASSERTE(hHub != NULL);
  _ASSERTE(((ComHub *)hHub)->IsValid());
  _ASSERTE(hMasterPort != NULL);
  _ASSERTE(((Port *)hMasterPort)->IsValid());

  ((ComHub *)hHub)->AddXon((Port *)hMasterPort);
}
///////////////////////////////////////////////////////////////
static void CALLBACK on_read(HHUB hHub, HMASTERPORT hMasterPort, HUB_MSG *pMsg)
{
  _ASSERTE(hHub != NULL);
  _ASSERTE(((ComHub *)hHub)->IsValid());
  _ASSERTE(hMasterPort != NULL);
  _ASSERTE(((Port *)hMasterPort)->IsValid());

  HubMsg msg;

  *(HUB_MSG *)&msg = *pMsg;
  ::memset(pMsg, 0, sizeof(*pMsg));

  ((ComHub *)hHub)->OnRead((Port *)hMasterPort, &msg);
}
///////////////////////////////////////////////////////////////
HUB_ROUTINES_A hubRoutines = {
  sizeof(HUB_ROUTINES_A),
  buf_alloc,
  buf_free,
  msg_replace_buf,
  msg_insert_buf,
  msg_replace_val,
  msg_insert_val,
  msg_replace_none,
  msg_insert_none,
  num_ports,
  port_name,
  on_xoff,
  on_xon,
  on_read,
};
///////////////////////////////////////////////////////////////
