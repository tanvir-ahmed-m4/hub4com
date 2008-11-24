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
 * Revision 1.2  2008/11/24 12:36:59  vfrolov
 * Changed plugin API
 *
 * Revision 1.1  2008/03/26 08:37:06  vfrolov
 * Initial revision
 *
 */

#ifndef _ROUTE_H
#define _ROUTE_H

///////////////////////////////////////////////////////////////
class Port;
///////////////////////////////////////////////////////////////
typedef multimap<Port *, Port *> PortMap;
///////////////////////////////////////////////////////////////
void AddRoute(
    PortMap &map,
    Port *pFrom,
    Port *pTo,
    BOOL noRoute,
    BOOL noEcho);
void SetFlowControlRoute(
    PortMap &routeFlowControlMap,
    PortMap &routeDataMap,
    BOOL fromAnyDataReceiver);
///////////////////////////////////////////////////////////////

#endif  // _ROUTE_H
