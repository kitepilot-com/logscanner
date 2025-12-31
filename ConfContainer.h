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

#include "INotifyObj.h"

class ConfContainer
{
	public:
		// This flag is set to "true" to restart the reading thread of *THIS* logfile.
		std::atomic<bool>   RESTART_THREAD;

		typedef enum {
			INVALID = -1,
			READLOGS_ERR,
			FEED_ERR,
			LUMP_ERR,
			READDIR_ERR,
			LUMFLAG_ERR,
			HEALTHY
		} StatusVal;

		typedef enum {
			REGEXP = 'R',
			BLDMAP = 'M',
			FORMAT = 'F'
		} DATE_EXTRACT;

		static constexpr int  m_FIRST_LINE_MONITOR_INTERVAL = 100; // milliseconds...

		ConfContainer(std::string logFilePath, std::map<char, std::string> dateExtrct, std::map<std::string, char> regexpList);
		virtual ~ConfContainer();

		INotifyObj::Outcome monitorNewLineThread(std::string logFilePath);
		INotifyObj::Outcome monitor4RenameThread(std::string logFilePath);

		// This functions serves 2 purposes: It shows components of a lump and it differenciates containers.
		virtual std::string getLumpComponent();

		virtual bool initReadLogThread();
		virtual void stopReadLogThread();

		bool      initCatchUpThread();
		void      stopCatchUpThread();
		bool      fileHasBackLog();
		StatusVal getObjStatus();

		const std::filesystem::path  getLogFilePath() const;
		const std::string           &getThreadName();
		const std::string           &getDateExtract(DATE_EXTRACT key);
		int                          getRegexpCount();
		char                         getRegexpSvrt(int rgxpThreadID);
		const std::string           &getRegexpText(int rgxpThreadID);

		bool                         getNextLogLine(std::string &line);
		bool                         getCatchupLine(std::string &line);
		void                         writePosOfNextLogline2Read();
		void                         writePosOfNextCatchup2Read();
		bool                         eofLiveLog();
		bool                         eofCatchUp();

		void        waitForFirstLineThread(std::string logFilePath, std::filesystem::path ctrlFileName);
	protected:
		static constexpr char m_FSTLINEOFLOGFILE_CTRL[] = "/fstLineOfLogfile.ctrl";
		static constexpr char m_FILEORSEGMENTEOF_CTRL[] = "/fileOrSegmentEOF.ctrl";
		static constexpr char m_NEXTLOGLINE2READ_CTRL[] = "/nextLogline2Read.ctrl";
		static constexpr char m_NEXTCATCHUP2READ_CTRL[] = "/nextCatchup2Read.ctrl";

		StatusVal          m_state;

		std::string getCtrlBaseName();   // m_ctrlBaseName
		void        ValidateAndUpdateFirstLineOf(std::string logFilePath, std::filesystem::path ctrlFileName);
		bool        refreshCtrlValOf(std::streampos &ctrlVal);
		bool        setCtrlValOf(std::string ctrlFile, std::streampos &ctrlVal);
		bool        getCtrlValOf(std::string ctrlFile, std::streampos *ctrlVal = nullptr);

		virtual void  StartWaitForFirstLineThread(std::string logFilePath, std::filesystem::path ctrlFileName);
		virtual void  removeAllControlFiles(std::string logFilePath);
	private:
//FIXTHIS!!! COMMENT!
#pragma message("//FIXTHIS!!! COMMENT!.")
		// When inotifyLogfile gets called it starts an execInotifyLogfileThread to detect lines being added to the file.
		// That thread locks on the inotify "read".
		// We save thread->native_handle() in m_logReadingThreadID so we can SIGUSR1 it when monitorLogfileForRename
		// detects that either the first line of the file changed or the file was renamed.
		// In such case we need to restart logReadingThread (and maybe catchUpLogThread) and the inotify "read" is holding the thread.
		// If execInotifyLogfileThread returns because a line was added then m_logReadingThreadID is set to 0.
//		boost::thread::native_handle_type      m_logReadingThreadID;
//		mutable boost::mutex                   m_logReadingThreadID_Mutex;

		// Reading file control stuff...
		// Control files names.
		static constexpr char m_CONTROL_FILES_PATH[] = "run/ctrl";

		static std::string m_TIMESTAMP_FILES_REGEX;

		// Save pointers for threads that may be concurrent.
		std::shared_ptr<boost::thread> m_monitorNewLineThread;

		std::set<std::string> m_filestampFileList;

		boost::mutex    m_writeMutexNextLogline;
		std::streampos  m_lastLoglinePosWritten;
		boost::mutex    m_writeMutexNextCatchup;
		std::streampos  m_lastCatchupPosWritten;

		// Reading file control stuff...
		std::ifstream   m_ifsReadLog;
		std::ifstream   m_ifsCatchUp;
		std::string     m_ctrlBaseName;
		std::streampos  m_fileOrSegmentEOF;
		std::streampos  m_nextLogline2Read;
		std::streampos  m_nextCatchup2Read;
		bool            m_doneCatchingUp;

		// Everything else....
		std::filesystem::path          m_logFilePath;
		std::string                    m_threadName;
		std::map<char, std::string>    m_dateExtrct;
		std::vector<char>              m_regexpSvrt;
		std::vector<std::string>       m_regexpText;
};

#endif // CONF_CONTAINER_H
/*END*/
