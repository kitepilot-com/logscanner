/*******************************************************************************
 * File: logScanner/ConfContainer.cpp                                          *
 * Created:     Jan-07-25                                                      *
 * Last update: May-17-25                                                      *
 * Top level program file.                                                     *
 *******************************************************************************/
/*
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
*/
#include <iostream>
#include <thread>
#include <chrono>
#include <boost/thread/thread.hpp>
//#include <boost/thread/future.hpp>
#include <signal.h>
#include <sys/inotify.h>

#include "LogManager.h"
#include "LogScanner.h"
#include "ConfContainer.h"

ConfContainer::ConfContainer(std::string logFilePath, std::map<char, std::string> dateExtrct, std::map<std::string, char> regexpList) :
	RESTART_THREAD(false),
	m_state(INVALID),
	m_logReadingThreadID(0),
	m_readCtrlLiveLog(logFilePath),
	m_ctrlBaseName("run/ctrl" + logFilePath),
	m_logFilePath(logFilePath),
	m_threadName(logFilePath),
	m_dateExtrct(dateExtrct)
{
	for(auto regexp : regexpList)
	{
		m_regexpSvrt.push_back(regexp.second);
		m_regexpText.push_back(regexp.first);
	}

	std::replace(m_threadName.begin(), m_threadName.end(), '/', '_');
	std::replace(m_threadName.begin(), m_threadName.end(), '.', '_');
	std::replace(m_threadName.begin(), m_threadName.end(), '-', '_');

	std::filesystem::create_directories(m_ctrlBaseName);    //FIXTHIS!!!   Use variables..

	m_state = ConfContainer::HEALTHY;
}

ConfContainer::~ConfContainer()
{
}

std::string ConfContainer::getCtrlBaseName()
{
	return m_ctrlBaseName;
}

bool ConfContainer::initReadLogThread()
{
	// initReadLogThread is virtual and LumpContainer has it's own implemententation.
	// See LumpContainer.cpp for more info.
	return m_readCtrlLiveLog.initReadLogThread();
}

void ConfContainer::stopReadLogThread()
{
	m_readCtrlLiveLog.stopReadLogThread();
}

bool ConfContainer::initCatchUpThread()
{
	return m_readCtrlLiveLog.initCatchUpThread();
}

void ConfContainer::stopCatchUpThread()
{
	m_readCtrlLiveLog.stopCatchUpThread();
}

void ConfContainer::monitorLogForRename(std::string ctrlFileName)
{
	execMonitorFileForRename(getLogFilePath().string(), ctrlFileName);

	// If m_logReadingThreadID > 0 then the thread is blocked in ConfContainer::inotifyLogfile
	m_logReadingThreadID_Mutex.lock();
	if(m_logReadingThreadID > 0)
	{
		LogManager::getInstance()->consoleMsg(("==>> monitorLogForRename sending SIGUSR1 for " + m_logFilePath.string()).c_str());
		pthread_kill(m_logReadingThreadID, SIGUSR1);
	}
	m_logReadingThreadID_Mutex.unlock();
}

void ConfContainer::execMonitorFileForRename(std::string logFilePath, std::string ctrlFileName)
{
	// This function monitors the first line of a file that's being read.
	// If that line changes then the file that we were reading changed (probably logrotate renamed it).
	// inotify could be used for this but we still neet to preserve the first line
	// because the swap logrotate may happen when the program is not running.
	LogManager::getInstance()->consoleMsg(("==> STARTING ConfContainer::execMonitorFileForRename for " + logFilePath).c_str());

	std::ifstream ifs;
	std::ofstream ofs;
	std::string   ctrlLine;
	std::string   fileLine;
	bool          emptyLogfile = false;
int JUNK_USE_INOTIFY_TO_DETECT_RENAME;   //FIXTHIS!!!
	if(LogScanner::KEEP_LOOPING.load() && !RESTART_THREAD.load())
	{
		// Get the first line of the logfile to be monitored.
		ifs.open(logFilePath, std::ios::in);
		std::getline(ifs, fileLine);
		emptyLogfile = ifs.eof();
		ifs.close();

		// If the logfile is empty we don't even try to open the contro file.
		// There are several scenarios where we can land in a situation of empty logfile
		// and existing control file.
		// No matter what, it makes no sense to force a restart on an empty logfile.
		// Hence the ctrlLine == fileLine) || emptyLogfile) condition on the loop below.
		if(!emptyLogfile)
		{
			// Does the ctrlFile file exist?
			ifs.open(ctrlFileName, std::ios::in);
			if(ifs.is_open())
			{
				// Get control line
				std::getline(ifs, ctrlLine);
				ifs.close();
			}
			else
			{
				// Force the condition.
				ctrlLine = fileLine;

				// And save control line
				ofs.open(ctrlFileName, std::ios::out | std::ios::trunc);
				ofs << ctrlLine << std::endl;
				ofs.close();
			}
		}

		LogManager::getInstance()->consoleMsg((">>>> MONITORING " + logFilePath + " fileLine " + ctrlLine).c_str());

		while(LogScanner::KEEP_LOOPING.load() && !RESTART_THREAD.load() && ((ctrlLine == fileLine) || emptyLogfile))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(m_FIRST_LINE_MONITOR_INTERVAL));

			ifs.open(logFilePath, std::ios::in);
			if(ifs.is_open())
			{
				std::getline(ifs, fileLine);

				if(emptyLogfile && !ifs.eof())
				{
					// Then save the first line of the logfile. 
					ctrlLine = fileLine;

					std::ofstream ofs(ctrlFileName, std::ios::out | std::ios::trunc);
					ofs << ctrlLine << std::endl;
					ofs.close();

					emptyLogfile = false;
				}

				ifs.close();
			}
			else
			{
				// There is a race condition here with logrotate:
				// It's possible that we try to open the logfile right at the moment
				// where logrotate renamed the old one and before it created the new one.
				// Sooooooo...
				// Let it rip and try again.
				// If I ever find a situation where this (uselessly) loops forever I'll reimplement the loop...
				LogManager::getInstance()->consoleMsg(("*** WARNING *** unable to open logfile " + logFilePath + " in ConfContainer::execMonitorFileForRename").c_str());
				std::this_thread::sleep_for(std::chrono::milliseconds(m_FIRST_LINE_MONITOR_INTERVAL));
			}
		}
 
		if(ctrlLine != fileLine)
		{
			LogManager::getInstance()->consoleMsg(("*** WARNING *** ctrlLine changed for " + logFilePath + ", from '" + ctrlLine + "' to '" + fileLine + "', Restarting thread...").c_str());
			remove(ctrlFileName.c_str());

			RESTART_THREAD.store(true);
		}
	}

	LogManager::getInstance()->consoleMsg(("==> EXITING ConfContainer::execMonitorFileForRename for " + logFilePath).c_str());
}

std::string ConfContainer::getLumpComponent()
{
	return "";
}

bool ConfContainer::fileHasBackLog()
{
	return m_readCtrlLiveLog.fileHasBackLog();
}

ConfContainer::StatusVal ConfContainer::getObjStatus()
{
	return m_state;
}

const std::filesystem::path ConfContainer::getLogFilePath() const
{
	return m_logFilePath;
}

const std::string &ConfContainer::getThreadName()
{
	return m_threadName;
}

const std::string &ConfContainer::getDateExtract(DATE_EXTRACT key)
{
	auto it = m_dateExtrct.find((char)key);
	return it->second;
}

int ConfContainer::getRegexpCount()
{
	return m_regexpSvrt.size();
}

char ConfContainer::getRegexpSvrt(int rgxpThreadID)
{
	return m_regexpSvrt[rgxpThreadID];
}

const std::string &ConfContainer::getRegexpText(int rgxpThreadID)
{
	return m_regexpText[rgxpThreadID];
}

bool ConfContainer::getNextLogLine(std::string &line)
{
	return m_readCtrlLiveLog.getNextLogLine(line);
}

bool ConfContainer::getCatchupLine(std::string &line)
{
	return m_readCtrlLiveLog.getCatchupLine(line);
}

void ConfContainer::writePosOfNextLogline2Read()
{
	m_readCtrlLiveLog.writePosOfNextLogline2Read();
}

void ConfContainer::writePosOfNextCatchup2Read()
{
	m_readCtrlLiveLog.writePosOfNextCatchup2Read();
}

bool ConfContainer::eofLiveLog()
{
	return m_readCtrlLiveLog.eofLiveLog();
}

bool ConfContainer::eofCatchUp()
{
	return m_readCtrlLiveLog.eofCatchUp();
}

void ConfContainer::inotifyLogfile(std::filesystem::path logFilePath)
{
	// Start a thread to monitor a logfile.
	m_logReadingThreadID_Mutex.lock();
	if(LogScanner::KEEP_LOOPING.load() && !RESTART_THREAD.load())
	{
//		boost::promise<int> prom;
//		boost::future<int>  tret = prom.get_future();
		std::shared_ptr<boost::thread> thread(std::make_shared<boost::thread>(boost::thread(&ConfContainer::execInotifyLogfileThread, this, logFilePath/*, boost::ref(prom)*/)));

		// We need to save thread->native_handle() because execInotifyLogfileThread
		// locks in inotify "read" while waiting for events.
		// We need to pthread_kill it to force the thread to exit when the
		// logfile is renamed (RESTART_THREAD) or a program stop (KEEP_LOOPING)
		m_logReadingThreadID = thread->native_handle();
		m_logReadingThreadID_Mutex.unlock();

//		RESTART_THREAD.store(tret.get());
		thread->join();

		m_logReadingThreadID_Mutex.lock();
		m_logReadingThreadID = 0;
	}
	m_logReadingThreadID_Mutex.unlock();
}

// SEE:
// https://stackoverflow.com/questions/32281277/too-many-open-files-failed-to-initialize-inotify-the-user-limit-on-the-total
// echo 2048 > /proc/sys/user/max_inotify_instances
// Someday I'll clean this up, by the meantime it does what I need...
void ConfContainer::execInotifyLogfileThread(std::filesystem::path logFilePath/*, boost::promise<int> &prom*/)
{
	// This function gets called as a thread from:
	// ConfContainer::inotifyLogfile => to monitor logfiles
	// and:
	// LumpContainer::inotifyLumpfile => to monitor LUMP components.

	bool fileChanged = false;

	while(LogScanner::KEEP_LOOPING.load() && !RESTART_THREAD.load() && !fileChanged)
	{
		// From: https://developer.ibm.com/tutorials/l-ubuntu-inotify/
		std::string logPath(logFilePath.parent_path().string());
		std::string logName(logFilePath.filename().string());

		int EVENT_SIZE = ( sizeof (struct inotify_event) );
		int BUF_LEN    = ( 1024 * ( EVENT_SIZE + 16 ) );

		int length, Idx = 0;
		int fd;
		int wd;
		char buffer[BUF_LEN];

		fd = inotify_init();

		if(fd < 0 ) {
			perror( "inotify_init" ); //FIXTHIS!!!
		}
//LogManager::getInstance()->consoleMsg(("DBG_execInotifyLogfileThread_B4_read => " + logPath + "/" + logName).c_str());
		wd = inotify_add_watch( fd, logPath.data(), IN_MODIFY | IN_CREATE | IN_DELETE );
		length = read( fd, buffer, BUF_LEN );
//LogManager::getInstance()->consoleMsg(("DBG_execInotifyLogfileThread_AF_read => " + logPath + "/" + logName).c_str());
		if (length == -1)
		{
			if (errno == EINTR)
			{
				LogManager::getInstance()->consoleMsg("===> execInotifyLogfileThread: read interrupted by signal, exiting...");
			}
			else
			{
				LogManager::getInstance()->consoleMsg(("===> Read error in execInotifyLogfileThread => " + std::string(strerror(errno))).c_str());
			}
		}

		while(LogScanner::KEEP_LOOPING.load() && !RESTART_THREAD.load() && Idx < length)
		{
			struct inotify_event *event = ( struct inotify_event * ) &buffer[Idx];

			if(event->len )
			{
				if(strcmp(event->name, logName.data()) == 0 )
				{
					if(event->mask & IN_CREATE )
					{
						if(event->mask & IN_ISDIR )
						{// printf( "The directory %s was created.\n", event->name );
						}
						else
						{// printf( "The file %s was created.\n", event->name );
						}
					}
					else if(event->mask & IN_DELETE )
					{
						if(event->mask & IN_ISDIR )
						{// printf( "The directory %s was deleted.\n", event->name );
						}
						else
						{// printf( "The file %s was deleted.\n", event->name );
						}
					}
					else if(event->mask & IN_MODIFY )
					{
						if(event->mask & IN_ISDIR )
						{// printf( "The directory %s was modified.\n", event->name );
						}
						else
						{
//LogManager::getInstance()->consoleMsg(("DBG_inotify => " + logFilePath.string() + "  => fileChanged = true;").c_str());
							fileChanged = true;
						}
					}
				}
			}

			Idx += EVENT_SIZE + event->len;
		}

		( void ) inotify_rm_watch( fd, wd );
		( void ) close( fd );
	}

//	prom.set_value(sigInterrupted);
}
/*END*/
