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
 * Revision 1.2  2009/02/02 15:21:42  vfrolov
 * Optimized filter's API
 *
 * Revision 1.1  2008/11/24 11:46:56  vfrolov
 * Initial revision
 *
 */

#ifndef _FILTER_H
#define _FILTER_H

///////////////////////////////////////////////////////////////
#define FILTER_SIGNATURE 'h4cF'
///////////////////////////////////////////////////////////////
class Filter
{
  public:
    Filter(
        const char *pGroup,
        const char *pName,
        FILTER_CREATE_INSTANCE *_pCreateInstance,
        FILTER_IN_METHOD *_pInMethod,
        FILTER_OUT_METHOD *_pOutMethod)
      : group(pGroup),
        name(pName),
        pCreateInstance(_pCreateInstance),
        pInMethod(_pInMethod),
        pOutMethod(_pOutMethod),
        hFilter(NULL)
    {
#ifdef _DEBUG
      signature = FILTER_SIGNATURE;
#endif
    }

#ifdef _DEBUG
    ~Filter() {
      _ASSERTE(signature == FILTER_SIGNATURE);
      signature = 0;
    }
#endif

    const string &Name() const { return name; }

  protected:
    friend class Filters;
    friend class FilterInstance;

    const string group;
    const string name;
    FILTER_CREATE_INSTANCE *const pCreateInstance;
    FILTER_IN_METHOD *const pInMethod;
    FILTER_OUT_METHOD *const pOutMethod;

    HFILTER hFilter;

#ifdef _DEBUG
  private:
    DWORD signature;

  public:
    BOOL IsValid() { return signature == FILTER_SIGNATURE; }
#endif
};
///////////////////////////////////////////////////////////////
#define FILTER_INSTANCE_SIGNATURE 'h4cI'
///////////////////////////////////////////////////////////////
class FilterInstance {
  public:
    FilterInstance(
        Filter &_filter,
        Port &_port,
        BOOL addInMethod,
        BOOL addOutMethod,
        const set<Port *> *_pSrcPorts)
      : filter(_filter),
        port(_port),
        pInMethod(addInMethod ? _filter.pInMethod : NULL),
        pOutMethod(addOutMethod ? _filter.pOutMethod : NULL),
        pSrcPorts(_pSrcPorts),
        hFilterInstance(NULL)
    {
#ifdef _DEBUG
      signature = FILTER_INSTANCE_SIGNATURE;
#endif
    }

    ~FilterInstance() {
      if (pSrcPorts)
        delete pSrcPorts;

      //if (hFilterInstance)
      //  Delete(hFilterInstance);

#ifdef _DEBUG
      _ASSERTE(signature == FILTER_INSTANCE_SIGNATURE);
      signature = 0;
#endif
    }

    Filter &filter;
    Port &port;

  protected:
    friend class Filters;

    FILTER_IN_METHOD *const pInMethod;
    FILTER_OUT_METHOD *const pOutMethod;
    const set<Port *> *const pSrcPorts;

    HFILTERINSTANCE hFilterInstance;

#ifdef _DEBUG
  private:
    DWORD signature;

  public:
    BOOL IsValid() { return signature == FILTER_INSTANCE_SIGNATURE; }
#endif
};
///////////////////////////////////////////////////////////////

#endif  // _FILTER_H
