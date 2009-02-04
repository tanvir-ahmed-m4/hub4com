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
 * Revision 1.7  2009/02/04 12:26:54  vfrolov
 * Implemented --load option for filters
 *
 * Revision 1.6  2008/10/02 07:52:38  vfrolov
 * Added removing macroses for undefined parameters of --load option
 *
 * Revision 1.5  2008/09/26 14:29:13  vfrolov
 * Added substitution <PRM0> by <file> for --load=<file>
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

#include "precomp.h"
#include "utils.h"

///////////////////////////////////////////////////////////////
Args::Args(int argc, const char *const argv[])
  : num_recursive(0)
{
  for (int i = 0 ; i < argc ; i++)
    Add(argv[i]);
}

void Args::Add(const vector<string> &args)
{
  for (vector<string>::const_iterator i = args.begin() ; i != args.end() ; i++)
    Add(*i);
}

static void SubstParams(string &argBuf, const vector<string> &params)
{
  for (string::size_type off = argBuf.find("%%"); off != argBuf.npos ; off = argBuf.find("%%", off)) {
    string::size_type off_beg = off + 2;
    string::size_type off_end = argBuf.find("%%", off_beg);

    if (off_end != argBuf.npos && off_end != off_beg) {
      string var = argBuf.substr(off_beg, off_end - off_beg);
      BOOL isToken = TRUE;

      for (string::size_type i = 0 ; i < var.length() ; i++) {
        if (!isdigit(var[i])) {
          isToken = FALSE;
          break;
        }
      }

      string val;

      if (isToken) {
        for (vector<string>::size_type i = 0 ; i < params.size() ; i++) {
          stringstream par;

          par << i;

          if (var == par.str()) {
            val = params[i];
            break;
          }
        }

        argBuf.replace(off, var.length() + 4, val);
        off += val.length();
        continue;
      }
    }

    off++;
  }
}

void Args::Add(const string &arg)
{
  const char *pLoad = GetParam(arg.c_str(), LoadPrefix());

  if (!pLoad) {
    //cout << "<" << arg << ">" << endl;
    push_back(arg);
    return;
  }

  char *pTmp = _strdup(pLoad);

  if (!pTmp) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  char *pSave;
  char *pFileSpec = STRQTOK_R(pTmp, ":", &pSave);

  char *pFile;
  char *pBegin;
  char *pEnd;

  if (pFileSpec) {
    char *pSave2;

    pFile = STRQTOK_R(pFileSpec, ",", &pSave2);
    pBegin = STRQTOK_R(NULL, ",", &pSave2);
    pEnd = STRQTOK_R(NULL, ",", &pSave2);
  } else {
    pFile = NULL;
    pBegin = NULL;
    pEnd = NULL;
  }

  vector<string> paramsLoad;

  ifstream ifile;
  istream *pInStream;

  if (pFile && *pFile) {
    ifile.open(pFile);

    if (ifile.fail()) {
      cerr << "Can't open file " << pFile << endl;
      exit(1);
    }

    paramsLoad.push_back(pFile);
    pInStream = &ifile;
  } else {
    paramsLoad.push_back("");
    pInStream = &cin;
  }

  for (char *p = STRQTOK_R(NULL, ",", &pSave) ; p ; p = STRQTOK_R(NULL, ",", &pSave))
    paramsLoad.push_back(p);

  while (pInStream->good()) {
    stringstream line;

    pInStream->get(*line.rdbuf());

    if (!pInStream->fail()) {
      string str = line.str();

      SubstParams(str, paramsLoad);

      string::size_type first_non_space = string::npos;
      string::size_type last_non_space = string::npos;

      for (string::size_type i = 0 ; i < str.length() ; i++) {
        if (!isspace(str[i])) {
          if (first_non_space == string::npos)
            first_non_space = i;

          last_non_space = i;
        }
      }

      if (first_non_space != string::npos) {
        str = str.substr(first_non_space, last_non_space + 1 - first_non_space);

        if (pBegin && *pBegin) {
          if (str == pBegin)
            pBegin = NULL;

          continue;
        }

        if (pEnd && *pEnd && str == pEnd)
          break;

        if (str[0] == '#')
          continue;

        if (num_recursive > RecursiveMax()) {
          cerr << "Too many recursive options " << arg << endl;
          exit(1);
        }

        num_recursive++;
        Add(str);
        num_recursive--;
      }
    } else {
      if (!pInStream->eof())
        pInStream->clear();
    }

    char ch;

    pInStream->get(ch);
  }

  free(pTmp);
}
///////////////////////////////////////////////////////////////
static BOOL IsDelim(char c, const char *pDelims)
{
  while (*pDelims) {
    if (c == *pDelims++)
      return TRUE;
  }

  return FALSE;
}
///////////////////////////////////////////////////////////////
char *STRTOK_R(char *pStr, const char *pDelims, char **ppSave, BOOL skipLeadingDelims)
{
  if (!pStr)
    pStr = *ppSave;

  if (skipLeadingDelims) {
    while (IsDelim(*pStr, pDelims))
      pStr++;
  }

  if (!*pStr) {
    *ppSave = pStr;
    return NULL;
  }

  char *pToken = pStr;

  while (*pStr && !IsDelim(*pStr, pDelims))
    pStr++;

  if (*pStr)
    *pStr++ = 0;

  *ppSave = pStr;

  return pToken;
}
///////////////////////////////////////////////////////////////
char *STRQTOK_R(
    char *pStr,
    const char *pDelims,
    char **ppSave,
    const char *pQuotes,
    BOOL discardQuotes,
    BOOL skipLeadingDelims)
{
  if (!pStr)
    pStr = *ppSave;

  if (skipLeadingDelims) {
    while (IsDelim(*pStr, pDelims))
      pStr++;
  }

  if (!*pStr) {
    *ppSave = pStr;
    return NULL;
  }

  char *pToken = pStr;
  BOOL quoted = FALSE;
  int cntMask = 0;

  while (*pStr && (quoted || !IsDelim(*pStr, pDelims))) {
    if (quoted ? (*pStr == pQuotes[1]) : (*pStr == pQuotes[0])) {
      if (cntMask%2 == 0) {
        if (discardQuotes)
          memmove(pStr, pStr + 1, strlen(pStr + 1) + 1);
        else
          pStr++;

        quoted = !quoted;
      } else {
        memmove(pStr - (cntMask/2 + 1), pStr, strlen(pStr) + 1);
        pStr -= cntMask/2;
      }
      cntMask = 0;
      continue;
    }

    if (*pStr == '\\')
      cntMask++;
    else
      cntMask = 0;

    pStr++;
  }

  if (*pStr)
    *pStr++ = 0;

  *ppSave = pStr;

  return pToken;
}
///////////////////////////////////////////////////////////////
BOOL StrToInt(const char *pStr, int *pNum)
{
  BOOL res = FALSE;
  int num;
  int sign = 1;

  switch (*pStr) {
    case '-':
      sign = -1;
    case '+':
      pStr++;
      break;
  }

  for (num = 0 ;; pStr++) {
    switch (*pStr) {
      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        num = num*10 + (*pStr - '0');
        res = TRUE;
        continue;
      case 0:
        break;
      default:
        res = FALSE;
    }
    break;
  }

  if (pNum)
    *pNum = num*sign;

  return res;
}
///////////////////////////////////////////////////////////////
const char *GetParam(const char *pArg, const char *pPattern)
{
  size_t lenPattern = strlen(pPattern);

  if (_strnicmp(pArg, pPattern, lenPattern) != 0)
    return NULL;

  return pArg + lenPattern;
}
///////////////////////////////////////////////////////////////
void CreateArgsVector(
    const char *pName,
    const char *pArgs,
    Args &args,
    int *pArgc,
    const char ***pArgv)
{
  vector<string> tokens;

  if (pArgs) {
    char *pTmp = _strdup(pArgs);

    if (!pTmp) {
      cerr << "No enough memory." << endl;
      exit(2);
    }

    char *pSave;
    const char *pToken;

    for (pToken = STRQTOK_R(pTmp, " ", &pSave, "\"\"", TRUE, TRUE) ;
         pToken ;
         pToken = STRQTOK_R(NULL, " ", &pSave, "\"\"", TRUE, TRUE))
    {
      tokens.push_back(pToken);
    }

    free(pTmp);
  }

  args.Add(tokens);

  *pArgc = int(args.size() + 1);
  *pArgv = (const char **)malloc((*pArgc + 1) * sizeof((*pArgv)[0]));

  if (!*pArgv) {
    cerr << "No enough memory." << endl;
    exit(2);
  }

  (*pArgv)[0] = pName;

  for (int i = 1 ; i < *pArgc ; i++)
    (*pArgv)[i] = args[i - 1].c_str();

  (*pArgv)[*pArgc] = NULL;
}

void FreeArgsVector(const char **argv)
{
  if (argv)
    free(argv);
}
///////////////////////////////////////////////////////////////
