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

#include "precomp.h"
#include "../plugins_api.h"
///////////////////////////////////////////////////////////////
namespace PortConnector {
///////////////////////////////////////////////////////////////
#include "comport.h"
#include "import.h"
///////////////////////////////////////////////////////////////
ComPort::ComPort(const char *pPath)
  : hMasterPort(NULL),
    name(pPath),
    countConnections(0),
    countXoff(0)
{
}

BOOL ComPort::Init(HMASTERPORT _hMasterPort)
{
  hMasterPort = _hMasterPort;

  return TRUE;
}

BOOL ComPort::FakeReadFilter(HUB_MSG *pInMsg)
{
  _ASSERTE(pInMsg != NULL);

  switch (pInMsg->type) {
    case HUB_MSG_TYPE_LOOP_TEST:
      pInMsg->u.hVal = (HANDLE)hMasterPort;
      break;
  }

  return pInMsg != NULL;
}

BOOL ComPort::Write(HUB_MSG *pMsg)
{
  _ASSERTE(pMsg != NULL);

  switch (pMsg->type) {
    case HUB_MSG_TYPE_LOOP_TEST:
      for (Ports::const_iterator i = connectedDataPorts.begin() ; i != connectedDataPorts.end() ; i++) {
        if (pMsg->u.hVal == (HANDLE)(*i)->hMasterPort) {
          cerr << "Loop detected on link " << Name() << "->" << (*i)->Name() << endl;
          exit(1);
        }
      }

      break;
    case HUB_MSG_TYPE_LINE_DATA:
      if (!pMsg->u.buf.size)
        break;

      for (Ports::const_iterator i = connectedDataPorts.begin() ; i != connectedDataPorts.end() ; i++) {
        BYTE *pBuf = pBufAlloc(pMsg->u.buf.size);

        if (!pBuf)
          return FALSE;

        memcpy(pBuf, pMsg->u.buf.pBuf, pMsg->u.buf.size);

        HUB_MSG msg;

        msg.type = HUB_MSG_TYPE_LINE_DATA;
        msg.u.buf.pBuf = pBuf;
        msg.u.buf.size = pMsg->u.buf.size;

        pOnRead((*i)->hMasterPort, &msg);
      }

      break;
    case HUB_MSG_TYPE_CONNECT:
      if (pMsg->u.val) {
        if (countConnections++ != 0)
          break;
      } else {
        if (--countConnections != 0)
          break;
      }

      for (Ports::const_iterator i = connectedDataPorts.begin() ; i != connectedDataPorts.end() ; i++) {
        HUB_MSG msg;

        msg.type = HUB_MSG_TYPE_CONNECT;
        msg.u.val = pMsg->u.val;

        pOnRead((*i)->hMasterPort, &msg);
      }

      break;
    case HUB_MSG_TYPE_SET_OUT_OPTS:
      if (pMsg->u.val) {
        cerr << name << " WARNING: Requested output option(s) [0x"
             << hex << pMsg->u.val << dec
             << "] will be ignored by driver" << endl;
      }
      break;
    case HUB_MSG_TYPE_ADD_XOFF_XON:
      if (pMsg->u.val) {
        if (countXoff++ != 0)
          break;

        for (Ports::const_iterator i = connectedFlowControlPorts.begin() ; i != connectedFlowControlPorts.end() ; i++) {
          HUB_MSG msg;

          msg.type = HUB_MSG_TYPE_ADD_XOFF_XON;
          msg.u.val = TRUE;

          pOnRead((*i)->hMasterPort, &msg);
        }
      } else {
        if (--countXoff != 0)
          break;

        for (Ports::const_iterator i = connectedFlowControlPorts.begin() ; i != connectedFlowControlPorts.end() ; i++) {
          HUB_MSG msg;

          msg.type = HUB_MSG_TYPE_ADD_XOFF_XON;
          msg.u.val = FALSE;

          pOnRead((*i)->hMasterPort, &msg);
        }
      }
      break;
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
