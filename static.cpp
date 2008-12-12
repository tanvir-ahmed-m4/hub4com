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
 * Revision 1.3  2008/12/12 08:04:48  vfrolov
 * Simplified adding new plugins
 *
 * Revision 1.2  2008/12/11 13:25:20  vfrolov
 * Added FilterCrypt, FilterPurge, FilterTag, PortConnector
 *
 * Revision 1.1  2008/11/13 08:03:24  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "plugins/plugins_api.h"

#include "static.h"

///////////////////////////////////////////////////////////////
#ifdef USE_STATIC_PLUGINS
  #define INIT_DECLARE(ns) namespace ns {PLUGIN_INIT_A InitA;}
  #define INIT_INSERT(ns) ns::InitA,
#else
  #define INIT_DECLARE(ns)
  #define INIT_INSERT(ns)
#endif
///////////////////////////////////////////////////////////////
#define NAMESPACES(pattern)     \
  pattern(FilterAwakSeq)        \
  pattern(FilterCrypt)          \
  pattern(FilterEcho)           \
  pattern(FilterEscInsert)      \
  pattern(FilterEscParse)       \
  pattern(FilterLineCtl)        \
  pattern(FilterLsrMap)         \
  pattern(FilterPin2Con)        \
  pattern(FilterPinMap)         \
  pattern(FilterPurge)          \
  pattern(FilterTag)            \
  pattern(FilterTelnet)         \
  pattern(FilterTrace)          \
  pattern(PortConnector)        \
  pattern(PortSerial)           \
  pattern(PortTcp)              \
///////////////////////////////////////////////////////////////
NAMESPACES(INIT_DECLARE)
///////////////////////////////////////////////////////////////
static PLUGIN_INIT_A *const list[] = {
  NAMESPACES(INIT_INSERT)
  NULL,
};
///////////////////////////////////////////////////////////////
PLUGIN_INIT_A *const *GetStaticInitList()
{
  return list;
}
///////////////////////////////////////////////////////////////
