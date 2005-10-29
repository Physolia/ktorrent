/***************************************************************************
 *   Copyright (C) 2005 by Joris Guisson                                   *
 *   joris.guisson@gmail.com                                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Steet, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ***************************************************************************/
#include <qfile.h>
#include <qtextstream.h>
#include <kparts/componentfactory.h>
#include <util/log.h>
#include <util/error.h>
#include <util/fileops.h>
#include <torrent/globals.h>
#include <interfaces/guiinterface.h>
#include "pluginmanager.h"
#include "pluginmanagerprefpage.h"

using namespace bt;

namespace kt
{

	PluginManager::PluginManager(CoreInterface* core,GUIInterface* gui) : core(core),gui(gui)
	{
		prefpage = 0;
		pltoload.append("infowidgetplugin");
		pltoload.append("searchplugin");
	}

	PluginManager::~PluginManager()
	{
		delete prefpage;
		unloaded.setAutoDelete(true);
		plugins.setAutoDelete(true);
	}

	void PluginManager::loadPluginList()
	{
		if (!prefpage)
		{
			prefpage = new PluginManagerPrefPage(this);
			gui->addPrefPage(prefpage);
		}
		KTrader::OfferList offers = KTrader::self()->query("KTorrent/Plugin");

		KTrader::OfferList::ConstIterator iter;
		for(iter = offers.begin(); iter != offers.end(); ++iter)
		{
			KService::Ptr service = *iter;
			int errCode = 0;
			Plugin* plugin =
					KParts::ComponentFactory::createInstanceFromService<kt::Plugin>
					(service, 0, 0, QStringList(),&errCode);
	        // here we ought to check the error code.
			
			if (plugin)
			{
				unloaded.insert(plugin->getName(),plugin);
				if (pltoload.contains(plugin->getName()))
					load(plugin->getName());
			}
		}
	}

	
	void PluginManager::load(const QString & name)
	{
		Plugin* p = unloaded.find(name);
		if (!p)
			return;

		Out() << "Loading plugin "<< p->getName() << endl;
		p->setCore(core);
		p->setGUI(gui);
		p->load();
		gui->mergePluginGui(p);
		unloaded.erase(name);
		plugins.insert(p->getName(),p);
		p->loaded = true;
	}
		
	void PluginManager::unload(const QString & name)
	{
		Plugin* p = plugins.find(name);
		if (!p)
			return;

		gui->removePluginGui(p);
		p->unload();
		plugins.erase(name);
		unloaded.insert(p->getName(),p);
		p->loaded = false;
	}
		
	void PluginManager::loadAll()
	{
		bt::PtrMap<QString,Plugin>::iterator i = unloaded.begin();
		while (i != unloaded.end())
		{
			Plugin* p = i->second;
			p->setCore(core);
			p->setGUI(gui);
			p->load();
			gui->mergePluginGui(p);
			unloaded.erase(p->getName());
			plugins.insert(p->getName(),p);
			p->loaded = true;
			i++;
		}
		unloaded.clear();
	}

	void PluginManager::unloadAll()
	{
		bt::PtrMap<QString,Plugin>::iterator i = plugins.begin();
		while (i != plugins.end())
		{
			Plugin* p = i->second;
			gui->removePluginGui(p);
			p->unload();
			unloaded.insert(p->getName(),p);
			p->loaded = false;
			i++;
		}
		plugins.clear();
	}

	void PluginManager::updateGuiPlugins()
	{
		bt::PtrMap<QString,Plugin>::iterator i = plugins.begin();
		while (i != plugins.end())
		{
			Plugin* p = i->second;
			p->guiUpdate();
			i++;
		}
	}

	void PluginManager::fillPluginList(QPtrList<Plugin> & plist)
	{
		bt::PtrMap<QString,Plugin>::iterator i = plugins.begin();
		while (i != plugins.end())
		{
			Plugin* p = i->second;
			plist.append(p);
			i++;
		}

		
		i = unloaded.begin();
		while (i != unloaded.end())
		{
			Plugin* p = i->second;
			plist.append(p);
			i++;
		}
	}

	bool PluginManager::isLoaded(const QString & name) const
	{
		const Plugin* p = plugins.find(name);
		return p != 0;
	}

	void PluginManager::loadConfigFile(const QString & file)
	{
		// make a default config file if doesn't exist
		if (!bt::Exists(file))
		{
			writeDefaultConfigFile(file);
			return;
		}

		QFile f(file);
		if (!f.open(IO_ReadOnly))
		{
			Out() << "Cannot open file " << file << " : " << f.errorString() << endl;
			return;
		}

		pltoload.clear();
		
		QTextStream in(&f);
		while (!in.atEnd())
		{
			QString l = in.readLine();
			if (l.isNull())
				break;

			pltoload.append(l);
		}
	}

	void PluginManager::saveConfigFile(const QString & file)
	{
		QFile f(file);
		if (!f.open(IO_WriteOnly))
		{
			Out() << "Cannot open file " << file << " : " << f.errorString() << endl;
			return;
		}

		QTextStream out(&f);
		bt::PtrMap<QString,Plugin>::iterator i = plugins.begin();
		while (i != plugins.end())
		{
			Plugin* p = i->second;
			out << p->getName() << endl;
			i++;
		}
	}
	

	void PluginManager::writeDefaultConfigFile(const QString & file)
	{
		// by default we will load the infowidget and searchplugin
		QFile f(file);
		if (!f.open(IO_WriteOnly))
		{
			Out() << "Cannot open file " << file << " : " << f.errorString() << endl;
			return;
		}

		QTextStream out(&f);
		out << "infowidgetplugin" << endl << "searchplugin" << endl;

		pltoload.clear();
		pltoload.append("infowidgetplugin");
		pltoload.append("searchplugin");
	}
}
