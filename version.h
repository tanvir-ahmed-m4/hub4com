/*
 * $Id$
 *
 * Copyright (c) 2006-2011 Vyacheslav Frolov
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
 */

#ifndef _H4C_VERSION_H_
#define _H4C_VERSION_H_

#define H4C_COPYRIGHT_YEARS "2006-2011"

#define H4C_V1 2
#define H4C_V2 0
#define H4C_V3 1
#define H4C_V4 0

#define MK_VERSION_STR1(V1, V2, V3, V4) #V1 "." #V2 "." #V3 "." #V4
#define MK_VERSION_STR(V1, V2, V3, V4) MK_VERSION_STR1(V1, V2, V3, V4)

#define H4C_VERSION_STR MK_VERSION_STR(H4C_V1, H4C_V2, H4C_V3, H4C_V4)

#endif /* _H4C_VERSION_H_ */
