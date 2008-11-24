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
 * Revision 1.4  2008/11/24 12:36:59  vfrolov
 * Changed plugin API
 *
 * Revision 1.3  2008/11/13 08:07:40  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.2  2008/08/15 12:44:59  vfrolov
 * Added fake read filter method to ports
 *
 * Revision 1.1  2008/03/26 08:36:47  vfrolov
 * Initial revision
 *
 */

#ifndef _PORT_H
#define _PORT_H

///////////////////////////////////////////////////////////////
class ComHub;
class HubMsg;
///////////////////////////////////////////////////////////////
#define PORT_SIGNATURE 'h4cP'
///////////////////////////////////////////////////////////////
class Port
{
  public:
    Port(ComHub &_hub, int _num);

#ifdef _DEBUG
    ~Port() {
      _ASSERTE(signature == PORT_SIGNATURE);
      signature = 0;
    }
#endif

    BOOL Init(
        const PORT_ROUTINES_A *pPortRoutines,
        HCONFIG hConfig,
        const char *pPath);
    BOOL Start();
    BOOL FakeReadFilter(HubMsg *pMsg);
    BOOL Write(HubMsg *pMsg);
    const string &Name() const { return name; }
    int Num() const { return num; }
    void LostReport();

  public:
    ComHub &hub;

  private:
    int num;
    string name;
    HPORT hPort;

    PORT_START *pStart;
    PORT_FAKE_READ_FILTER *pFakeReadFilter;
    PORT_WRITE *pWrite;
    PORT_LOST_REPORT *pLostReport;

#ifdef _DEBUG
    DWORD signature;

  public:
    BOOL IsValid() { return signature == PORT_SIGNATURE; }
#endif
};
///////////////////////////////////////////////////////////////

#endif  // _PORT_H
