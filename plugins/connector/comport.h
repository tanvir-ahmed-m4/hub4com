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
 * Revision 1.1  2008/11/27 16:38:05  vfrolov
 * Initial revision
 *
 */

#ifndef _COMPORT_H
#define _COMPORT_H

///////////////////////////////////////////////////////////////
class ComPort;
typedef set<ComPort *> Ports;
///////////////////////////////////////////////////////////////
class ComPort
{
  public:
    ComPort(const char *pPath);

    BOOL Init(HMASTERPORT _hMasterPort);
    BOOL FakeReadFilter(HUB_MSG *pInMsg);
    BOOL Write(HUB_MSG *pMsg);

    void ConnectDataPort(ComPort *pPort) { connectedDataPorts.insert(pPort); }
    void ConnectFlowControlPort(ComPort *pPort) { connectedFlowControlPorts.insert(pPort); }
    const string &Name() const { return name; }

  private:
    string name;
    HMASTERPORT hMasterPort;

    Ports connectedDataPorts;
    Ports connectedFlowControlPorts;

    int countConnections;
    int countXoff;
};
///////////////////////////////////////////////////////////////

#endif  // _COMPORT_H
