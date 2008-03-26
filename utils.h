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
 * Revision 1.2  2008/03/26 08:14:09  vfrolov
 * Added
 *   - class Args
 *   - STRQTOK_R()
 *   - CreateArgsVector()/FreeArgsVector()
 *
 * Revision 1.1  2007/01/23 09:13:10  vfrolov
 * Initial revision
 *
 */

#ifndef _UTILS_H_
#define _UTILS_H_

///////////////////////////////////////////////////////////////
class Args : public vector<string>
{
  public:
    Args(int argc, const char *const argv[]);

  private:
    void Add(const string &arg, const vector<string> &params);

    int num_recursive;
};
///////////////////////////////////////////////////////////////
char *STRTOK_R(char *pStr, const char *pDelims, char **ppSave);
char *STRQTOK_R(char *pStr, const char *pDelims, char **ppSave);
BOOL StrToInt(const char *pStr, int *pNum);
const char *GetParam(const char *pArg, const char *pPattern);
void CreateArgsVector(
    const char *pName,
    const char *pArgs,
    int *pArgc,
    const char ***pArgv,
    void **ppTmp);
void FreeArgsVector(
    const char **argv,
    void *pTmp);
///////////////////////////////////////////////////////////////

#endif /* _UTILS_H_ */
