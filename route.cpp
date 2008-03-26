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
 * Revision 1.1  2008/03/26 08:37:06  vfrolov
 * Initial revision
 *
 *
 */

#include "precomp.h"
#include "route.h"

///////////////////////////////////////////////////////////////
typedef pair<int, int> PortPair;
///////////////////////////////////////////////////////////////
static PortNumMap::iterator FindPair(PortNumMap &map, const PortPair &pair)
{
  PortNumMap::iterator i;

  for (i = map.find(pair.first) ; i != map.end() ; i++) {
    if (i->first != pair.first)
      return map.end();

    if (i->second == pair.second)
      break;
  }

  return i;
}

void AddRoute(
    PortNumMap &map,
    int iFrom,
    int iTo,
    BOOL noRoute,
    BOOL noEcho)
{
  if (iFrom != iTo || !noEcho || noRoute) {
    PortPair pair(iFrom, iTo);

    for (;;) {
      PortNumMap::iterator i = FindPair(map, pair);

      if (i == map.end()) {
        if (!noRoute)
          map.insert(pair);
        break;
      } else if (noRoute) {
        map.erase(i);
      } else {
        break;
      }
    }
  }
}

void SetFlowControlRoute(
    PortNumMap &routeFlowControlMap,
    PortNumMap &routeDataMap,
    BOOL fromAnyDataReceiver)
{
  routeFlowControlMap.clear();

  for (PortNumMap::const_iterator i = routeDataMap.begin() ; i != routeDataMap.end() ; i++) {
    PortPair pair(i->second, i->first);

    if (FindPair(routeFlowControlMap, pair) == routeFlowControlMap.end()) {
      if (fromAnyDataReceiver || FindPair(routeDataMap, pair) != routeDataMap.end())
        routeFlowControlMap.insert(pair);
    }
  }
}
///////////////////////////////////////////////////////////////
