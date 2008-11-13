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
 * Revision 1.2  2008/11/13 08:07:40  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.1  2008/03/26 08:36:03  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "plugins/plugins_api.h"

#include "hubmsg.h"
#include "bufutils.h"

///////////////////////////////////////////////////////////////
HubMsg::HubMsg()
  : pNext(NULL)
{
#ifdef _DEBUG
  signature = MSG_SIGNATURE;
#endif

  ::memset((HUB_MSG *)this, 0, sizeof(HUB_MSG));
}
///////////////////////////////////////////////////////////////
HubMsg::~HubMsg()
{
  _ASSERTE(signature == MSG_SIGNATURE);

  if (pNext)
    delete pNext;

  Clean();

#ifdef _DEBUG
  signature = 0;
#endif
}
///////////////////////////////////////////////////////////////
HubMsg *HubMsg::Clone() const
{
  _ASSERTE(signature == MSG_SIGNATURE);

  HubMsg *pNewMsg = new HubMsg();

  if (!pNewMsg)
    return NULL;

  if (pNext) {
    pNewMsg->pNext = pNext->Clone();

    if (!pNewMsg->pNext) {
      delete pNewMsg;
      return NULL;
    }
  }

  if ((type & HUB_MSG_UNION_TYPE_MASK) == HUB_MSG_UNION_TYPE_BUF) {
    BufAppend(&pNewMsg->u.buf.pBuf, 0, u.buf.pBuf, u.buf.size);

    if (pNewMsg->u.buf.pBuf || !u.buf.size) {
      pNewMsg->type = type;
      pNewMsg->u.buf.size = u.buf.size;
    } else {
      delete pNewMsg;
      return NULL;
    }
  } else {
    *(HUB_MSG *)pNewMsg = *(const HUB_MSG *)this;
  }

  return pNewMsg;
}
///////////////////////////////////////////////////////////////
void HubMsg::Clean()
{
  _ASSERTE(signature == MSG_SIGNATURE);

  if ((type & HUB_MSG_UNION_TYPE_MASK) == HUB_MSG_UNION_TYPE_BUF)
    BufFree(u.buf.pBuf);

  ::memset((HUB_MSG *)this, 0, sizeof(HUB_MSG));
}
///////////////////////////////////////////////////////////////
void HubMsg::Merge(HubMsg *pMsg)
{
  _ASSERTE(signature == MSG_SIGNATURE);
  _ASSERTE(pMsg == NULL || pMsg->signature == MSG_SIGNATURE);

  HubMsg **ppNext = &pNext;
  while (*ppNext)
    ppNext = &(*ppNext)->pNext;

  *ppNext = pMsg;
}
///////////////////////////////////////////////////////////////
