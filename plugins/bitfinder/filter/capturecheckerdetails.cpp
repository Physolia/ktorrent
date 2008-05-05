/***************************************************************************
 *   Copyright (C) 2008 by Alan Jones                                      *
 *   skyphyr@gmail.com                                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
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
#include <kinputdialog.h>

#include "capturecheckerdetails.h"

#include <util/log.h>

using namespace bt;

namespace kt
	{
	
	void CaptureCheckerDetails::resizeColumns()
		{
		if (captures->columnWidth(1) != 65 && captures->width()!=100)
			return;
		
		int captureWidth = captures->width();
		captures->setColumnWidth(0, captureWidth * 0.35 - 5);
		captures->setColumnWidth(1, captureWidth * 0.65);
		
		int variableWidth = variables->width();
		variables->setColumnWidth(0, variableWidth * 0.5 - 5);
		variables->setColumnWidth(1, variableWidth * 0.25);
		variables->setColumnWidth(2, variableWidth * 0.25);
		
		int mappingWidth = mappings->width();
		mappings->setColumnWidth(0, mappingWidth * 0.2);
		mappings->setColumnWidth(1, mappingWidth * 0.2);
		mappings->setColumnWidth(2, mappingWidth * 0.1);
		mappings->setColumnWidth(3, mappingWidth * 0.5 - 5);
		}
	
	CaptureCheckerDetails::CaptureCheckerDetails(QWidget * parent) 
			: QWidget(parent)
		{
		setupUi(this);
		
		//Let's build the toolbars we need
		//capturesToolbar
		capturesToolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
		capturesToolbar->setOrientation(Qt::Vertical);
		capturesToolbar->setIconDimensions(16);
		captureAdd = new KAction(KIcon("list-add"),i18n("Add Capture"),this);
		captureRemove = new KAction(KIcon("list-remove"),i18n("Remove Capture"),this);
		capturesToolbar->addAction(captureAdd);
		capturesToolbar->addAction(captureRemove);
		connect(captureAdd, SIGNAL(triggered( Qt::MouseButtons, Qt::KeyboardModifiers )), this, SLOT(addNewCapture()));

		//variablesToolbar
		variablesToolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
		variablesToolbar->setOrientation(Qt::Vertical);
		variablesToolbar->setIconDimensions(16);
		variableAdd = new KAction(KIcon("list-add"),i18n("Add Variable"),this);
		variableRemove = new KAction(KIcon("list-remove"),i18n("Remove Variable"),this);
		variableUp = new KAction(KIcon("arrow-up"), i18n("Move Variable Up"), this);
		variableDown = new KAction(KIcon("arrow-down"), i18n("Move Variable Down"), this);
		variablesToolbar->addAction(variableAdd);
		variablesToolbar->addAction(variableRemove);
		variablesToolbar->addAction(variableUp);
		variablesToolbar->addAction(variableDown);
		
		//captures
		captures->setColumnCount(2);
		QStringList captureHeaders;
		captureHeaders << "Name" << "Expression";
		captures->setHorizontalHeaderLabels(captureHeaders);
		
		//variables
		variables->setColumnCount(3);
		QStringList variableHeaders;
		variableHeaders << "Name" << "Min" << "Max";
		variables->setHorizontalHeaderLabels(variableHeaders);
		
		//mappings
		mappings->setColumnCount(4);
		QStringList mappingHeaders;
		mappingHeaders << "Capture Name" << "Variable" << "Index" << "Test Value";
		mappings->setHorizontalHeaderLabels(mappingHeaders);
		
		}
	
	void CaptureCheckerDetails::setCaptureChecker(CaptureChecker* value)
		{
		captureChecker = value;
		}
	
	void CaptureCheckerDetails::addNewCapture()
		{
		bool ok = false;
		QString name = KInputDialog::getText(i18n("Add New Capture"), 
					i18n("Please enter the new capture name."),QString(),&ok,this);
		
		if (ok)
			{
			
			}
		}
	
	}