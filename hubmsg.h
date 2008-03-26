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
 * Revision 1.1  2008/03/26 08:36:03  vfrolov
 * Initial revision
 *
 *
 */

#ifndef _HUBMSG_H
#define _HUBMSG_H

///////////////////////////////////////////////////////////////
#include "plugins/plugins_api.h"
///////////////////////////////////////////////////////////////
#define MSG_SIGNATURE 'h4cM'
///////////////////////////////////////////////////////////////
class HubMsg : public HUB_MSG
{
  public:
    HubMsg();
    ~HubMsg();

    void Clean();
    void Merge(HubMsg *pMsg);
    HubMsg *Clone() const;

    void Insert(HubMsg *pPrevMsg) {
      _ASSERTE(signature == MSG_SIGNATURE);
      _ASSERTE(pPrevMsg->signature == MSG_SIGNATURE);

      pNext = pPrevMsg->pNext;
      pPrevMsg->pNext = this;
    }

    HubMsg *Next() {
      _ASSERTE(signature == MSG_SIGNATURE);

      return pNext;
    }

  private:
    HubMsg *pNext;

#ifdef _DEBUG
    DWORD signature;
#endif
};
///////////////////////////////////////////////////////////////

#endif /* _HUBMSG_H */
