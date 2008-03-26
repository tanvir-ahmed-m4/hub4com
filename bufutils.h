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
 * Revision 1.1  2008/03/26 08:34:39  vfrolov
 * Initial revision
 *
 *
 */

#ifndef _BUFUTILS_H_
#define _BUFUTILS_H_

///////////////////////////////////////////////////////////////
#define BUF_SIGNATURE 'h4cB'
///////////////////////////////////////////////////////////////
inline BYTE *BufAlloc(DWORD size)
{
  if (!size)
    return NULL;

  size += 16;

  BYTE *pBuf = new BYTE[size
#ifdef _DEBUG
                        + sizeof(DWORD)
#endif
                        + sizeof(DWORD)];

  if (!pBuf)
    return NULL;

#ifdef _DEBUG
  *(DWORD *)pBuf = BUF_SIGNATURE;
  pBuf += sizeof(DWORD);
#endif

  *(DWORD *)pBuf = size;
  pBuf += sizeof(DWORD);

  return pBuf;
}
///////////////////////////////////////////////////////////////
inline VOID BufFree(BYTE *pBuf)
{
  if (pBuf) {
    _ASSERTE(*(DWORD *)(pBuf - sizeof(DWORD) - sizeof(DWORD)) == BUF_SIGNATURE);

#ifdef _DEBUG
    *(DWORD *)(pBuf - sizeof(DWORD) - sizeof(DWORD)) = 0;
#endif

    delete [] (pBuf
#ifdef _DEBUG
               - sizeof(DWORD)
#endif
               - sizeof(DWORD));
  }
}
///////////////////////////////////////////////////////////////
inline void BufAppend(BYTE **ppBuf, DWORD offset, const BYTE *pSrc, DWORD sizeSrc)
{
  BYTE *pBuf = *ppBuf;

  _ASSERTE(!pBuf || (*(DWORD *)(pBuf - sizeof(DWORD) - sizeof(DWORD)) == BUF_SIGNATURE));

  DWORD sizeOld = pBuf ? *(DWORD *)(pBuf - sizeof(DWORD)) : 0;
  DWORD sizeNew = offset + sizeSrc;

  if (sizeOld < sizeNew) {
    *ppBuf = BufAlloc(sizeNew);

    if (sizeOld > offset)
      sizeOld = offset;

    if (sizeOld && *ppBuf)
      memcpy(*ppBuf, pBuf, sizeOld);

    BufFree(pBuf);
    pBuf = *ppBuf;
  }

  if (sizeSrc && pSrc && pBuf)
    memcpy(pBuf + offset, pSrc, sizeSrc);
}
///////////////////////////////////////////////////////////////

#endif /* _BUFUTILS_H_ */
