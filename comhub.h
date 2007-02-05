/*
 * $Id$
 *
 * Copyright (c) 2006-2007 Vyacheslav Frolov
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
 * Revision 1.3  2007/02/05 09:33:20  vfrolov
 * Implemented internal flow control
 *
 * Revision 1.2  2007/02/01 12:14:58  vfrolov
 * Redesigned COM port params
 *
 * Revision 1.1  2007/01/23 09:13:10  vfrolov
 * Initial revision
 *
 *
 */

#ifndef _COMHUB_H
#define _COMHUB_H

///////////////////////////////////////////////////////////////
class ComPort;
class ComParams;
///////////////////////////////////////////////////////////////
typedef vector<ComPort*> ComPorts;
typedef multimap<ComPort*, ComPort*> ComPortMap;
///////////////////////////////////////////////////////////////
class ComHub
{
  public:
    ComHub() {}

    BOOL Add(const char *pPath);
    BOOL PlugIn(int n, const char *pPath, const ComParams &comParams);
    BOOL StartAll();
    void Write(ComPort *pFromPort, LPCVOID pData, DWORD len);
    void AddXoff(ComPort *pFromPort, int count);
    void LostReport() const;

    void RouteData(int iFrom, int iTo, BOOL noRoute) {
      Route(routeDataMap, iFrom, iTo, noRoute);
    }

    void RouteFlowControl(int iFrom, int iTo, BOOL noRoute) {
      Route(routeFlowControlMap, iFrom, iTo, noRoute);
    }

    void RouteFlowControl(BOOL fromAnyDataReceiver);
    void RouteReport() const;
    int NumPorts() const { return (int)ports.size(); }

  private:
    void Route(ComPortMap &map, int iFrom, int iTo, BOOL noRoute) const;

    ComPorts ports;
    ComPortMap routeDataMap;
    ComPortMap routeFlowControlMap;
};
///////////////////////////////////////////////////////////////

#endif  // _COMHUB_H
