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
 * Revision 1.3  2008/12/01 17:14:52  vfrolov
 * Implemented --fc-route and --no-default-fc-route options
 *
 * Revision 1.2  2008/11/24 12:36:59  vfrolov
 * Changed plugin API
 *
 * Revision 1.1  2008/03/26 08:37:06  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "route.h"

///////////////////////////////////////////////////////////////
typedef pair<Port *, Port *> PortPair;
///////////////////////////////////////////////////////////////
static PortMap::iterator FindPair(PortMap &map, const PortPair &pair)
{
  PortMap::iterator i;

  for (i = map.find(pair.first) ; i != map.end() ; i++) {
    if (i->first != pair.first)
      return map.end();

    if (i->second == pair.second)
      break;
  }

  return i;
}

static void AddRoute(
    PortMap &map,
    const PortPair &pair,
    BOOL noRoute)
{
  for (;;) {
    PortMap::iterator i = FindPair(map, pair);

    if (i == map.end()) {
      if (!noRoute)
        map.insert(pair);
      break;
    }
    else
    if (noRoute) {
      map.erase(i);
    }
    else {
      break;
    }
  }
}

void AddRoute(
    PortMap &map,
    Port *pFrom,
    Port *pTo,
    BOOL noRoute,
    BOOL noEcho)
{
  if (pFrom != pTo || !noEcho || noRoute)
    AddRoute(map, PortPair(pFrom, pTo), noRoute);
}

void AddRoute(
    PortMap &map,
    PortMap &noMap,
    BOOL noRoute)
{
  for (PortMap::const_iterator i = noMap.begin() ; i != noMap.end() ; i++)
    AddRoute(map, *i, noRoute);
}

void SetFlowControlRoute(
    PortMap &routeFlowControlMap,
    PortMap &routeDataMap,
    BOOL fromAnyDataReceiver)
{
  routeFlowControlMap.clear();

  for (PortMap::const_iterator i = routeDataMap.begin() ; i != routeDataMap.end() ; i++) {
    PortPair pair(i->second, i->first);

    if (FindPair(routeFlowControlMap, pair) == routeFlowControlMap.end()) {
      if (fromAnyDataReceiver || FindPair(routeDataMap, pair) != routeDataMap.end())
        routeFlowControlMap.insert(pair);
    }
  }
}
///////////////////////////////////////////////////////////////
