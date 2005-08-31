/***************************************************************************
 *   Copyright (C) 2005 by                                                 *
 *   Joris Guisson <joris.guisson@gmail.com>                               *
 *   Ivan Vasic <ivasic@gmail.com>                                         *
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
#ifndef BTTORRENTCONTROL_H
#define BTTORRENTCONTROL_H

#include <qobject.h>
#include <qcstring.h> 
#include <qtimer.h>
#include <kurl.h>
#include "globals.h"
#include <libutil/timer.h>

class KProgressDialog;

namespace bt
{
	class Choker;
	class Torrent;
	class Tracker;
	class ChunkManager;
	class PeerManager;
	class Downloader;
	class Uploader;
	class Peer;
	class TorrentMonitor;
	class BitSet;
	
	/**
	 * @author Joris Guisson
	 * @brief Controls just about everything
	 * 
	 * This is the interface which any user gets to deal with.
	 * This class controls the uploading, downloading, choking,
	 * updating the tracker and chunk management.
	 */
	class TorrentControl : public QObject
	{
		Q_OBJECT
	public:
		TorrentControl();
		virtual ~TorrentControl();

		enum Status
		{
			NOT_STARTED,
			COMPLETE,
			SEEDING,
			DOWNLOADING,
			STALLED,
			STOPPED,
			ERROR
		};

		/**
		 * Make a BitSet of the status of all Chunks
		 * @param bs The BitSet
		 */
		void downloadedChunksToBitSet(BitSet & bs);

		/**
		 * Make a BitSet of the availability of all Chunks
		 * @param bs The BitSet
		 */
		void availableChunksToBitSet(BitSet & bs);

		/**
		 * Make a BitSet of the excluded Chunks
		 * @param bs The BitSet
		 */
		void excludedChunksToBitSet(BitSet & bs);
		
		/**
		 * Initialize the TorrentControl. 
		 * @param torrent The filename of the torrent file
		 * @param datadir The directory to store the data
		 * @throw Error when something goes wrong
		 */
		void init(const QString & torrent,const QString & datadir);

		/**
		 * Change to a new data dir. If this fails
		 * we will fall back on the old directory.
		 * @param new_dir The new directory
		 * @return true upon succes
		 */
		bool changeDataDir(const QString & new_dir);

		/**
		 * Roll back the previous changeDataDir call.
		 * Does nothing if there was no previous changeDataDir call.
		 */
		void rollback();

		/// Get the suggested name of the torrent
		QString getTorrentName() const;

		/// Get the number of bytes downloaded
		Uint64 getBytesDownloaded() const;

		/// Get the number of bytes uploaded
		Uint64 getBytesUploaded() const;

		/// Get the number of bytes left to download
		Uint64 getBytesLeft() const;

		/// Get the total number of bytes
		Uint64 getTotalBytes() const;

		/// Get the total number of bytes which need to be downloaded
		Uint64 getTotalBytesToDownload() const;

		/// Get the download rate in bytes per sec
		Uint32 getDownloadRate() const;

		/// Get the upload rate in bytes per sec
		Uint32 getUploadRate() const;

		/// Get the number of peers we are connected to
		Uint32 getNumPeers() const;

		/// Get the number of chunks we are currently downloading
		Uint32 getNumChunksDownloading() const;

		/// Get the total number of chunks
		Uint32 getTotalChunks() const;

		/// Get the number of chunks which have been downloaded
		Uint32 getNumChunksDownloaded() const;

		/// Get the number of chunks which have been excluded
		Uint32 getNumChunksExcluded() const;

		/**
		 * Get the number of seeders (total and the number connected to).
		 * @param total Total seederes 
		 * @param connected_to Number connected to
		 */
		void getSeederInfo(Uint32 & total,Uint32 & connected_to) const;

		/**
		 * Get the number of leechers (total and the number connected to).
		 * @param total Total seederes
		 * @param connected_to Number connected to
		 */
		void getLeecherInfo(Uint32 & total,Uint32 & connected_to) const;

		/// Get the current status of the download.
		Status getStatus() const {return status;}
		
		/// See if we are running
		bool isRunning() const {return running;}

		/// See if the torrent has been started
		bool isStarted() const {return started;}

		/// See if the torrent was saved
		bool isSaved() const {return saved;}

		/// See if we are allowed to startup this torrent automatically.
		bool isAutostartAllowed() const {return autostart;}

		/// Get the data directory of this torrent
		QString getDataDir() const {return datadir;}

		/// Set the monitor
		void setMonitor(TorrentMonitor* tmo);

		/// See if we have a multi file torrent
		bool isMultiFileTorrent() const;

		/// Get the Torrent.
		const Torrent & getTorrent() const {return *tor;}

		/// Return an error message (only valid when status == ERROR).
		QString getErrorMessage() const {return error_msg;}

		/// Return a short error message (only valid when status == ERROR). 
		QString getShortErrorMessage() const {return short_error_msg; }
		/**
		 * Set the interval between two tracker updates.
		 * @param interval The interval in milliseconds
		 */
		void setTrackerTimerInterval(Uint32 interval);

		/**
		 * Get the running time of this torrent in seconds
		 * @return Uint32 - time in seconds
		 */
		Uint32 getRunningTime() const; 

		/**
		* Checks if torrent is multimedial and chunks needed for preview are downloaded
		* @param start_chunk The index of starting chunk to check
		* @param end_chunk The index of the last chunk to check
		* In case of single torrent file defaults can be used (0,1)
		**/
		bool readyForPreview(int start_chunk = 0, int end_chunk = 1);
	public slots:
		/**
		 * Update the object, should be called periodically.
		 */
		void update();
		
		/**
		 * Start the download of the torrent.
		 */
		void start();
		
		/**
		 * Stop the download, closes all connections.
		 * @param user wether or not the user did this explicitly
		 */
		void stop(bool user);
		
		/**
		 * When the torrent is finished, the final file(s) can be
		 * reconstructed.
		 * @param dir The or directory to store files
		 */
		void reconstruct(const QString & dir);
		
		/**
		 * Update the tracker, this should normally handled internally.
		 * We leave it public so that the user can do a manual announce.
		 */
		void updateTracker() {updateTracker(QString::null);}

		/// Get the time to the next tracker update in milliseconds.
		Uint32 getTimeToNextTrackerUpdate() const;
		
		/// Get the status of the tracker
		QString getTrackerStatus() const {return trackerstatus;}

		/// Get the number of bytes downloaded in this session 
		Uint32 getSessionBytesDownloaded() const { return getBytesDownloaded() - prev_bytes_dl; }

		/// Get the number of bytes uploaded in this session 
		Uint32 getSessionBytesUploaded() const {return (up) ? getBytesUploaded() - prev_bytes_ul : 0; }
		
	private slots:
		void onNewPeer(Peer* p);
		void onPeerRemoved(Peer* p);
		void doChoking();

		/**
		 * An error occured during the update of the tracker.
		 */
		void trackerResponseError();

		/**
		 * The Tracker updated.
		 */
		void trackerResponse();

	signals:
		/**
		 * Emited when a TorrentControl object is finished downloading.
		 * @param me The TorrentControl
		 */
		void finished(bt::TorrentControl* me);

		/**
		 * Emited when a Torrent download is stopped by error
		 * @param me The TorrentControl
		 * @param msg Error message
		 */
		void stoppedByError(bt::TorrentControl* me, QString msg);

		
	private:	
		void updateTracker(const QString & ev,bool last_succes = true);
		void updateStatusMsg();
		void saveStats();
		void loadStats();
		
	private:
		Torrent* tor;
		Tracker* tracker;
		ChunkManager* cman;
		PeerManager* pman;
		Downloader* down;
		Uploader* up;
		Choker* choke;
		
		Timer tracker_update_timer,choker_update_timer,stats_save_timer;
		Uint32 tracker_update_interval;
		
		QString datadir,old_datadir,trackerevent;
		QString trackerstatus,error_msg,short_error_msg;
		Uint16 port;
		bool completed,running,started,saved,autostart,stopped_by_error;
		TorrentMonitor* tmon;
		Uint32 num_tracker_attempts;
		KURL last_tracker_url;
		Status status;
		QTime time_started;
		int running_time;
		Uint64 prev_bytes_dl, prev_bytes_ul;
	};
}

#endif
