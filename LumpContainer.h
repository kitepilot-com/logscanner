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
		void    monitorComponentsForRename(std::string lumpComponent, std::string &ctrlFilePath);
		void    feedLumpLogFile(std::string lumpComponent, std::atomic<std::size_t> &filesSeenCnt);
		void    writeLineToLumpFile(std::string line, std::filesystem::path logFilePath);
		void    inotifyLumpfile(std::filesystem::path logFilePath);
	private:
		// Result of initReadLogThread...
//		std::atomic<std::size_t>   m_resultCnt;
		std::atomic<bool>          m_resultFlag;
		boost::condition_variable  m_condVar;

		// This vector holds the pointers to the feedLumpLogFile threads created by initReadLogThread.
		// They ar joined at stopReadLogThread()
		std::vector<std::shared_ptr<boost::thread>> m_allThreads;

		// std::set to store lump components names.
		// The iterator is used to return one name at a time in getLumpComponent.
		std::set<std::string>            m_logList;
		std::set<std::string>::iterator  m_logListIter;

		// This member stores the thread ID we need to kill this thread as part of a restart.
		std::map<std::string, boost::thread::native_handle_type>   m_logReadingThreadIdList;
		mutable boost::mutex                                       m_logReadingThreadIdListMutex;
//		std::atomic<bool>                                          m_killAllThreads;

		// This mutex control writes to the LUMP file.
		mutable boost::mutex       m_writeMutex;
};

#endif // LUMP_CONTAINER_H
/*END*/
