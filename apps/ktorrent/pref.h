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


#ifndef _KTORRENTPREF_H_
#define _KTORRENTPREF_H_

#include <kdialogbase.h>
#include <qframe.h>
#include <qmap.h> 
#include <interfaces/prefpageinterface.h>
 

class DownloadPref;
class GeneralPref;
class KTorrent;
class QListViewItem; 


class PrefPageOne : public kt::PrefPageInterface
{
	DownloadPref* dp;
public:
	PrefPageOne();
	virtual ~PrefPageOne();
	
	virtual void apply();
	virtual void updateData();	
	virtual void createWidget(QWidget* parent);
	virtual void deleteWidget();
};

class PrefPageTwo : public QObject,public kt::PrefPageInterface
{
	Q_OBJECT 
	GeneralPref* gp;
public:
	PrefPageTwo();
	virtual ~PrefPageTwo();
	
	virtual void apply();
	virtual void updateData();
	virtual void createWidget(QWidget* parent);
	virtual void deleteWidget();
	
private slots:
	void autosaveChecked(bool on);
};


 
class KTorrentPreferences : public KDialogBase
{
	Q_OBJECT
public:
	KTorrentPreferences(KTorrent & ktor);
	virtual ~KTorrentPreferences();
	
	void updateData();
	void addPrefPage(kt::PrefPageInterface* prefInterface);
	void removePrefPage(kt::PrefPageInterface* prefInterface);
private:
	virtual void slotOk();
	virtual void slotApply();
	
	
	
private:
	KTorrent & ktor;
	PrefPageOne* page_one;
	PrefPageTwo* page_two;
	QMap<kt::PrefPageInterface*,QFrame*> pages;
};





#endif // _KTORRENTPREF_H_
