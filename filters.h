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
 * Revision 1.1  2008/03/26 08:35:32  vfrolov
 * Initial revision
 *
 *
 */

#ifndef _FILTERS_H
#define _FILTERS_H

#include "plugins/plugins_api.h"

///////////////////////////////////////////////////////////////
class ComHub;
class Filter;
class FilterMethod;
class HubMsg;
///////////////////////////////////////////////////////////////
typedef vector<Filter*> FilterArray;
typedef vector<FilterMethod*> FilterMethodArray;
typedef map<int, FilterMethodArray*> PortFiltersMap;
///////////////////////////////////////////////////////////////
class Filters
{
  public:
    Filters(const ComHub &_hub) : hub(_hub) {}
    ~Filters();
    BOOL CreateFilter(
        const FILTER_ROUTINES_A *pFltRoutines,
        const char *pFilterName,
        HCONFIG hConfig,
        const char *pArgs);
    BOOL AddFilter(
        int iPort,
        const char *pName,
        BOOL isInMethod);
    void Report() const;
    BOOL Init() const;
    BOOL InMethod(
        int nFromPort,
        HubMsg *pInMsg,
        HubMsg **ppEchoMsg) const;
    BOOL OutMethod(
        int nFromPort,
        int nToPort,
        HubMsg *pOutMsg) const;

  private:
    const ComHub &hub;
    FilterArray allFilters;
    PortFiltersMap portFilters;
};
///////////////////////////////////////////////////////////////

#endif  // _FILTERS_H
