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
 * Revision 1.5  2008/08/25 08:15:02  vfrolov
 * Itilized TimerAPCProc()
 *
 * Revision 1.4  2008/04/16 14:09:41  vfrolov
 * Included <set>
 *
 * Revision 1.3  2008/03/26 08:15:24  vfrolov
 * Added more includes
 *
 * Revision 1.2  2007/02/01 12:14:59  vfrolov
 * Redesigned COM port params
 *
 * Revision 1.1  2007/01/23 09:13:10  vfrolov
 * Initial revision
 *
 */

#ifndef _PRECOMP_H_
#define _PRECOMP_H_

#define _WIN32_WINNT 0x0500

#include <windows.h>
#include <crtdbg.h>

#include <string>
#include <vector>
#include <set>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

#pragma warning(disable:4512) // assignment operator could not be generated

#endif /* _PRECOMP_H_ */
