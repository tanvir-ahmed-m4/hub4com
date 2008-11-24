/*
 * $Id$
 *
 * Copyright (c) 2006-2008 Vyacheslav Frolov
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
 * Revision 1.9  2008/11/24 12:36:59  vfrolov
 * Changed plugin API
 *
 * Revision 1.8  2008/11/13 08:07:40  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.7  2008/08/20 08:46:06  vfrolov
 * Implemented ComHub::FilterName()
 *
 * Revision 1.6  2008/08/15 12:44:59  vfrolov
 * Added fake read filter method to ports
 *
 * Revision 1.5  2008/03/26 08:48:18  vfrolov
 * Initial revision
 *
 * Revision 1.4  2007/12/19 13:46:36  vfrolov
 * Added ability to send data received from port to the same port
 *
 * Revision 1.3  2007/02/05 09:33:20  vfrolov
 * Implemented internal flow control
 *
 * Revision 1.2  2007/02/01 12:14:58  vfrolov
 * Redesigned COM port params
 *
 * Revision 1.1  2007/01/23 09:13:10  vfrolov
 * Initial revision
 *
 */

#ifndef _COMHUB_H
#define _COMHUB_H

///////////////////////////////////////////////////////////////
class Port;
class Filters;
class HubMsg;
///////////////////////////////////////////////////////////////
typedef vector<Port*> Ports;
typedef multimap<Port*, Port*> PortMap;
///////////////////////////////////////////////////////////////
#define HUB_SIGNATURE 'h4cH'
///////////////////////////////////////////////////////////////
class ComHub
{
  public:
    ComHub() : pFilters(NULL) {
#ifdef _DEBUG
      signature = HUB_SIGNATURE;
#endif
    }

#ifdef _DEBUG
    ~ComHub() {
      _ASSERTE(signature == HUB_SIGNATURE);
      signature = 0;
    }
#endif

    void Add();
    BOOL InitPort(
        int n,
        const PORT_ROUTINES_A *pPortRoutines,
        HCONFIG hConfig,
        const char *pPath);
    BOOL StartAll() const;
    BOOL OnFakeRead(Port *pFromPort, HubMsg *pMsg) const;
    void OnRead(Port *pFromPort, HubMsg *pMsg) const;
    void AddXoffXon(Port *pFromPort, BOOL xoff) const;
    void LostReport() const;
    void SetDataRoute(const PortMap &map) { routeDataMap = map; }
    void SetFlowControlRoute(const PortMap &map) { routeFlowControlMap = map; }
    void RouteReport() const;
    int NumPorts() const { return (int)ports.size(); }

    Filters *SetFilters(Filters *_pFilters) {
      Filters *pFiltersOld = pFilters;
      pFilters = _pFilters;
      return pFiltersOld;
    }

    Port *ComHub::GetPort(int n) const {
      _ASSERTE(n >= 0 && n < NumPorts());
      return ports.at(n);
    }

    const char *FilterName(HFILTER hFilter) const;

  private:
    void OnRead(const PortMap &routeMap, Port *pFromPort, HubMsg *pMsg) const;

    Ports ports;
    PortMap routeDataMap;
    PortMap routeFlowControlMap;

    Filters *pFilters;

#ifdef _DEBUG
  private:
    DWORD signature;

  public:
    BOOL IsValid() { return signature == HUB_SIGNATURE; }
#endif
};
///////////////////////////////////////////////////////////////

#endif  // _COMHUB_H
