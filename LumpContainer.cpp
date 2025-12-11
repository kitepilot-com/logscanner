/*******************************************************************************
 * File: logScanner/LumpContainer.cpp                                          *
 * Created:     Apr-14-25                                                      *
 * Last update: Apr-14-25                                                      *
 * Top level program file.                                                     *
 *******************************************************************************/

#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION

#include <fstream>
#include <iostream>
#include <signal.h>
#include <boost/regex.hpp>
#include <boost/algorithm/string/replace.hpp>

#include "LogManager.h"
#include "LogScanner.h"
#include "LumpContainer.h"

LumpContainer::LumpContainer(std::string logFilePath, std::map<char, std::string> extrctDate, std::map<std::string, char> regexpList) :
	ConfContainer(logFilePath, extrctDate, regexpList),
//	m_resultCnt(0),
	m_resultFlag(true)
{
	// This function will load the names of all the logfiles
	// to read for feeding the LUMP file.
	// It will create the LUMP file if non existing..
	m_state = ConfContainer::INVALID;

	std::string logname(getLogFilePath().string());

	// Let's get the directory of the logfile and build a rexexp to identify the components.
	// strPath is the path to the log and strRexp is the logfile name.
	// We will replace "LUMP" with "*" in strRexp to create a regex to match the compoonent's of the LUMP.
	std::size_t found = logname.find_last_of("/");
	std::string strPath(logname.substr(0, found));
	std::string strRexp(logname.substr(found + 1));

	// Validate that we have LUMP in the log name,
	size_t start_pos = strRexp.find(m_LUMP);
	if(start_pos != std::string::npos)
	{
		// Escape the dots => "." to "\\."
		// Replace LUMP with ".*" and add '$'
		// In the case of apache the conversion of "access-LUMP.log"
		// will yield "access-.*\.log$"
		boost::replace_all(strRexp, ".", "\\.");
		strRexp.replace(start_pos, m_SIZE, ".*");
		strRexp += "$";

		LogManager::getInstance()->consoleMsg();
		LogManager::getInstance()->consoleMsg(("\tLooking for files named as '" + strRexp + "' in directory '" + strPath + "'").c_str());

		boost::match_results<std::string::const_iterator> result;
		boost::regex                                      regexp(strRexp);
		const std::filesystem::path                       FsPath{strPath};

		try
		{
			int logCount = 0;

			// Read the directory where the LUMP's components are.
			for (auto const& dir_entry : std::filesystem::directory_iterator{FsPath})
			{
				if (std::filesystem::is_regular_file(dir_entry.path()))
				{
					// Filter the files that match our mask.
					if(boost::regex_search(dir_entry.path().string(), result, regexp))
					{
						// The regexp will pick up the LUMP file, exclude it!
						start_pos = dir_entry.path().filename().string().find(m_LUMP);
						if(start_pos == std::string::npos)
						{
							std::string   file2test(FsPath.string() + "/" + dir_entry.path().filename().string());
							std::ifstream readTest(file2test);

							if(readTest.is_open())
							{
								readTest.close();
								m_logList.insert(file2test);
							}
							else
							{
								LogManager::getInstance()->consoleMsg(("*** FATAL *** Unable to read file " + file2test).c_str());
								m_state = ConfContainer::READLOGS_ERR;
							}

							logCount++;
						}
					}
				}
			}

			if(logCount > 0)
			{
				LogManager::getInstance()->consoleMsg(("\t" + std::to_string(logCount) + " files will write to " + logname + "'").c_str());
				LogManager::getInstance()->consoleMsg(("\tCreating or truncating special file: '" + logname + "'").c_str());
//				LogManager::getInstance()->consoleMsg();

				std::ofstream  myfile;
				myfile.open(logname.data(), std::ios_base::out | std::ios_base::app);
				if(myfile.is_open())
				{
					myfile.close();

					if(m_state == INVALID)
					{
						m_state = ConfContainer::HEALTHY;
					}
				}
				else
				{
					LogManager::getInstance()->consoleMsg(("*** FATAL *** => unable to create/access '" + logname + "'").c_str());
					m_state = ConfContainer::FEED_ERR;
				}

			}
			else
			{
				LogManager::getInstance()->consoleMsg(("*** FATAL *** => No logs found to feed '" + logname + "'").c_str());
				m_state = ConfContainer::LUMP_ERR;
			}
		}
		catch (std::filesystem::filesystem_error const& ex)
		{
			LogManager::getInstance()->consoleMsg(("*** FATAL *** => Unable to read directory '" + strPath + "'").c_str());
			m_state = ConfContainer::READDIR_ERR;
		}
	}
	else
	{
		LogManager::getInstance()->consoleMsg(("I can't find string '" + std::string(m_LUMP) + "' in <" + strPath + ">").c_str());
		m_state = ConfContainer::LUMFLAG_ERR;
	}

	// We need to be at the begining for getLumpComponent() to work.
	m_logListIter = m_logList.begin();
}

LumpContainer::~LumpContainer()
{
}

std::string LumpContainer::getLumpComponent()
{
	std::string   log2go;

	if(m_logListIter == m_logList.end())
	{
		m_logListIter = m_logList.begin();
	}
	else
	{
		log2go = *m_logListIter;
		m_logListIter++;
	}

	return log2go;
}

bool LumpContainer::initReadLogThread()
{
	//	This function initializes the "components" of a "lump(ed) container".
	//	It insures that all the components are readable (or not) upon exit.
	//	A LumpContainer consolidates a group of similar logfiles (like apache's)
	//	into a single logfile that is read by the scanner.
	bool allGood = ConfContainer::initReadLogThread();

	if(allGood)
	{
		std::atomic<std::size_t>  filesSeenCnt(0);
		boost::mutex              localMutex;

		for(auto lumpComponent : m_logList)
		{
			// Start feeding components to the LUMP logfile.
			m_allThreads.push_back(std::make_shared<boost::thread>(&LumpContainer::feedLumpLogFile, this, lumpComponent, boost::ref(filesSeenCnt)));
		}

		// Wait until all files are open (or not), if any fails to open m_resultFlag is set to false.
		boost::unique_lock<boost::mutex> lock(localMutex);
		while(filesSeenCnt.load() < m_logList.size())
		{
			m_condVar.wait(lock);
		}
	}

	return allGood && m_resultFlag;
}

void LumpContainer::stopReadLogThread()
{
	// m_allThreads holds the pointers for all LumpContainer::feedLumpLogFile executing.
	for(auto thread : m_allThreads)
	{
		thread->join();
	}

	ConfContainer::stopReadLogThread();
}

void LumpContainer::monitorComponentsForRename(std::string lumpComponent, std::string &ctrlFilePath)
{
	execMonitorFileForRename(lumpComponent, ctrlFilePath + "/fstline.txt");

	m_logReadingThreadIdListMutex.lock();
	std::map<std::string, boost::thread::native_handle_type>::iterator thread2kill = m_logReadingThreadIdList.find(lumpComponent);

	if(thread2kill != m_logReadingThreadIdList.end())
	{
		LogManager::getInstance()->consoleMsg(("===>  monitorComponentsForRename sending SIGUSR1 for " + thread2kill->first).c_str());
		pthread_kill(thread2kill->second, SIGUSR1);
	}
	m_logReadingThreadIdListMutex.unlock();
}

void LumpContainer::feedLumpLogFile(std::string lumpComponent, std::atomic<std::size_t> &filesSeenCnt)
{
	// This function opens the components to read lines.
	// Those lines are unconditionally appended to the "LUMP" file.
	// This function returns "true" if (and only if) all the components have been opened.
	LogManager::getInstance()->consoleMsg((">>>>> STARTING feedLumpLogFile for " + getLogFilePath().string() + " => " + lumpComponent).c_str());

	// Increase filesSeenCnt so we can release lock in LumpContainer::initReadLogThread()
	++filesSeenCnt;

	std::size_t found = lumpComponent.find_last_of("/");
	std::string ctrlFilePath(getCtrlBaseName() + "/" + lumpComponent.substr(found + 1));
	std::string ctrlFileName(ctrlFilePath + "/nextLogline2Read.ctrl");

	std::filesystem::create_directories(ctrlFilePath);

	std::ifstream ifs(lumpComponent, std::ios::in);
	if(ifs.is_open())
	{
		// Start monitor for name change.
		std::shared_ptr<boost::thread> monThread(std::make_shared<boost::thread>(boost::thread(&LumpContainer::monitorComponentsForRename, this, lumpComponent, ctrlFilePath)));

		// Does ctrlFile file exist?
		std::ifstream ctrlifs(ctrlFileName, std::ios::in);
		if(ctrlifs.is_open())
		{
			// Get last file offset.
			std::string line;
			std::getline(ifs, line);
			ifs.close();

			ifs.seekg(atoi(line.c_str()), ifs.beg);
		}

		// Notify initReadLogThread that this file has been counted.
		m_condVar.notify_one();
		while(LogScanner::KEEP_LOOPING.load() && !RESTART_THREAD.load())
		{
			// Read incoming line...
			std::string line;
			while(LogScanner::KEEP_LOOPING.load() && !RESTART_THREAD.load() && std::getline(ifs, line))
			{
				if(!line.empty())
				{
					// The space between the quotes below is a *REAL* TAB character.
					// For some reason '\t' will appear as 't' in the target file.
					// Go figure...
					writeLineToLumpFile(lumpComponent + '	' + line, getLogFilePath());

					std::ofstream  ofs(ctrlFileName, std::ios_base::trunc);
					ofs << ifs.tellg() << std::endl;
					ofs.close();
				}
			}

			if(LogScanner::KEEP_LOOPING.load() && !RESTART_THREAD.load() && ifs.eof())
			{
				ifs.clear();
				inotifyLumpfile(lumpComponent);
			}
		}

		ifs.close();

		monThread->join();
	}
	else
	{
		LogManager::getInstance()->consoleMsg(("*** FATAL *** unable to open component " + lumpComponent).c_str());

		// Set initReadLogThread to fail because we can't open this file and...
		m_resultFlag.store(false);

		// Notify initReadLogThread that this file has been counted.
		m_condVar.notify_one();
	}

	LogManager::getInstance()->consoleMsg((">>>>> EXITING feedLumpLogFile for " + getLogFilePath().string() + " => " + lumpComponent).c_str());
}

void LumpContainer::writeLineToLumpFile(std::string line, std::filesystem::path logFilePath)
{
	boost::mutex::scoped_lock lock(m_writeMutex);

	std::ofstream  ofs(logFilePath.string().data(), std::ios_base::app);
	ofs << line << std::endl;
	ofs.close();
}

void LumpContainer::inotifyLumpfile(std::filesystem::path logFilePath)
{
	// Start a thread to monitor a LUMP component.
	m_logReadingThreadIdListMutex.lock();
	if(LogScanner::KEEP_LOOPING.load() && !RESTART_THREAD.load())
	{
		std::shared_ptr<boost::thread> thread(std::make_shared<boost::thread>(boost::thread(&ConfContainer::execInotifyLogfileThread, this, logFilePath/*, boost::ref(prom)*/)));

		// We need to save thread->native_handle() because execInotifyLogfileThread
		// locks in inotify "read" while waiting for events.
		// We need to pthread_kill it to force the thread to exit when the
		// logfile is renamed (RESTART_THREAD) or a program stop (KEEP_LOOPING)
		m_logReadingThreadIdList.insert({logFilePath.string(), thread->native_handle()});
		m_logReadingThreadIdListMutex.unlock();

		thread->join();

		m_logReadingThreadIdListMutex.lock();
		m_logReadingThreadIdList.erase(logFilePath.string());
	}
	m_logReadingThreadIdListMutex.unlock();
}
/**END*/
