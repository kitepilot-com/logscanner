/*******************************************************************************
 * File: logscanner/ConfContainer.h                                            *
 * Created:     Jan-07-25                                                      *
 * Last update: May-17-25                                                      *
 * Top level header file.                                                      *
 *                                                                             *
 * This class contains all the information about a specific log file.          *
 * There is a instance of this clas foe every logfile specified in the         *
 " "config" directory.                                                         *
 *******************************************************************************/

#ifndef CONF_CONTAINER_H
#define CONF_CONTAINER_H

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <atomic>

#include "ReadFileCtrl.h"

class ConfContainer
{
	public:
		// This flag is to "true"  restart of the reading thread of *THIS* logfile.
		std::atomic<bool>   RESTART_THREAD;

		enum StatusVal {
			INVALID = -1,
			READLOGS_ERR,
			FEED_ERR,
			LUMP_ERR,
			READDIR_ERR,
			LUMFLAG_ERR,
			HEALTHY
		};

		typedef enum {
			REGEXP = 'R',
			BLDMAP = 'M',
			FORMAT = 'F'
		} DATE_EXTRACT;

		static constexpr int  m_FIRST_LINE_MONITOR_INTERVAL = 100; // milliseconds...

		ConfContainer(std::string logFilePath, std::map<char, std::string> dateExtrct, std::map<std::string, char> regexpList);
		virtual ~ConfContainer();

		std::string getCtrlBaseName();   // m_ctrlBaseName

		// This functions serves 2 purposes: It shows components of a lump and it differenciates containers.
		virtual std::string getLumpComponent();

		virtual bool initReadLogThread();
		virtual void stopReadLogThread();

		bool initCatchUpThread();
		void stopCatchUpThread();

		void monitorLogForRename(std::string ctrlFileName);
		void execMonitorFileForRename(std::string logFilePath, std::string ctrlFileName);

		bool fileHasBackLog();

		StatusVal getObjStatus();

		const std::filesystem::path      getLogFilePath() const;
		const std::string               &getThreadName();
		const std::string               &getDateExtract(DATE_EXTRACT key);
		int                              getRegexpCount();
		char                             getRegexpSvrt(int rgxpThreadID);
		const std::string               &getRegexpText(int rgxpThreadID);

		bool                            getNextLogLine(std::string &line);
		bool                            getCatchupLine(std::string &line);
		void                            writePosOfNextLogline2Read();
		void                            writePosOfNextCatchup2Read();
		bool                            eofLiveLog();
		bool                            eofCatchUp();

		void                             inotifyLogfile(std::filesystem::path logFilePath);
		void                             execInotifyLogfileThread(std::filesystem::path logFilePath/*, boost::promise<int> &prom*/);

	protected:
		StatusVal          m_state;
	private:
		// When inotifyLogfile gets called it starts an execInotifyLogfileThread to detect lines being added to the file.
		// That thread locks on the inotify "read".
		// We save thread->native_handle() in m_logReadingThreadID so we can SIGUSR1 it when monitorLogForRename
		// detects that either the first line of the file changed or the file was renamed.
		// In such case we need to restart logReadingThread (and maybe catchUpLogThread) and the inotify "read" is holding the thread.
		// If execInotifyLogfileThread returns because a line was added then m_logReadingThreadID is set to 0.
		boost::thread::native_handle_type      m_logReadingThreadID;
		mutable boost::mutex                   m_logReadingThreadID_Mutex;


		// Reading file control stuff...
		ReadFileCtrl                    m_readCtrlLiveLog;
		std::string                     m_ctrlBaseName;


		// Everything else....
		std::filesystem::path          m_logFilePath;
		std::string                    m_threadName;
		std::map<char, std::string>    m_dateExtrct;
		std::vector<char>              m_regexpSvrt;
		std::vector<std::string>       m_regexpText;
};

#endif // CONF_CONTAINER_H
/*END*/
