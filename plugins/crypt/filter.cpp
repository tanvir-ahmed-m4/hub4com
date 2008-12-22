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
 * Revision 1.3  2008/12/22 09:40:45  vfrolov
 * Optimized message switching
 *
 * Revision 1.2  2008/12/19 18:27:47  vfrolov
 * Fixed Release compile error
 *
 * Revision 1.1  2008/12/05 14:27:02  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "../plugins_api.h"
///////////////////////////////////////////////////////////////
namespace FilterCrypt {
///////////////////////////////////////////////////////////////
static ROUTINE_BUF_APPEND *pBufAppend;
static ROUTINE_MSG_REPLACE_BUF *pmsgreplacebuf;
///////////////////////////////////////////////////////////////
#ifndef _DEBUG
  #define DEBUG_PARAM(par)
#else   /* _DEBUG */
  #define DEBUG_PARAM(par) par
#endif  /* _DEBUG */
///////////////////////////////////////////////////////////////
static const char *GetParam(const char *pArg, const char *pPattern)
{
  size_t lenPattern = strlen(pPattern);

  if (_strnicmp(pArg, pPattern, lenPattern) != 0)
    return NULL;

  return pArg + lenPattern;
}
///////////////////////////////////////////////////////////////
class Valid {
  public:
    Valid() : isValid(TRUE) {}
    void Invalidate() { isValid = FALSE; }
    void Validate() { isValid = TRUE; }
    BOOL IsValid() const { return isValid; }
  private:
    BOOL isValid;
};
///////////////////////////////////////////////////////////////
class State : public Valid {
  public:
    State() { Invalidate(); }

    HCRYPTKEY hKeyIn;
    HCRYPTKEY hKeyOut;
};
///////////////////////////////////////////////////////////////
class Filter : public Valid {
  public:
    Filter(int argc, const char *const argv[]);
    State *GetState(HMASTERPORT hPort);
    void Open(State *pState);
    void Close(State *pState);

  private:
    HCRYPTPROV hProv;
    HCRYPTHASH hHash;

    typedef map<HMASTERPORT, State*> PortsMap;
    typedef pair<HMASTERPORT, State*> PortPair;

    PortsMap portsMap;
};

Filter::Filter(int argc, const char *const argv[])
{
  BOOL noSecret = TRUE;

  for (const char *const *pArgs = &argv[1] ; argc > 1 ; pArgs++, argc--) {
    const char *pArg = GetParam(*pArgs, "--");

    if (!pArg) {
      cerr << "Unknown option " << *pArgs << endl;
      Invalidate();
      continue;
    }

    const char *pParam;

    if ((pParam = GetParam(pArg, "secret=")) != NULL) {
      if (!*pParam)
        cerr << "WARNING: The secret is empty" << endl;

      if (!CryptAcquireContext(&hProv, NULL, MS_ENHANCED_PROV, PROV_RSA_FULL, 0)) {
        DWORD err = GetLastError();
        cerr << "CryptAcquireContext() - error=" << err << endl;
        Invalidate();
        continue;
      }

      if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
        DWORD err = GetLastError();
        cerr << "CryptCreateHash() - error=" << err << endl;
        Invalidate();
        continue;
      }

      if (!CryptHashData(hHash, (const BYTE *)pParam, (DWORD)strlen(pParam), 0)) {
        DWORD err = GetLastError();
        cerr << "CryptHashData() - error=" << err << endl;
        Invalidate();
        continue;
      }

      //if (!CryptDestroyHash(hHash)) {
      //  DWORD err = GetLastError();
      //  cerr << "CryptDestroyHash() - error=" << err << endl;
      //}

      //if (CryptReleaseContext(hProv, 0)) {
      //  DWORD err = GetLastError();
      //  cerr << "CryptReleaseContext() - error=" << err << endl;
      //}

      noSecret = FALSE;
    }
    else {
      cerr << "Unknown option " << pArg << endl;
      Invalidate();
    }
  }

  if (noSecret) {
    cerr << "The secret was not set" << endl;
    Invalidate();
  }
}

State *Filter::GetState(HMASTERPORT hPort)
{
  PortsMap::iterator iPair = portsMap.find(hPort);

  if (iPair == portsMap.end()) {
      portsMap.insert(PortPair(hPort, NULL));

      iPair = portsMap.find(hPort);

      if (iPair == portsMap.end())
        return NULL;
  }

  if (!iPair->second)
    iPair->second = new State();

  return iPair->second;
}

void Filter::Open(State *pState)
{
  _ASSERTE(!pState->IsValid());

  static const DWORD flags = (((DWORD)128) << 16);

  if (!CryptDeriveKey(hProv, CALG_RC4, hHash, flags, &pState->hKeyIn)) {
    DWORD err = GetLastError();
    cerr << "CryptDeriveKey() - error=" << err << endl;
    return;
  }

  if (!CryptDeriveKey(hProv, CALG_RC4, hHash, flags, &pState->hKeyOut)) {
    DWORD err = GetLastError();
    cerr << "CryptDeriveKey() - error=" << err << endl;

    if (!CryptDestroyKey(pState->hKeyIn)) {
      DWORD err = GetLastError();
      cerr << "CryptDestroyKey() - error=" << err << endl;
    }

    return;
  }

  pState->Validate();
}

void Filter::Close(State *pState)
{
  if (!pState->IsValid())
    return;

  pState->Invalidate();

  if (!CryptDestroyKey(pState->hKeyIn) || !CryptDestroyKey(pState->hKeyOut)) {
    DWORD err = GetLastError();
    cerr << "CryptDestroyKey() - error=" << err << endl;
  }
}
///////////////////////////////////////////////////////////////
static PLUGIN_TYPE CALLBACK GetPluginType()
{
  return PLUGIN_TYPE_FILTER;
}
///////////////////////////////////////////////////////////////
static const PLUGIN_ABOUT_A about = {
  sizeof(PLUGIN_ABOUT_A),
  "crypt",
  "Copyright (c) 2008 Vyacheslav Frolov",
  "GNU General Public License",
  "Encrypting/decrypting filter",
};

static const PLUGIN_ABOUT_A * CALLBACK GetPluginAbout()
{
  return &about;
}
///////////////////////////////////////////////////////////////
static void CALLBACK Help(const char *pProgPath)
{
  cerr
  << "Usage:" << endl
  << "  " << pProgPath << " ... --create-filter=" << GetPluginAbout()->pName << "[,<FID>][:<options>] ... --add-filters=<ports>:[...,]<FID>[,...] ..." << endl
  << endl
  << "Options:" << endl
  << "  --secret=<secret>       - set secret (mandatory)." << endl
  << endl
  << "IN method input data stream description:" << endl
  << "  LINE_DATA - encrypted data." << endl
  << endl
  << "IN method output data stream description:" << endl
  << "  LINE_DATA - decrypted data." << endl
  << endl
  << "OUT method input data stream description:" << endl
  << "  LINE_DATA - raw (not encrypted) data." << endl
  << endl
  << "OUT method output data stream description:" << endl
  << "  LINE_DATA - encrypted data." << endl
  << endl
  << "Examples:" << endl
  ;
}
///////////////////////////////////////////////////////////////
static HFILTER CALLBACK Create(
    HCONFIG /*hConfig*/,
    int argc,
    const char *const argv[])
{
  Filter *pFilter = new Filter(argc, argv);

  if (!pFilter)
    return NULL;

  if (!pFilter->IsValid()) {
    delete pFilter;
    return NULL;
  }

  return (HFILTER)pFilter;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK InMethod(
    HFILTER hFilter,
    HMASTERPORT hFromPort,
    HUB_MSG *pInMsg,
    HUB_MSG **DEBUG_PARAM(ppEchoMsg))
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(hFromPort != NULL);
  _ASSERTE(pInMsg != NULL);
  _ASSERTE(ppEchoMsg != NULL);
  _ASSERTE(*ppEchoMsg == NULL);

  switch (HUB_MSG_T2N(pInMsg->type)) {
    case HUB_MSG_T2N(HUB_MSG_TYPE_LINE_DATA): {
      _ASSERTE(pInMsg->u.buf.pBuf != NULL || pInMsg->u.buf.size == 0);

      DWORD len = pInMsg->u.buf.size;

      if (len == 0)
        break;

      State *pState = ((Filter *)hFilter)->GetState(hFromPort);

      if (!pState)
        return FALSE;

      if (!CryptDecrypt(pState->hKeyIn, 0, FALSE, 0, pInMsg->u.buf.pBuf, &pInMsg->u.buf.size)) {
        DWORD err = GetLastError();
        cerr << "CryptDecrypt() - error=" << err << endl;
        return FALSE;
      }

      _ASSERTE(pInMsg->u.buf.size == len);

      break;
    }
    case HUB_MSG_T2N(HUB_MSG_TYPE_CONNECT): {
      State *pState = ((Filter *)hFilter)->GetState(hFromPort);

      if (!pState)
        return FALSE;

      if (pInMsg->u.val)
        ((Filter *)hFilter)->Open(pState);
      else
        ((Filter *)hFilter)->Close(pState);

      break;
    }
  }

  return TRUE;
}
///////////////////////////////////////////////////////////////
static BOOL CALLBACK OutMethod(
    HFILTER hFilter,
    HMASTERPORT DEBUG_PARAM(hFromPort),
    HMASTERPORT hToPort,
    HUB_MSG *pOutMsg)
{
  _ASSERTE(hFilter != NULL);
  _ASSERTE(hFromPort != NULL);
  _ASSERTE(hToPort != NULL);
  _ASSERTE(pOutMsg != NULL);

  switch (HUB_MSG_T2N(pOutMsg->type)) {
    case HUB_MSG_T2N(HUB_MSG_TYPE_LINE_DATA): {
      _ASSERTE(pOutMsg->u.buf.pBuf != NULL || pOutMsg->u.buf.size == 0);

      DWORD len = pOutMsg->u.buf.size;

      if (len == 0)
        break;

      State *pState = ((Filter *)hFilter)->GetState(hToPort);

      if (!pState)
        return FALSE;

      if (!CryptEncrypt(pState->hKeyOut, 0, FALSE, 0, pOutMsg->u.buf.pBuf, &pOutMsg->u.buf.size, len)) {
        DWORD err = GetLastError();
        cerr << "CryptEncrypt() - error=" << err << endl;
        return FALSE;
      }

      _ASSERTE(pOutMsg->u.buf.size == len);

      break;
    }
  }

  return pOutMsg != NULL;
}
///////////////////////////////////////////////////////////////
static const FILTER_ROUTINES_A routines = {
  sizeof(FILTER_ROUTINES_A),
  GetPluginType,
  GetPluginAbout,
  Help,
  NULL,           // ConfigStart
  NULL,           // Config
  NULL,           // ConfigStop
  Create,
  NULL,           // Init
  InMethod,
  OutMethod,
};

static const PLUGIN_ROUTINES_A *const plugins[] = {
  (const PLUGIN_ROUTINES_A *)&routines,
  NULL
};
///////////////////////////////////////////////////////////////
PLUGIN_INIT_A InitA;
const PLUGIN_ROUTINES_A *const * CALLBACK InitA(
    const HUB_ROUTINES_A * pHubRoutines)
{
  if (!ROUTINE_IS_VALID(pHubRoutines, pBufAppend))
    return NULL;

  pBufAppend = pHubRoutines->pBufAppend;

  return plugins;
}
///////////////////////////////////////////////////////////////
} // end namespace
///////////////////////////////////////////////////////////////
