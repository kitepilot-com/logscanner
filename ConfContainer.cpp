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
#include <boost/regex.hpp>
#include <signal.h>
#include <sys/inotify.h>

#include "LogManager.h"
#include "LogScanner.h"
#include "ConfContainer.h"

std::string ConfContainer::m_TIMESTAMP_FILES_REGEX = "[[:digit:]]{" + std::to_string(LogManager::TIMESTAMP_SIZE) + "}-[[:alpha:]]{1,}\\.ctrl";

ConfContainer::ConfContainer(std::string logFilePath, std::map<char, std::string> dateExtrct, std::map<std::string, char> regexpList) :
	RESTART_THREAD(false),
	m_state(INVALID),
	m_ctrlBaseName("run/ctrl" + logFilePath),    //FIXTHIS!!!   Use variables..
	m_logFilePath(logFilePath),
	m_threadName(logFilePath),
	m_dateExtrct(dateExtrct)
{
int JUNK_CONVERT_LITERAL_PATHS_TO_VARIABLES;//FIXTHIS!!!
	for(auto regexp : regexpList)
	{
		m_regexpSvrt.push_back(regexp.second);
		m_regexpText.push_back(regexp.first);
	}

	std::replace(m_threadName.begin(), m_threadName.end(), '/', '_');
	std::replace(m_threadName.begin(), m_threadName.end(), '.', '_');
	std::replace(m_threadName.begin(), m_threadName.end(), '-', '_');

	std::filesystem::create_directories(m_ctrlBaseName);

	m_state = ConfContainer::HEALTHY;
}

ConfContainer::~ConfContainer()
{
}

INotifyObj::Outcome ConfContainer::monitorNewLineThread(std::string logFilePath)
{
//LogManager::getInstance()->consoleMsg(("DBG_monitorNewLineThread for " + logFilePath).c_str());
	INotifyObj iNotifyObj;
	iNotifyObj.monitorNewLineThread(logFilePath);
//LogManager::getInstance()->consoleMsg(("DBG_monitorNewLineThread for " + logFilePath).c_str());
}

INotifyObj::Outcome ConfContainer::monitor4RenameThread(std::string logFilePath)
{
	LogManager::getInstance()->consoleMsg(("=> STARTING monitor4RenameThread for " + logFilePath).c_str());
//LogManager::getInstance()->consoleMsg(("DBG_monitor4RenameThread B4 m_monitor4RenameThread.insert " + logFilePath).c_str());
//	m_monitor4RenameThread.insert(logFilePath);
//LogManager::getInstance()->consoleMsg(("DBG_monitor4RenameThread AF m_monitor4RenameThread.insert " + logFilePath).c_str());
	INotifyObj iNotifyObj;
	if(iNotifyObj.monitor4RenameThread(logFilePath) == INotifyObj::EVENT_TRUE)
	{
		LogManager::getInstance()->consoleMsg(("*** WARNING ***  " + logFilePath + " was renamed (or deleted), Restarting thread...").c_str());
		removeAllControlFiles(logFilePath);
		RESTART_THREAD.store(true);
	}
//LogManager::getInstance()->consoleMsg(("DBG_monitor4RenameThread B4 m_monitor4RenameThread.erase " + logFilePath).c_str());
//	m_monitor4RenameThread.erase(m_monitor4RenameThread.find(logFilePath));
//LogManager::getInstance()->consoleMsg(("DBG_monitor4RenameThread AF m_monitor4RenameThread.erase " + logFilePath).c_str());
	LogManager::getInstance()->consoleMsg(("=> EXITING monitor4RenameThread for " + logFilePath).c_str());
}

bool ConfContainer::initReadLogThread()
{
	// This function opens the logfile and uses std::ifstream::seekg() to set the input stream
	// pointer to EOF, then stores the size of the file in m_fileOrSegmentEOF.
	// initReadLogThread is virtual and LumpContainer has it's own implemententation.
	// See LumpContainer.cpp for more info.
	LogManager::getInstance()->consoleMsg(("Opening logfile for reading beyond EOF\t=>\t" + m_logFilePath.string()).c_str());

	// We save the first line of the logfile in m_FSTLINEOFLOGFILE_CTRL
	// If that line changes the most likely scenario is that logrotate
	// renamed the logfila and all our file ponters are invalid.
	m_monitorNewLineThread = nullptr;
	ValidateAndUpdateFirstLineOf(m_logFilePath, m_ctrlBaseName + m_FSTLINEOFLOGFILE_CTRL);

	// Open the logfile.
	m_ifsReadLog.open(m_logFilePath, std::ios::in);
	if(m_ifsReadLog.is_open())
	{
		// Set the reading cursor to EOF and store the size of the file
		// We want to read the lines that we are getting NOW to detect immediate threats.
		// The function fileHasBackLog() will detect the need to read lines left behind.
		m_ifsReadLog.seekg(0, m_ifsReadLog.end);
		m_fileOrSegmentEOF = m_ifsReadLog.tellg();
	}
	else
	{
		LogManager::getInstance()->consoleMsg(("*** FATAL *** unable to open logfile for reading beyond EOF " + m_logFilePath.string()).c_str());
	}

	return m_ifsReadLog.is_open();
}

void ConfContainer::stopReadLogThread()
{
	m_ifsReadLog.close();

	// We need to release the monitor because INotifyObj::intfThread will not exit.
	if(RESTART_THREAD.load() == true)
	{
		INotifyObj iNotifyObj;
		iNotifyObj.releaseMonitorNewLineThread(m_logFilePath.string());
		iNotifyObj.releaseMonitor4RenameThread(m_logFilePath.string());
	}

	if(m_monitorNewLineThread.get() != nullptr)
	{
		m_monitorNewLineThread->join();
	}
}

bool ConfContainer::initCatchUpThread()
{
//	return m_readCtrlLiveLog.initCatchUpThread();
	// This function opens (AGAIN) the logfile and sets the input stream pointer to "nextCatchup2Read.ctrl" 
	// "fileOrSegmentEOF.ctrl" is used as EOF.
	LogManager::getInstance()->consoleMsg(("Opening (Catch UP) logfile\t=>\t" + m_logFilePath.string()).c_str());

	m_doneCatchingUp = false;

	// Open the logfile.
	m_ifsCatchUp.open(m_logFilePath, std::ios::in);
	if(m_ifsCatchUp.is_open())
	{
		// Set the reading cursor to m_NEXTCATCHUP2READ_CTRL
		// We want to read the lines that we between m_NEXTCATCHUP2READ_CTRL and m_FILEORSEGMENTEOF_CTRL.
		// The function getCatchupLine() will use m_fileOrSegmentEOF as EOF and, upon reaching it, check for TIMESTAMP files.
		getCtrlValOf(m_ctrlBaseName + m_NEXTCATCHUP2READ_CTRL, &m_nextCatchup2Read);
		getCtrlValOf(m_ctrlBaseName + m_FILEORSEGMENTEOF_CTRL, &m_fileOrSegmentEOF);

		m_ifsCatchUp.seekg(m_nextCatchup2Read, m_ifsCatchUp.beg);

		// Load the list of TIMESTAMP "segment" files.
		boost::match_results<std::string::const_iterator> result;
		boost::regex                                      regexp(m_TIMESTAMP_FILES_REGEX);

		for (auto const& dir_entry : std::filesystem::directory_iterator{m_ctrlBaseName})
		{
			if (std::filesystem::is_regular_file(dir_entry.path()))
			{
				// Filter the files that match our mask.
				if(boost::regex_search(dir_entry.path().string(), result, regexp))
				{
					// Got a segment file...
					m_filestampFileList.insert(dir_entry.path().filename().string());
				}
			}
		}
	}
	else
	{
		LogManager::getInstance()->consoleMsg(("*** FATAL *** unable to open logfile (Catch UP) " + m_logFilePath.string()).c_str());
	}

	return m_ifsCatchUp.is_open();
}

void ConfContainer::stopCatchUpThread()
{
//	m_readCtrlLiveLog.stopCatchUpThread();
	m_ifsCatchUp.close();
}

std::string ConfContainer::getLumpComponent()
{
	return "";
}

bool ConfContainer::fileHasBackLog()
{
//	return m_readCtrlLiveLog.fileHasBackLog();
	// What is a backlog?
	// How do we detect it?
	// And what do we do about it...

	// The back log happens because we want to process lines as soon as they arrive.
	// Reading (and processing) a few megabytes of old lines may take hours (ask me how I know)
	// and by the meantime we don't address the immediate threat.

	// So there are 2 threads ready to go at startup.
	//   LogScanner::logReadingThread() => Thread 1, reads lines that are being appended to the logfile.
	//   LogScanner::catchUpLogThread() => Thread 2, reads lines that were already in the logfile.
	//     Thread 1 always starts and waits for lines to be appended to the file, it's the main thread of the program.
	//     Thread 2 starts when there is "backlog" and exits after all the lines are read.

	// The program uses "Control Files" to resume reading logs (and backlogs) at startup
	// without wasting time re-reading old lines.`

	// Control Files:
	// ==============

	// fileOrSegmentEOF.ctrl:
	// ----------------------
	// This file holds the input stream pointer to the EOF of the freshly opened logfile
	// and it is created in this function..
	// The lines between nextCatchup2Read.ctrl and fileOrSegmentEOF.ctrl describe a "Backlog Segment".
	// There may be several Backlog Segments active at any given time, and thhose are differentiated by names as:
	// "{TIMESTAMP}-{CTRL_FILE_NAME}.ctrl"
	// The cases that create those files are enumerated below.

	// nextLogline2Read.ctrl:
	// ----------------------
	// This file holds the input stream pointer to fetch the next NEW line from the logfile.
	// It is created and updated by writePosOfNextLogline2Read() and it always
	// exists unless we are starting fresh with a brand new logfile (maybe (most likely) created by logrotate).
	// Each getNextLogLine() call saves the return of std::ifstream::tellg() to m_nextLogline2Read.
	// LogScanner::processingThread() calls ConfContainer::writePosOfNextLogline2Read()
	// to store the value of m_nextLogline2Read to "nextLogline2Read.ctrl" AFTER the line has been fully processed
	// and written to table T100_LinesSeen.

	// nextCatchup2Read.ctrl:
	// ----------------------
	// This file holds the input stream pointer to fetch the next CATCHUP line from the logfile.
	// It is created and updated by writePosOfNextCatchup2Read() and it DOES NOT always
	// exist. It's created to work in cunjunction with "fileOrSegmentEOF.ctrl" and t is removed after we
	// finish reading the backlog..
	// Each getNextCatchUp() call saves the return of std::ifstream::tellg() to m_nextCatchup2Read.
	// LogScanner::processingThread() calls ConfContainer::writePosOfNextCatchup2Read()
	// to store the value of m_nextCatchup2Read to "nextCatchup2Read.ctrl" AFTER the line has been fully processed
	// and written to table T100_LinesSeen.

	// ALL Control Files are deleted logrotate hads us a new file.

	// States and actions at startup:
	// ==================================================================================================

	// Case 1: The program has a fresh start on an empty logfile -> Control files SHOULD NOT exist.
	//      => Thread 1 starts reading from begining of file.
	//      => Thread 2 doesn't start.

	// Case 2: The program has a fresh start on a non empty logfile -> Control files SHOULD NOT exist.
	//      => "nextLogline2Read.ctrl" is created containing EOF.
	//      => "fileOrSegmentEOF.ctrl" IS created containing EOF.
	//      => "nextCatchup2Read.ctrl" is created containing 0.
	//      => Thread 1 starts reading from EOF.
	//      => Thread 2 starts reading from begining of file to "fileOrSegmentEOF.ctrl".
	//      => hasBacklog => TRUE;

	// Case 3: The program starts after it has been interrupted -> nextLogline2Read.ctrl DOES exist.
	//   Case 3.1: Case 1 has been interrupted -> "nextCatchup2Read.ctrl" and "fileOrSegmentEOF.ctrl" DO NOT exist.
	//     Case 3.1,1: EOF == "nextLogline2Read.ctrl" -> NO NEW LINES have been appended.
	//          => Thread 1 starts reading from EOF.
	//          => Thread 2 doesn't start.

	//     Case 3.1,2: EOF > "nextLogline2Read.ctrl" ->  NEW LINES HAVE been appended.
	//          => "fileOrSegmentEOF.ctrl" IS created containing EOF.
	//          => "nextLogline2Read.ctrl" is renamed to "nextCatchup2Read.ctrl".
	//          => Thread 1 starts reading from EOF.
	//          => Thread 2 starts reading from "nextCatchup2Read.ctrl" up to "fileOrSegmentEOF.ctrl".
	//          => hasBacklog => TRUE;

	//   Case 3.2: Case 2 has been interrupted -> "nextCatchup2Read.ctrl" and "fileOrSegmentEOF.ctrl" exist.
	//     Case 3.2,1: EOF == "nextLogline2Read.ctrl" -> NO NEW LINES have been appended.
	//          => Thread 1 starts reading from EOF.
	//          => Thread 2 starts reading from "nextCatchup2Read.ctrl" up to "fileOrSegmentEOF.ctrl".

	//     Case 3.2,2: EOF > "fileOrSegmentEOF.ctrl" ->  NEW LINES HAVE been appended.
	//          => A TIMESTAMP is created.
	//          => The file "nextLogline2Read.ctrl" is renamed to "{TIMESTAMP}-nextCatchup2Read.ctrl".
	//          => The current EOF                  is written to "{TIMESTAMP}-fileOrSegmentEOF.ctrl".
	//          => Thread 1 starts reading from EOF.
	//          => Thread 2 starts reading from "nextCatchup2Read.ctrl" up to "fileOrSegmentEOF.ctrl".
	//          => hasBacklog => TRUE;

	// Case 3.2,2: Special handling:
	// -----------------------------
	//    NOTE: this process is addressed in getCatchupLine().
	//      => For each TIMESTAMP group:
	//        => Remove "{TIMESTAMP}-" from file names (rename).
	//        => Thread 2 starts reading from "nextCatchup2Read.ctrl" up to "fileOrSegmentEOF.ctrl".

	bool hasBacklog = false;
//LogManager::getInstance()->consoleMsg(("DBG_INSIDE_fileHasBackLog").c_str());
	// m_fileOrSegmentEOF was set to EOF by ConfContainer::initReadLogThread()
	if(m_fileOrSegmentEOF == 0)
	{
		// EMPTY logfile. fresh start...
		// Case 1: The program has a fresh start on an empty logfile -> Control files SHOULD NOT exist.
		//      => Thread 1 starts reading from begining of file.
		//      => Thread 2 doesn't start.
		LogManager::getInstance()->consoleMsg(("fileHasBackLog => Case 1: Logfile " + m_logFilePath.string()).c_str());

		// Just in case remove control files, we don't want confusions below...
		removeAllControlFiles(m_logFilePath);
	}
	else
	{
		// NON empty logfile.
		LogManager::getInstance()->consoleMsg(("fileHasBackLog => Logfile " + m_logFilePath.string() + " size is " + std::to_string(m_fileOrSegmentEOF)).c_str());

		bool gotNextLogline2Read = getCtrlValOf(m_ctrlBaseName + m_NEXTLOGLINE2READ_CTRL, &m_nextLogline2Read);

		if(!gotNextLogline2Read)
		{
			// NO control files, fresh start...
			// Case 2: The program has a fresh start on a non empty logfile -> Control files SHOULD NOT exist.
			//      => "nextLogline2Read.ctrl" is created containing EOF.
			//      => "fileOrSegmentEOF.ctrl" IS created containing EOF.
			//      => "nextCatchup2Read.ctrl" is created containing 0.
			//      => Thread 1 starts reading from EOF.
			//      => Thread 2 starts reading from begining of file to "fileOrSegmentEOF.ctrl".
			//      => hasBacklog => TRUE;
			LogManager::getInstance()->consoleMsg(("fileHasBackLog => Case 2: Logfile " + m_logFilePath.string()).c_str());

			setCtrlValOf(m_ctrlBaseName + m_NEXTLOGLINE2READ_CTRL, m_fileOrSegmentEOF);
			setCtrlValOf(m_ctrlBaseName + m_FILEORSEGMENTEOF_CTRL, m_fileOrSegmentEOF);
			setCtrlValOf(m_ctrlBaseName + m_NEXTCATCHUP2READ_CTRL, m_nextCatchup2Read);
			hasBacklog = true;
		}
		else
		{
			LogManager::getInstance()->consoleMsg(("fileHasBackLog => Case 3: Logfile " + m_logFilePath.string()).c_str());

			// We have control files, this is NOT a fresh start.
			bool gotNextCatchUp2Read = getCtrlValOf(m_ctrlBaseName + m_NEXTCATCHUP2READ_CTRL);
			bool gotfileOrSegmentEOF = getCtrlValOf(m_ctrlBaseName + m_FILEORSEGMENTEOF_CTRL);

			// Case 3: The program starts after it has been interrupted -> nextLogline2Read.ctrl DOES exist.
			if(!gotNextCatchUp2Read && !gotfileOrSegmentEOF)
			{
				LogManager::getInstance()->consoleMsg(("fileHasBackLog => Case 3.1: Logfile " + m_logFilePath.string()).c_str());

				//   Case 3.1: Case 1 has been interrupted -> "nextCatchup2Read.ctrl" and "fileOrSegmentEOF.ctrl" DO NOT exist.
				if(m_fileOrSegmentEOF == m_nextLogline2Read) // Nothing to do...
				{
					//     Case 3.1,1: EOF == "nextLogline2Read.ctrl" -> NO NEW LINES have been appended.
					//          => Thread 1 starts reading from EOF.
					//          => Thread 2 doesn't start.
					LogManager::getInstance()->consoleMsg(("fileHasBackLog => Case 3.1.1: Logfile " + m_logFilePath.string()).c_str());
				}
				else if(m_fileOrSegmentEOF > m_nextLogline2Read)
				{
					//     Case 3.1,2: EOF > "nextLogline2Read.ctrl" ->  NEW LINES HAVE been appended.
					//          => "fileOrSegmentEOF.ctrl" IS created containing EOF.
					//          => "nextLogline2Read.ctrl" is renamed to "nextCatchup2Read.ctrl".
					//          => Thread 1 starts reading from EOF.
					//          => Thread 2 starts reading from "nextCatchup2Read.ctrl" up to "fileOrSegmentEOF.ctrl".
					//          => hasBacklog => TRUE;
					LogManager::getInstance()->consoleMsg(("fileHasBackLog => Case 3.1.2: Logfile " + m_logFilePath.string()).c_str());

					setCtrlValOf(m_ctrlBaseName + m_FILEORSEGMENTEOF_CTRL, m_fileOrSegmentEOF);
					rename((m_ctrlBaseName + m_NEXTLOGLINE2READ_CTRL).c_str(), (m_ctrlBaseName + m_NEXTCATCHUP2READ_CTRL).c_str());
					hasBacklog = true;
				}
				else
				{
					// This is a problem...
					// How did I get here?     :(
					LogManager::getInstance()->consoleMsg(("*** WARNING ***  => fileHasBackLog => Case 3.1: Logfile " + m_logFilePath.string() +
					                                       " m_fileOrSegmentEOF = " + std::to_string(m_fileOrSegmentEOF) +
					                                   " <=> m_nextLogline2Read " + std::to_string(m_nextLogline2Read)).c_str());
				}
			}
			else if(gotNextCatchUp2Read && gotfileOrSegmentEOF)
			{
				LogManager::getInstance()->consoleMsg(("fileHasBackLog => Case 3.2: Logfile " + m_logFilePath.string()).c_str());

				//   Case 3.2: Case 2 has been interrupted -> "nextCatchup2Read.ctrl" and "fileOrSegmentEOF.ctrl" exist.
				if(m_fileOrSegmentEOF == m_nextLogline2Read) // Nothing to do...
				{
					//     Case 3.2,1: EOF == "nextLogline2Read.ctrl" -> NO NEW LINES have been appended.
					//          => Thread 1 starts reading from EOF.
					//          => Thread 2 starts reading from "nextCatchup2Read.ctrl" up to "fileOrSegmentEOF.ctrl".
					LogManager::getInstance()->consoleMsg(("fileHasBackLog => Case 3.2.1:Logfile " + m_logFilePath.string()).c_str());
				}
				else if(m_fileOrSegmentEOF > m_nextLogline2Read)
				{
					//     Case 3.2,2: EOF > "fileOrSegmentEOF.ctrl" ->  NEW LINES HAVE been appended.
					//          => A TIMESTAMP is created.
					//          => The file "nextLogline2Read.ctrl" is renamed to "{TIMESTAMP}-nextCatchup2Read.ctrl".
					//          => The current EOF                  is written to "{TIMESTAMP}-fileOrSegmentEOF.ctrl".
					//          => Thread 1 starts reading from EOF.
					//          => Thread 2 starts reading from "nextCatchup2Read.ctrl" up to "fileOrSegmentEOF.ctrl".
					//          => hasBacklog => TRUE;
					LogManager::getInstance()->consoleMsg(("fileHasBackLog => Case 3.2.2:Logfile " + m_logFilePath.string()).c_str());

					char  timestamp[LogManager::TIMESTAMP_SIZE + 1];
					LogManager::getInstance()->nanosecTimestamp(timestamp);
					rename((m_ctrlBaseName + m_NEXTLOGLINE2READ_CTRL).c_str(), (m_ctrlBaseName + "/" + timestamp + "-" + (m_NEXTCATCHUP2READ_CTRL + 1)).c_str());
					setCtrlValOf(m_ctrlBaseName + "/" + timestamp + "-" + (m_FILEORSEGMENTEOF_CTRL + 1), m_fileOrSegmentEOF);
					hasBacklog = true;
				}
				else
				{
					// This is a problem...
					// How did I get here?     :(
					LogManager::getInstance()->consoleMsg(("*** WARNING ***  => fileHasBackLog => Case 3.2: Logfile " + m_logFilePath.string() +
					                                       " m_fileOrSegmentEOF = " + std::to_string(m_fileOrSegmentEOF) +
					                                   " <=> m_nextLogline2Read = " + std::to_string(m_nextLogline2Read)).c_str());
				}
			}
			else
			{
				// This is a problem...
				// How did I get here?     :(
				LogManager::getInstance()->consoleMsg(("*** WARNING ***  => fileHasBackLog => Case 3: Logfile " + m_logFilePath.string() +
				                                       "  is NULL => gotNextCatchUp2Read = '" + std::string(gotNextCatchUp2Read ? "T" : "F") +
				                                             "' <==> gotfileOrSegmentEOF = '" + std::string(gotfileOrSegmentEOF ? "T" : "F")).c_str());
			}
		}
	}
//LogManager::getInstance()->consoleMsg(("DBG_RETURNING_fileHasBackLog WITH hasBacklog = " + (hasBacklog ? "T" : "F")).c_str());
	return hasBacklog;
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
//	return m_readCtrlLiveLog.getNextLogLine(line);
	// When a line is read the input stream pointer points to the next line to be read.
	// If this is the last line of the file the input stream pointer now points beyond EOF
	// and the next read will report EOF and m_ifsReadLog.tellg() will return -1;
	// Next line to read will be in m_nextLogline2Read.
	m_nextLogline2Read = m_ifsReadLog.tellg();

	std::getline(m_ifsReadLog, line);
//LogManager::getInstance()->consoleMsg(("DBG_ConfContainer::getNextLogLine -> EOF = " + std::string(m_ifsReadLog.eof() ? "T" : "F") + " => LINE '" + line + "'").c_str()); //FIXTHIS!!!
	return !m_ifsReadLog.eof();
}

bool ConfContainer::getCatchupLine(std::string &line)
{
//	return m_readCtrlLiveLog.getCatchupLine(line);
	// This function should NEVER see EOF!
	// In ConfContainer::initReadLogThread() we do:
	    // m_ifsReadLog.open(m_logFilePath, std::ios::in);
	    // Set the reading cursor to EOF and store the size of the file
	    // m_ifsReadLog.seekg(0, m_ifsReadLog.end);
	    // m_fileOrSegmentEOF = m_ifsReadLog.tellg();
	  // At this point m_fileOrSegmentEOF is equal to the size of the file.
	    // Entering this function we do:
	      // m_nextCatchup2Read = m_ifsCatchUp.tellg();
	    // and then we check:
	      // if(m_nextCatchup2Read < m_fileOrSegmentEOF)
	        // The if above will fail before we hit EOF.

	// After we finish reading a "segment" we look for TIMESTAMP files.
	// m_doneCatchingUp becomes true when we don't find any.
	bool gotLineToGo = false;

	// Next line to read will be in m_nextCatchup2Read.
	m_nextCatchup2Read = m_ifsCatchUp.tellg();
//LogManager::getInstance()->consoleMsg(("DBG_ConfContainer::getCatchupLine -> m_doneCatchingUp = " + (m_doneCatchingUp ? "T" : "F") + " => m_ifsCatchUp.eof() = " + (m_ifsCatchUp.eof() ? "T" : "F")).c_str()); //FIXTHIS!!!
	while(!m_doneCatchingUp && !gotLineToGo && !m_ifsCatchUp.eof())
	{
//LogManager::getInstance()->consoleMsg(("DBG_ConfContainer::getCatchupLine -> TOP LOOP.").c_str()); //FIXTHIS!!!
		if(m_nextCatchup2Read < m_fileOrSegmentEOF)
		{
			// Still reading this segment.
			std::getline(m_ifsCatchUp, line);
//LogManager::getInstance()->consoleMsg(("DBG_ConfContainer::getCatchupLine -> EOF = " + (m_ifsCatchUp.eof() ? "T" : "F") + " => LINE '" + line + "'").c_str()); //FIXTHIS!!!
			gotLineToGo = true;
		}
		else
		{
//LogManager::getInstance()->consoleMsg(("DBG_ConfContainer::getCatchupLine -> NXY_SEG = "  + " => LINE '" + line + "'").c_str()); //FIXTHIS!!!
			// Start reading a new segment.
			if(m_filestampFileList.size() > 0)
			{
				// Get the last 2 names.
				// This list *HAS* to be in alphabetical reverse order!!!
				refreshCtrlValOf(m_nextCatchup2Read);
				refreshCtrlValOf(m_fileOrSegmentEOF);

				// And set the input stream pointer...
				m_ifsCatchUp.seekg(m_nextCatchup2Read, m_ifsCatchUp.beg);
			}
			else
			{
				// We ran out of segments!
				remove((m_ctrlBaseName + m_FILEORSEGMENTEOF_CTRL).c_str());
				remove((m_ctrlBaseName + m_NEXTCATCHUP2READ_CTRL).c_str());
//LogManager::getInstance()->consoleMsg(("DBG_ConfContainer::getCatchupLine -> END_ALL_SEG").c_str()); //FIXTHIS!!!
				m_doneCatchingUp = true;
			}
		}
	}
int JUNK_MAY_BE_COMING_HOURS_LATE_ADJUST_EVAL;  // We don't want lines to drop from eval until we evaluate the group.
	return !m_doneCatchingUp;
}

void ConfContainer::writePosOfNextLogline2Read()
{
//	m_readCtrlLiveLog.writePosOfNextLogline2Read();
	m_writeMutexNextLogline.lock();
//LogManager::getInstance()->consoleMsg(("DBG_ConfContainer::writePosOfNextLogline2Read => " + m_ctrlBaseName + m_NEXTLOGLINE2READ_CTRL + " => " + std::to_string(m_nextLogline2Read)).c_str());
	// This function is called at the end of LogScanner::insertIntoSeenTable which
	// in turn is called after all the regexps for that line have been evaluated.
	// There is a race condition to write the offset of the last "read",
	// because rexgeps are evaluated asynchronically, so...
	// We ensure that we only write the higest m_nextLogline2Read..
	// Rexgeps process will happen even after KEEP_LOOPING && RESTART_THREAD
	// are flagged to stop, so the program will "catch up" with itself.
	if(m_nextLogline2Read > m_lastLoglinePosWritten)
	{
		setCtrlValOf(m_ctrlBaseName + m_NEXTLOGLINE2READ_CTRL, m_nextLogline2Read);
		m_lastLoglinePosWritten = m_nextLogline2Read;
	}

	m_writeMutexNextLogline.unlock();
}

void ConfContainer::writePosOfNextCatchup2Read()
{
//	m_readCtrlLiveLog.writePosOfNextCatchup2Read();
	m_writeMutexNextCatchup.lock();
//LogManager::getInstance()->consoleMsg(("DBG_ConfContainer::writePosOfNextCatchup2Read => " + m_ctrlBaseName + m_NEXTCATCHUP2READ_CTRL + " => " + std::to_string(m_nextCatchup2Read)).c_str());
	// This function is called at the end of LogScanner::insertIntoSeenTable which
	// in turn is called after all the regexps for that line have been evaluated.
	// There is a race condition to write the offset of the last "read",
	// because rexgeps are evaluated asynchronically, so...
	// We ensure that we only write the higest m_nextCatchup2Read..
	// Rexgeps process will happen even after KEEP_LOOPING && RESTART_THREAD
	// are flagged to stop, so the program will "catch up" with itself.
	if(m_nextCatchup2Read > m_lastCatchupPosWritten)
	{
		setCtrlValOf(m_ctrlBaseName + m_NEXTCATCHUP2READ_CTRL, m_nextCatchup2Read);
		m_lastCatchupPosWritten = m_nextCatchup2Read;
	}

	m_writeMutexNextCatchup.unlock();
}

bool ConfContainer::eofLiveLog()
{
//	return m_readCtrlLiveLog.eofLiveLog();
	bool gotEOF = m_ifsReadLog.eof();

	if(gotEOF)
	{
		m_ifsReadLog.clear();
	}

	return gotEOF;
}

bool ConfContainer::eofCatchUp()
{
//	return m_readCtrlLiveLog.eofCatchUp();
	return m_doneCatchingUp;
}

std::string ConfContainer::getCtrlBaseName()
{
	return m_ctrlBaseName;
}

void ConfContainer::ValidateAndUpdateFirstLineOf(std::string logFilePath, std::filesystem::path ctrlFileName)
{
	// We save the first line of the logfile in m_FSTLINEOFLOGFILE_CTRL
	// If that line changes the most likely scenario is that logrotate
	// renamed the logfila and all our file ponters are invalid.
	// This criteria is valid even if a single LUMP component is renamed.
	std::filesystem::path logFileName(logFilePath);
LogManager::getInstance()->consoleMsg(("DBG_ValidateAndUpdateFirstLineOf EMPTY? => " + logFilePath + " => " + (std::filesystem::is_empty(logFileName) ? "T" : "F")).data());//FIXTHIS!!!
	// If the logfile is empty
	if(std::filesystem::is_empty(logFileName))
	{
LogManager::getInstance()->consoleMsg(("DBG_ValidateAndUpdateFirstLineOf EMPTY => " + logFilePath).data());//FIXTHIS!!!
		// All control files are invalid...
		removeAllControlFiles(logFileName);
		StartWaitForFirstLineThread(logFilePath, ctrlFileName);
	}
	else
	{
LogManager::getInstance()->consoleMsg(("DBG_ValidateAndUpdateFirstLineOf NON EMPTY => " + logFilePath).data());//FIXTHIS!!!
		// Get first line of logfile.
		std::ifstream ifsFile(logFileName, std::ios::in);
		std::string   fileLine;

		std::getline(ifsFile, fileLine);
		ifsFile.close();

		// If the control file exists
		if(std::filesystem::exists(ctrlFileName))
		{
			// Compare the lines...
			std::ifstream ifsCtrl(ctrlFileName, std::ios::in);
			std::string   ctrlLine;

			std::getline(ifsCtrl, ctrlLine);
			ifsCtrl.close();

			if(fileLine != ctrlLine)
			{
				// OK, we are not reading the same file as before,
				// all control files are invalid...
				removeAllControlFiles(logFileName);
			}
		}
		else
		{
			// Create the file...
			std::ofstream ofs(ctrlFileName, std::ios::out | std::ios::trunc);
			ofs << fileLine << std::endl;
			ofs.close();
		}
	}
}

void ConfContainer::StartWaitForFirstLineThread(std::string logFilePath, std::filesystem::path ctrlFileName)
{
	m_monitorNewLineThread = std::make_shared<boost::thread>(&ConfContainer::waitForFirstLineThread, this, logFilePath, ctrlFileName);
}

void ConfContainer::waitForFirstLineThread(std::string logFilePath, std::filesystem::path ctrlFileName)
{
	LogManager::getInstance()->consoleMsg(("=> STARTING waitForFirstLineThread for files in " + logFilePath).c_str());
LogManager::getInstance()->consoleMsg(("ctrlFileName => " + ctrlFileName.string()).data());//FIXTHIS!!!
	if(monitorNewLineThread(logFilePath) == INotifyObj::EVENT_TRUE)
	{
		// Get the line...
		std::ifstream ifsFile(logFilePath, std::ios::in);
		std::string   fileLine;

		std::getline(ifsFile, fileLine);
		ifsFile.close();

		// Create the file...
		std::ofstream ofs(ctrlFileName, std::ios::out | std::ios::trunc);

		ofs << fileLine << std::endl;
		ofs.close();
	}

	LogManager::getInstance()->consoleMsg(("=> EXITING waitForFirstLineThread for files in " + logFilePath).c_str());
}

bool ConfContainer::refreshCtrlValOf(std::streampos &ctrlVal)
{
	std::set<std::string>::iterator ctrlItem;
	std::string                     strPath;
	std::string                     strFile;
	std::string                     ctrlFile;
	std::size_t                     found = 0;

	// Last file.
	ctrlItem = m_filestampFileList.end();

	// Split it in path and name
	found    = (*ctrlItem).find_last_of("/");
	strPath  = (*ctrlItem).substr(0, found);
	strFile  = (*ctrlItem).substr(found + 1);

	// Remove the timestamp
	found    = strFile.find_last_of("-");
	ctrlFile = strPath + "/" + strFile.substr(found + 1);

	// Now remane de file without the timestamp in the filesystem.
	remove((*ctrlItem).c_str());
	rename((*ctrlItem).c_str(), ctrlFile.c_str());

	// And remove it from the set.
	m_filestampFileList.erase(*ctrlItem);

	// Finally set the variable to watever was in the file.
	return getCtrlValOf(*ctrlItem, &ctrlVal);
}

bool ConfContainer::setCtrlValOf(std::string ctrlFile, std::streampos &ctrlVal)
{
	std::ofstream ofs(ctrlFile, std::ios_base::trunc);
	bool          fileOpened = false;
//LogManager::getInstance()->consoleMsg(("DBG_ConfContainer::setCtrlValOf => IN => '"         + ctrlFile + "'" + std::to_string(ctrlVal)).c_str()); //FIXTHIS!!!
	if(ofs.is_open())
	{
		ofs << ctrlVal << std::endl;
		ofs.close();
//LogManager::getInstance()->consoleMsg(("DBG_WRITTEN_ConfContainer::setCtrlValOf => IN => '" + ctrlFile + "'" + std::to_string(ctrlVal)).c_str()); //FIXTHIS!!!
		fileOpened = true;
	}

	return fileOpened;
}

bool ConfContainer::getCtrlValOf(std::string ctrlFile, std::streampos *ctrlVal)
{
	std::ifstream ifs(ctrlFile, std::ios::in);
	bool          fileOpened = false;
//LogManager::getInstance()->consoleMsg(("DBG_ConfContainer::getCtrlValOf => IN => '" + ctrlFile + "'").c_str()); //FIXTHIS!!!
	if(ifs.is_open())
	{
		std::string line;

		std::getline(ifs, line);
		ifs.close();

		if(ctrlVal)
		{
			*ctrlVal = atoi(line.c_str());
		}

		fileOpened = true;
	}

	return fileOpened;
}

void ConfContainer::removeAllControlFiles(std::string logFilePath)
{
	boost::match_results<std::string::const_iterator> result;
	boost::regex                                      regexp(m_TIMESTAMP_FILES_REGEX);
	std::filesystem::path                             ctrlFilePath(m_ctrlBaseName + logFilePath + "/");

	// Remove all TIMESTAMP "segment" files.
	for (auto const& dir_entry : std::filesystem::directory_iterator{ctrlFilePath})
	{
		if (std::filesystem::is_regular_file(dir_entry.path()))
		{
			// Filter the files that match our mask.
			if(boost::regex_search(dir_entry.path().string(), result, regexp))
			{
				// Got a segment file...
				remove(dir_entry.path().filename().string().c_str());
			}
		}
	}

	remove((ctrlFilePath.string()+ m_FSTLINEOFLOGFILE_CTRL).data());
	remove((ctrlFilePath.string()+ m_FILEORSEGMENTEOF_CTRL).data());
	remove((ctrlFilePath.string()+ m_NEXTLOGLINE2READ_CTRL).data());
	remove((ctrlFilePath.string()+ m_NEXTCATCHUP2READ_CTRL).data());
}
/*END*/
