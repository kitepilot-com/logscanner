/*******************************************************************************
 * File: logscanner/LogScanner.h                                               *
 * Created:     Jan-07-25                                                      *
 * Last update: May-17-25                                                      *
 * Top level header file.                                                      *
 *******************************************************************************/

#ifndef LOGSCANNER_H
#define LOGSCANNER_H

#include <sys/un.h>
#include <filesystem>
#include <vector>
#include <set>
#include <map>
#include <mariadb/conncpp.hpp>
#include <boost/thread/barrier.hpp>

#include "ConfContainer.h"
#include "Workarea.h"
#include "NotifyIPC.h"
//#include <boost/interprocess/sync/named_semaphore.hpp>

class ConveyorBelt;
class LogScanner
{
	public:
		static std::atomic<bool>   KEEP_LOOPING;

		enum StatusVal {
			INVALID = -1,
			SIGNAL_ERR,
			CONFIG_ERR,
			CONN_ERR,
			CONTNER_ERR,
			CRASH_ERR,
			CONTENT_ERR,
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
			TOOSHORT_FEW,
			WHITE_ERR,
			SMPHR_ERR,
			HEALTHY
		};

		LogScanner();
		virtual ~LogScanner();

		int exec();

		// This could be arguments...  vvvvvvvvvvvvvvvvvvvv
		static constexpr char m_CONF_PATH[] = "config/logfiles";
		static constexpr char m_EVAL_PATH[] = "config/evalregexps.conf";
		static constexpr char m_FILE_TYPE[] = ".conf";
		static constexpr int  m_totThreads  = 5;
		static constexpr int  m_MULTIPLIER  = 5;                // Multiply m_totThreads * m_MULTIPLIER in catchUpLogThread to speed up catch up...
		static constexpr int  m_MAX_BACKLOG = 1024 * 1024 * 10; // Max byte offset to allow before triggering catchUpLogThread.
		// This could be arguments...  ^^^^^^^^^^^^^^^^^^^^
	protected:
	private:
		void        loadDroppedIPs();

		void        uploadConfiguration(std::filesystem::path confPath = m_CONF_PATH);
		void        uploadEvalRegexps(std::filesystem::path confPath = m_EVAL_PATH);

		int         readDirectoryTree(std::filesystem::path confPath, std::set<std::filesystem::path> &sortedTree, std::set<std::filesystem::path> &emptyDirs);
		void        loadAndParseConfFiles(std::set<std::filesystem::path> &sortedTree, std::set<std::filesystem::path> &emptyDirs);
		void        parseConfOf(std::filesystem::path confPath);
		void        uploadWhitelistedIPs();

		bool        initCommServer(int &fdSocketSrv, int &fdAccept, const char* socketPath, int maxConnections);
		bool        initDB();

		void        commThread();
		void        intfThread();
		void        SQL_Thread();
		void        inptThread();
		void        evalThread();
		void        dropThread();

		void        execSqlInsert(std::string tableName, NotifyIPC *notifyIPC);

		void        logReadingThread(ConfContainer *logfileData);
		void        catchUpLogThread(ConfContainer *logfileData);
		void        processingThread(ConfContainer *logfileData, const char *caller, ConveyorBelt *conveyorBelt, int procThreadID, void (ConfContainer::*ptr_writePosOfNextline2Read)());
		void        regexpExecThread(ConfContainer *logfileData, const char *caller, Workarea *threadWkArea, std::string *linePtr, boost::barrier &barrier, int rgxpThreadID);

		void        monitorNewLineThread(ConfContainer *logfileData);
		void        monitor4RenameThread(ConfContainer *logfileData);

		bool        chkEvalRegexps(std::array<std::string, 4> &IPaddr);

		std::string lineTimestamp2Seconds(ConfContainer *logfileData, const std::string &line);
		std::string createTimestamp();

		bool        hasIPaddress(std::string &line);
		bool        checkForWhitelist(const std::string &IPaddr);

		// The "insertInto.*Table" functions below don't really "insert" anything using SQL.
		// Multiple concurrent SQL Inserts are a bottleneck and randomly "deadlock".
		// In order to avoid any kind of time-consuming synchronization primitive,
		// the design choice was to create text files with SQL statements to be read and executed later by another thread.
		// The reason is that any number of text files can be created concurrently with no "lock" required,
		// and then the SQL inserts can be executed sequentially without "deadlocks".
		// The tormat of the file is:
		// Line one is the SQL statement.
		// Other lines are values to be used n "prepere stateent".
		void        insertIntoSeenTable(ConfContainer *logfileData, std::string &line, bool regexMatch);
		void        insertIntoAudtTable(ConfContainer *logfileData, Workarea *threadWkArea, std::string *linePtr, int rgxpThreadID);
		void        insertIntoEvalTable(ConfContainer *logfileData, Workarea *threadWkArea, std::string *linePtr, int rgxpThreadID);
		void        insertIntoHistTable(ConfContainer *logfileData, Workarea *threadWkArea, std::string *linePtr, int rgxpThreadID);
		void        insertIntoDropTable(std::array<std::string, 4> &ipAddr, const char *ProcName, std::string *lineOrTmstmp, char Status = 'N');

		void        execBatchSQL(std::string &sql2go);

		// Kindda utility functions below...
		sql::Connection *connectToDB();
		void             storeIPaddrInArray(const std::string &IPaddr, std::array<std::string, 4> &IPaArr);
		int              fullTrimmedLine(std::string &line, char **fistChar, char **lastChar);
//		std::string      escapeString(std::string srcstr);

		// This could be arguments...  vvvvvvvvvvvvvvvvvvvv
		static constexpr char m_DB_URL[]  = "jdbc:mariadb://localhost:3306/logscanner";
		static constexpr char m_DB_USER[] = "logscanner_user";
		static constexpr char m_DB_PSWD[] = "USER-PASSWORD-HERE";
		// This could be arguments...  ^^^^^^^^^^^^^^^^^^^^

		// IPC
		std::map<std::string, std::shared_ptr<NotifyIPC>>  m_NotifyIPC_list;
		static constexpr char                              m_SQL_IPC_PATH[] = "run/sql";
		static std::string                                 m_BASE_SEMAPHORE_NAME;

		// DB Connection and SQL.
		static std::string    m_QUOTE;
		static constexpr int  m_MAX_CONN_FAILURES_TO_QUIT = 100;
		static constexpr int  m_MYSQL_MAX_INDEX_LENGTH    = (1024 * 3);
		sql::SQLString        m_url;
		sql::Properties       m_properties;
		sql::Driver          *m_driver;

		// Eval...
		// Max longevity of rows in the eval table.
		static constexpr int m_MAX_SECONDS_TO_KEEP = 600;  //FIXTHIS!!!   This should be in a conf file.

		// Highest row count to evaluate possible atack.
		static constexpr int m_MAX_HITCOUNT_TO_CHK = 10;   //FIXTHIS!!!   This should be in a conf file.

		std::map<std::pair<std::string, std::string>, int>  m_evalRegexps;

		// Comm server
		static constexpr char m_SYS_SOCKET[] = "_sys_comm_socket";
		static constexpr int  m_SYS_CONNECTS = 1;
		int                   m_fdSocketSrv;
		int                   m_fdAccept;

		// Sys status...
		int       m_confCount = 0;
		StatusVal m_state     = INVALID;

		// There is a ConfContainer for every log file being read.
		// ConfContainers for LUMP files also contain the list of files to LUMP together.
		// See logscanner/LumpContainer.h
		std::vector<std::shared_ptr<ConfContainer>>  m_ConfContainerList;
		std::set<std::string>                        m_inotifyTargetPaths;

		// This map contains the threads IDs to "kill" when restarting the thread.
		// See /LogScanner::monitorFirstLine
//		std::vector<boost::thread::native_handle_type>       m_logReadinghreadList;

		// Whitelisted IPs, IP pattern and descriptionm.
		std::map<std::string, std::string>  m_whitelist;

		// IPC...
		NotifyIPC m_notifyEval;
		NotifyIPC m_notifyDrop;
};
#endif // LOGSCANNER_H
/*END*/
