/*******************************************************************************
 * File: logscanner/LogScanner.h                                               *
 * Created:     Jan-07-25                                                      *
 * Last update: May-17-25                                                      *
 * Top level header file.                                                      *
 *******************************************************************************/

#ifndef LOGSCANNER_H
#define LOGSCANNER_H
/*
#include <sys/un.h>
#include <vector>
#include <set>
#include <map>
#include <mariadb/conncpp.hpp>

#include "ConfContainer.h"
#include "Workarea.h"
#include "NotifyIPC.h"
#include <boost/thread/barrier.hpp>
//#include <boost/interprocess/sync/named_semaphore.hpp>
*/
#include <boost/thread.hpp>
#include <filesystem>

class ConfContainer
{
	public:
		ConfContainer();
		~ConfContainer();

		const std::filesystem::path      getLogFilePath() const;
		const std::string               &getThreadName();

		void                             deliverToProcThreads(std::string &line);
		void                             acceptFromReadThread(std::string &line);

	protected:
	private:
		mutable boost::mutex      m_lineExchangeMutex;
		boost::condition_variable m_deliverConditionVariable;
		boost::condition_variable m_acceptLConditionVariable;
		bool                      m_gotLineFrmLog;
		bool                      m_lineDelivered;

		std::string                    m_lineBuffer;
		std::string                    m_threadName = "getThreadName";
		std::filesystem::path          m_logFilePath;
};

class LogScanner
{
	public:
		LogScanner();
		virtual ~LogScanner();

		int exec();
	protected:
	private:
		bool        initCommServer(int &fdSocketSrv, int &fdAccept, const char* socketPath, int maxConnections);

		void        commThread();
		void        inptThread();

		void        logReadingThread(ConfContainer *logfileData);
		void        lumpReadingThread(boost::mutex *writeMutex, std::filesystem::path logFilePath, std::string lumpCompenent, boost::atomic<bool> &restartThread);

		std::string createTimestamp();

		void             inotifyLogfile(std::filesystem::path logFilePath);

		static boost::atomic<bool>   KEEP_LOOPING;

		static constexpr int  m_totThreads  = 5;

		static constexpr char m_SYS_SOCKET[] = "_sys_comm_socket";
		static constexpr int  m_SYS_CONNECTS = 1;
		int                   m_fdSocketSrv;
		int                   m_fdAccept;

		std::vector<ConfContainer *>  m_logfileList;
};

#endif // LOGSCANNER_H
/*END*/
