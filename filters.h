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
 * Revision 1.7  2008/11/24 12:36:59  vfrolov
 * Changed plugin API
 *
 * Revision 1.6  2008/11/13 08:07:40  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.5  2008/10/16 06:19:12  vfrolov
 * Divided filter ID to filter group ID and filter name
 *
 * Revision 1.4  2008/09/26 15:34:50  vfrolov
 * Fixed adding order for filters with the same FID
 *
 * Revision 1.3  2008/08/20 08:32:35  vfrolov
 * Implemented Filters::FilterName()
 *
 * Revision 1.2  2008/04/16 14:13:59  vfrolov
 * Added ability to specify source posts for OUT method
 *
 * Revision 1.1  2008/03/26 08:35:32  vfrolov
 * Initial revision
 *
 */

#ifndef _FILTERS_H
#define _FILTERS_H

///////////////////////////////////////////////////////////////
class ComHub;
class Filter;
class FilterMethod;
class HubMsg;
///////////////////////////////////////////////////////////////
typedef vector<Filter*> FilterArray;
typedef vector<FilterMethod*> FilterMethodArray;
typedef map<Port *, FilterMethodArray*> PortFiltersMap;
typedef set<Port *> SetOfPorts;
///////////////////////////////////////////////////////////////
class Filters
{
  public:
    Filters(const ComHub &_hub) : hub(_hub) {}
    ~Filters();
    BOOL CreateFilter(
        const FILTER_ROUTINES_A *pFltRoutines,
        const char *pFilterGroup,
        const char *pFilterName,
        HCONFIG hConfig,
        const char *pArgs);
    BOOL AddFilter(
        Port *pPort,
        const char *pGroup,
        BOOL addInMethod,
        BOOL addOutMethod,
        const SetOfPorts *pOutMethodSrcPorts);
    void Report() const;
    BOOL InMethod(
        Port *pFromPort,
        HubMsg *pInMsg,
        HubMsg **ppEchoMsg) const;
    BOOL OutMethod(
        Port *pFromPort,
        Port *pToPort,
        HubMsg *pOutMsg) const;

  private:
    const ComHub &hub;
    FilterArray allFilters;
    PortFiltersMap portFilters;
};
///////////////////////////////////////////////////////////////

#endif  // _FILTERS_H
