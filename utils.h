/*
 * $Id$
 *
 * Copyright (c) 2006-2009 Vyacheslav Frolov
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
 * Revision 1.6  2009/02/04 12:26:54  vfrolov
 * Implemented --load option for filters
 *
 * Revision 1.5  2008/10/02 07:52:38  vfrolov
 * Added removing macroses for undefined parameters of --load option
 *
 * Revision 1.4  2008/08/28 15:53:13  vfrolov
 * Added ability to load arguments from standard input and
 * to select fragment for loading
 *
 * Revision 1.3  2008/04/16 14:07:12  vfrolov
 * Extended STRQTOK_R()
 *
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
    Args() : num_recursive(0) {}
    Args(int argc, const char *const argv[]);

    void Add(const vector<string> &args);

    static const char *LoadPrefix() { return "--load="; }
    static int RecursiveMax() { return 256; }

  private:
    void Add(const string &arg);

    int num_recursive;
};
///////////////////////////////////////////////////////////////
char *STRTOK_R(char *pStr, const char *pDelims, char **ppSave, BOOL skipLeadingDelims = FALSE);
char *STRQTOK_R(
    char *pStr,
    const char *pDelims,
    char **ppSave,
    const char *pQuotes = "\"\"",
    BOOL discardQuotes = TRUE,
    BOOL skipLeadingDelims = FALSE);
BOOL StrToInt(const char *pStr, int *pNum);
const char *GetParam(const char *pArg, const char *pPattern);
void CreateArgsVector(
    const char *pName,
    const char *pArgs,
    Args &args,
    int *pArgc,
    const char ***pArgv);
void FreeArgsVector(
    const char **argv);
///////////////////////////////////////////////////////////////

#endif /* _UTILS_H_ */
