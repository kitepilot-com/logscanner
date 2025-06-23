/*******************************************************************************
 * File: logscanner/LogScanner.h                                               *
 * Created:     Jan-07-25                                                      *
 * Last update: May-17-25                                                      *
 * Top level header file.                                                      *
 *******************************************************************************/

#ifndef LOG_SCANNER_H
#define LOG_SCANNER_H

#include <sys/un.h>
#include <filesystem>
#include <vector>
#include <set>
#include <mariadb/conncpp.hpp>

#include "ConfContainer.h"
#include "Workarea.h"
#include <boost/thread/barrier.hpp>
#include <boost/interprocess/sync/named_semaphore.hpp>

class LogScanner
{
	public:
		enum StatusVal {
			INVALID = -1,
			CONFIG_ERR,
			CONN_ERR,
			CONTNER_ERR,
			CRASH_ERR,
			DATEEXT_ERR,
			D_BASE_ERR,
			DUPDTINFO_ERR,
			DUPRGXP_ERR,
			EMPTY_ERR,
			EXIST_ERR,
			FILE_ERR,
			FILETYPE_ERR,
			INIT_ERR,
			NORXGP_ERR,
			OPEN_ERR,
			STRUCT_ERR,
			TOOSHORT_ERR,
			WHITE_ERR,
			SMPHR_ERR,
			HEALTHY
		};

		LogScanner();
		virtual ~LogScanner();

		int exec();
	protected:
	private:
		void        uploadConfiguration(std::filesystem::path confPath = m_BASE_PATH);
		bool        initDB();
		int         readDirectoryTree(std::filesystem::path confPath, std::set<std::filesystem::path> &sortedTree, std::set<std::filesystem::path> &emptyDirs);
		void        loadAndParseConfFiles(std::set<std::filesystem::path> &sortedTree, std::set<std::filesystem::path> &emptyDirs);
		void        parseConfOf(std::filesystem::path confPath);
		void        uploadWhitelistedIPs();

		void        commThread();
		void        inptThread();
		void        evalThread();
		void        dropThread();

		void        readingThread(ConfContainer *logfileData);
		void        processingThread(ConfContainer *logfileData, int procThreadID);
		void        regexpExecThread(std::string *linePtr, Workarea *threadWkArea, boost::barrier &barrier, int rgxpThreadID);

		void        lumpReadingThread(std::filesystem::path logFilePath, std::string lumpCompenent);
		void        writeLineToLumpFile(boost::mutex *writeMutex, std::string line, std::filesystem::path &logFilePath);

		bool        createServer(int &fdSocketSrv, int &fdAccept, const char* socketPath, int maxConnections);

		std::string lineTimestamp2Seconds(ConfContainer *logfileData, const std::string &line);
		std::string createTimestamp();

		bool        hasIPaddress(std::string line);
		bool        notEverSeen(std::string line);
		bool        checkForWhitelist(Workarea *threadWkArea);
		void        insertIntoSeenTable(ConfContainer *logfileData, std::string &line);
		void        insertIntoAditTable(ConfContainer *logfileData, Workarea *threadWkArea, std::string &line, int rgxpThreadID);
		void        insertIntoEvalTable(ConfContainer *logfileData, Workarea *threadWkArea, std::string &line, int rgxpThreadID);
		void        insertIntoHistTable(ConfContainer *logfileData, Workarea *threadWkArea, std::string &line, int rgxpThreadID);
		void        insertIntoDropTable(std::array<std::string, 4> &ipAddr, char Status = 'N');

		std::string writeInsertDropFor(std::array<std::string, 4> &ipAddr, char Status = 'N');
		void        execBatchSQL(std::string &sql2go);

		void        notifyDrop();
		void        wait4NotifyDrop();
		void        setNotifyDrop(bool flag = false);

		void        notifyEval();
		void        wait4NotifyEval();
		void        setNotifyEval(bool flag = false);

		// Kindda utility functions below...
		sql::Connection *connectToDB();
		void             storeIPaddrInArray(std::string &IPaddr, std::array<std::string, 4> &IPaArr);
		int              fullTrimmedLine(std::string &line, char **fistChar, char **lastChar);
		std::string      escapeString(std::string srcstr);
		void             inotifyLogfile(std::filesystem::path logFilePath);

		static bool  KEEP_LOOPING;

		// This could be arguments...  vvvvvvvvvvvvvvvvvvvv
		static constexpr char m_BASE_PATH[] = "config";
		static constexpr char m_FILE_TYPE[] = ".conf";
		static constexpr char m_IP_REGEXP[] = "[^[:digit:]]*([[:digit:]]{1,3}\\.[[:digit:]]{1,3}\\.[[:digit:]]{1,3}\\.[[:digit:]]{1,3})[^[:digit:]]";

		static constexpr int  m_totThreads  = 5;
		static constexpr int  m_pendingDrp  = 100;
		static constexpr int  m_pendingAdt  = 100;

		// mysql -u logscanner_user -p logscanner #  "U5erP455W0rd!"
		static constexpr char m_DB_URL[]  = "jdbc:mariadb://localhost:3306/logscanner";
		static constexpr char m_DB_USER[] = "logscanner_user";
		static constexpr char m_DB_PSWD[] = "U5erP455W0rd!";
		// This could be arguments...  ^^^^^^^^^^^^^^^^^^^^

		// DB Connection and SQL.
		static           std::string m_QUOTE;
		static constexpr int         m_MAX_CONN_FAILURES_TO_QUIT = 100;
		static constexpr int         m_MYSQL_MAX_INDEX_LENGTH    = (1024 * 3);
		sql::SQLString   m_url;
		sql::Properties  m_properties;
		sql::Driver     *m_driver;

		// Eval...
		static constexpr unsigned int m_MAX_SECONDS_TO_KEEP = 10 ;  //FIXTHIS!!!   This should be in a conf file.
		static constexpr float        m_MAX_REQUEST_X_SEC   = 3.0f; //FIXTHIS!!!   This should be in a conf file.

		// Comm server
		static constexpr char m_SYS_SOCKET[] = "_sys_comm_socket";
		static constexpr int  m_SYS_CONNECTS = 1;
		int                   m_fdSocketSrv;
		int                   m_fdAccept;

		// semaphores...
		static constexpr char                 m_NOTIFY_EVAL[] = "logscanner_NOTIFY_EVAL";
		static constexpr char                 m_NOTIFY_DROP[] = "logscanner_NOTIFY_DROP";
		boost::interprocess::named_semaphore *m_evalsmphr;
		boost::interprocess::named_semaphore *m_dropSmphr;

		// Sys status...
		StatusVal m_state     = INVALID;
		int       m_confCount = 0;

		mutable boost::mutex  m_mutex4sql;
		mutable boost::mutex  m_mutex4Timestamp;
		mutable boost::mutex  m_mutex4NotifyDrop;
		mutable boost::mutex  m_mutex4NotifyEval;

		// There is a ConfContainer for every log file read.
		std::vector<ConfContainer *>  m_logfileList;

//		// This pointer works with void LogScanner::notifyInput()
//		void (LogScanner::* m_pushQueuePtr)();

		// Whitelisted IPs, IP pattern and descriptionm.
		std::map<std::string, std::string>  m_whitelist;

		// Flag to notify eval thread that the input table has new rows.
		bool                                m_notifyEval;

		// Flag to notify drop thread that the eval thread added rows to the drop table.
		bool                                m_notifyDrop;
};

#endif // LOG_SCANNER_H
/*END*/
