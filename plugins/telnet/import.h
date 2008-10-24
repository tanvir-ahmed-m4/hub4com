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
 * Revision 1.2  2008/10/24 08:29:01  vfrolov
 * Implemented RFC 2217
 *
 * Revision 1.1  2008/10/09 11:02:58  vfrolov
 * Redesigned class TelnetProtocol
 *
 */

#ifndef _IMPORT_H
#define _IMPORT_H

#include "../plugins_api.h"

///////////////////////////////////////////////////////////////
extern ROUTINE_MSG_INSERT_VAL *pMsgInsertVal;
extern ROUTINE_MSG_REPLACE_BUF *pMsgReplaceBuf;
extern ROUTINE_MSG_INSERT_BUF *pMsgInsertBuf;
extern ROUTINE_MSG_REPLACE_NONE *pMsgReplaceNone;
///////////////////////////////////////////////////////////////

#endif  // _IMPORT_H
