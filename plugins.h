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
 * Revision 1.2  2008/11/13 08:07:40  vfrolov
 * Changed for staticaly linking
 *
 * Revision 1.1  2008/03/26 08:36:25  vfrolov
 * Initial revision
 *
 */

#ifndef _PLUGINS_H
#define _PLUGINS_H

///////////////////////////////////////////////////////////////
class PluginEnt;
///////////////////////////////////////////////////////////////
typedef vector<PluginEnt*> PluginArray;
typedef map<PLUGIN_TYPE, PluginArray*> TypePluginsMap;
typedef pair<HMODULE, PluginArray*> DllPlugins;
typedef vector<DllPlugins*> DllPluginsArray;
///////////////////////////////////////////////////////////////
class Plugins
{
  public:
    Plugins();
    ~Plugins();

    void List(ostream &o) const;
    void Help(const char *pProgPath, const char *pPluginName) const;

    BOOL Config(const char *pArg) const;

    const PLUGIN_ROUTINES_A *GetRoutines(
      PLUGIN_TYPE type,
      const char *pPluginName,
      HCONFIG *phConfig) const;

  private:
    void LoadPlugin(const string &pathPlugin);
    void InitPlugin(
        HMODULE hDll,
        PLUGIN_INIT_A *pInitProc,
        PluginArray &pluginArray);

    TypePluginsMap plugins;
    DllPluginsArray dllPluginsArray;
};
///////////////////////////////////////////////////////////////

#endif  // _PLUGINS_H
