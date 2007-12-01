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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 ***************************************************************************/
#include <util/log.h>
#include <kio/jobuidelegate.h>
#include "movedatafilesjob.h"

namespace bt
{

	MoveDataFilesJob::MoveDataFilesJob() : err(false),active_job(0)
	{}


	MoveDataFilesJob::~MoveDataFilesJob()
	{}

	void MoveDataFilesJob::addMove(const QString & src,const QString & dst)
	{
		todo.insert(src,dst);
	}
		
	void MoveDataFilesJob::onJobDone(KJob* j)
	{
		if (j->error() || err)
		{
			if (!err)
				setError(KIO::ERR_INTERNAL);
			
			active_job = 0;
			if (j->error())
				((KIO::Job*)j)->ui()->showErrorMessage();
			
			// shit happened cancel all previous moves
			err = true;
			recover();
		}
		else
		{
			success.insert(active_src,active_dst);
			active_src = active_dst = QString::null;
			active_job = 0;
			startMoving();
		}
	}
	
	void MoveDataFilesJob::onCanceled(KJob* j)
	{
		setError(KIO::ERR_USER_CANCELED);
		active_job = 0;
		err = true;
		recover();
	}
	
	void MoveDataFilesJob::start()
	{
		startMoving();
	}
	
	void MoveDataFilesJob::startMoving()
	{
		if (todo.isEmpty())
		{
			emitResult();
			return;
		}
			
		QMap<QString,QString>::iterator i = todo.begin();	
		active_job = KIO::file_move(KUrl(i.key()),KUrl(i.value()),-1,KIO::HideProgressInfo);
		active_src = i.key();
		active_dst = i.value();
		Out(SYS_GEN|LOG_DEBUG) << "Moving " << active_src << " -> " << active_dst << endl;
		connect(active_job,SIGNAL(result(KJob*)),this,SLOT(onJobDone(KJob*)));
		connect(active_job,SIGNAL(canceled(KJob*)),this,SLOT(onCanceled(KJob*)));
		todo.erase(i);
	}
	
	void MoveDataFilesJob::recover()
	{
		if (success.isEmpty())
		{
			emitResult();
			return;
		}
		QMap<QString,QString>::iterator i = success.begin();	
		active_job = KIO::file_move(KUrl(i.value()),KUrl(i.key()),-1,KIO::HideProgressInfo);
		connect(active_job,SIGNAL(result(KJob*)),this,SLOT(onJobDone(KJob*)));
		connect(active_job,SIGNAL(canceled(KJob*)),this,SLOT(onCanceled(KJob*)));
		success.erase(i);
	}
}
#include "movedatafilesjob.moc"
