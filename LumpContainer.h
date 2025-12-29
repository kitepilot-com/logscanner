/*******************************************************************************
 * File: logscanner/LumpContainer.h                                            *
 * Created:     Apr-14-25                                                      *
 * Last update: Apr-14-25                                                      *
 * Top level header file.                                                      *
 *                                                                             *
 * This class implements the LUMP logfile type.                                *
 * A LumpContainer consolidates a group of similar logfiles (like apache's)    *
 * into a single logfile that is read by the scanner.                          *
 *******************************************************************************/

#ifndef LUMP_CONTAINER_H
#define LUMP_CONTAINER_H

#include <set>
#include <string>

#include "ConfContainer.h"

class LumpContainer : public ConfContainer
{
	public:
		static constexpr char m_LUMP[] = "LUMP";
		static constexpr int  m_SIZE   = strlen(m_LUMP);

		LumpContainer(std::string logFilePath, std::map<char, std::string> extrctDate, std::map<std::string, char> regexpList);
		virtual ~LumpContainer();

		// This functions serves 2 purposes: It shows components of a lump and it differenciates containers.
		virtual std::string  getLumpComponent();

		virtual bool initReadLogThread();
		virtual void stopReadLogThread();
	protected:
		void    feedLumpLogFile(std::string lumpComponent, std::atomic<std::size_t> &filesSeenCnt);
		void    writeLineToLumpFile(std::string line, std::filesystem::path logFilePath);
//		void    inotifyLumpfile(std::filesystem::path logFilePath);

		virtual void  StartWaitForFirstLineThread(std::string logFilePath, std::filesystem::path ctrlFileName);
		virtual void  removeAllControlFiles(std::string logFilePath);
	private:

		// Members below are used to control the result of initReadLogThread...
		// initReadLogThread starts a feedLumpLogFile for every compnent of the LUMP object,
		// initReadLogThread returns true when all the components have been propperly opened.
		// After all feedLumpLogFile have been started a loop 'while(filesSeenCnt.load() < m_logList.size())'
		// uses m_condVar to wait for all the results.
		std::atomic<bool>          m_resultFlag;
		boost::condition_variable  m_condVar;

		// This vector holds the pointers to the feedLumpLogFile threads created by initReadLogThread.
		// They ar joined at stopReadLogThread()
		std::vector<std::shared_ptr<boost::thread>> m_allFeedLumpLogFileThreads;

		// std::set to store lump components names.
		// The iterator is used to return one name at a time in getLumpComponent.
		// This list also used to trigger the exit of all files from INotifyObj::intfThread
		// if any component or the LUMP file itself is renamed bt logrotate.
		std::set<std::string>            m_logList;
		std::set<std::string>::iterator  m_logListIter;

		// Save list of watched files so we can release them in bulk.
		std::vector<std::shared_ptr<boost::thread>> m_allMonitorThreads;

		// This member stores the thread ID we need to kill this thread as part of a restart.
//		std::map<std::string, boost::thread::native_handle_type>   m_logReadingThreadIdList;
		mutable boost::mutex                                       m_logReadingThreadIdListMutex;

		// This mutex is used to manage writes to the LUMP file in writeLineToLumpFile.
		mutable boost::mutex       m_writeMutex;
};

#endif // LUMP_CONTAINER_H
/*END*/
