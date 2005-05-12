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
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include <qurl.h>
#include <string.h>
#include <algorithm>
#include "log.h"
#include "sha1hash.h"
#include "sha1hashgen.h"
#include "urlencoder.h"


namespace bt
{
	SHA1Hash::SHA1Hash()
	{
		std::fill(hash,hash+20,'\0');
	}

	SHA1Hash::SHA1Hash(const SHA1Hash & other)
	{
		for (int i = 0;i < 20;i++)
			hash[i] = other.hash[i];
	}

	SHA1Hash::SHA1Hash(const unsigned char* h)
	{
		memcpy(hash,h,20);
	}


	SHA1Hash::~SHA1Hash()
	{}

	SHA1Hash & SHA1Hash::operator = (const SHA1Hash & other)
	{
		for (int i = 0;i < 20;i++)
			hash[i] = other.hash[i];
		return *this;
	}

	bool SHA1Hash::operator == (const SHA1Hash & other) const
	{
		for (int i = 0;i < 20;i++)
			if (hash[i] != other.hash[i])
				return false;

		return true;
	}

	SHA1Hash SHA1Hash::generate(unsigned char* data,unsigned int len)
	{
		SHA1HashGen hg;

		return hg.generate(data,len);
	}

	QString SHA1Hash::toString() const
	{
		char tmp[41];
		QString fmt;
		for (int i = 0;i < 20;i++)
			fmt += "%02x";
		tmp[40] = '\0';
		snprintf(tmp,40,fmt.ascii(),
				hash[0],hash[1],hash[2],hash[3],hash[4],
				hash[5],hash[6],hash[7],hash[8],hash[9],
				hash[10],hash[11],hash[12],hash[13],hash[14],
				hash[15],hash[16],hash[17],hash[18],hash[19]);
		return QString(tmp);
	}

	QString SHA1Hash::toURLString() const
	{
	/*	QChar dodo[20];
		for (int i = 0;i < 20;i++)
		dodo[i] = hash[i];
		QByteArray dodo(20);
		dodo.assign((const char*)hash,20);
		Out() << toString() << endl;
		QString res(dodo);
		return res;*/
		return URLEncoder::encode((const char*)hash,20);
	}

	Log & operator << (Log & out,const SHA1Hash & h)
	{
		out << h.toString();
		return out;
	}


}
;
