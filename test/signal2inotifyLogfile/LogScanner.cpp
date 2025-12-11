/*******************************************************************************
 * File: LogScanner/LogScanner.cpp                                             *
 * Created:     Jan-07-25                                                      *
 * Last update: Jun-30-25                                                      *
 * Top level program file.                                                     *
 *******************************************************************************/
/*
#include <time.h>
#include <stdlib.h>
//#include <unistd.h>
//#include <sys/types.h>
#include <ctime>
#include <vector>
#include <iomanip>
#include <sstream>
#include <string>
#include <iostream>
#include <boost/regex.hpp>
#include <boost/thread.hpp>

#include "LumpContainer.h"
*/
#include <sys/socket.h>
#include <sys/inotify.h>
#include <sys/un.h>
#include <fstream>

#include "LogManager.h"
#include "LogScanner.h"

/*
pthread_t native_handle = my_thread.native_handle();
int result = pthread_kill(native_handle, SIGTERM);
*/
const char TEST_FILE[] = "tmp/tstFile.txt";

ConfContainer::ConfContainer() :
	m_logFilePath(TEST_FILE)
{
	std::ofstream  rules;
	rules.open(TEST_FILE, std::ios_base::trunc);
	rules.close();
}

ConfContainer::~ConfContainer()
{
}

const std::filesystem::path ConfContainer::getLogFilePath() const
{
	std::filesystem::path tstFile(TEST_FILE);
	return tstFile;
}

const std::string &ConfContainer::getThreadName()
{
	return m_threadName;
}

// Initial conditions:
//	m_gotLineFrmLog(false),
//	m_lineDelivered(true),
void ConfContainer::deliverToProcThreads(std::string &line)
{
	boost::mutex::scoped_lock lock(m_lineExchangeMutex);

	while(!m_lineDelivered)
	{
		m_deliverConditionVariable.wait(lock);
	}

	m_lineBuffer = line;
//LogManager::getInstance()->logEvent("DBG_deliverToProcThreads", line);  //FIXTHIS!!!
	m_gotLineFrmLog = true;
	m_lineDelivered = false;
	m_acceptLConditionVariable.notify_one();
}

void ConfContainer::acceptFromReadThread(std::string &line)
{
	boost::mutex::scoped_lock lock(m_lineExchangeMutex);

	while(!m_gotLineFrmLog)
	{
		m_acceptLConditionVariable.wait(lock);
	}

	line = m_lineBuffer;
//LogManager::getInstance()->logEvent("DBG_acceptFromReadThread", line);  //FIXTHIS!!!
	m_lineDelivered = true;
	m_gotLineFrmLog = false;
	m_deliverConditionVariable.notify_one();
}
////////////////////////////////////////////////////////////////////////////////
boost::atomic<bool>  LogScanner::KEEP_LOOPING = true;

LogScanner::LogScanner()
{
	LogManager::init();

	ConfContainer *logfileData = new ConfContainer;
	m_logfileList.push_back(logfileData);

	initCommServer(m_fdSocketSrv, m_fdAccept, m_SYS_SOCKET, m_SYS_CONNECTS);
}

LogScanner::~LogScanner()
{
}

int LogScanner::exec()
{
	LogManager::getInstance()->consoleMsg("=====> ENTERING LogScanner::exec()");

	boost::thread *comm = new boost::thread(&LogScanner::commThread, this);
	boost::thread *inpt = new boost::thread(&LogScanner::inptThread, this);

	comm->join();
	inpt->join();

	delete comm;
	delete inpt;

	LogManager::getInstance()->consoleMsg("==> All threads finished...");
	LogManager::getInstance()->consoleMsg("==> Bye bye cruel World");

	return 0;
}

bool LogScanner::initCommServer(int &fdSocketSrv, int &fdAccept, const char* socketPath, int maxConnections)
{
	// The main purpose of this function is to prevent 2 instances of the program running.
	// But it could be used for other things...
	LogManager::getInstance()->consoleMsg("=====> ENTERING void LogScanner::initCommServer()");
	LogManager::getInstance()->consoleMsg();

	struct sockaddr_un local;
	int                dataSize = 0;
	bool               allGood  = false;
//FIXTHIS!!!
	fdSocketSrv = socket(AF_UNIX, SOCK_STREAM, 0);

	if(fdSocketSrv > -1)
	{
		memset(&local, 0x00, sizeof(struct sockaddr_un));
		local.sun_family = AF_UNIX;
		strcpy(&local.sun_path[1], socketPath);
		dataSize = strlen(&local.sun_path[1]) + sizeof(local.sun_family) + 1;

		if(bind(fdSocketSrv, (struct sockaddr*)&local, dataSize) == 0)
		{
			if(listen(fdSocketSrv, maxConnections) == 0 )
			{
				LogManager::getInstance()->consoleMsg(("Socket " + std::string(socketPath) + " is listening.").c_str()); //FIXTHIS!!!  Do I want this here?
				allGood = true;
			}
			else
			{
				LogManager::getInstance()->consoleMsg("*** FATAL *** Error calling 'listen(fdSocketSrv, connections)'");
			}
		}
		else
		{
			LogManager::getInstance()->consoleMsg("*** FATAL *** Error calling 'bind(fdSocketSrv, (struct sockaddr*)&local, dataSize)'");
		}
	}
	else
	{
		LogManager::getInstance()->consoleMsg("*** FATAL *** Error calling 'fdSocketSrv = socket(AF_UNIX, SOCK_STREAM, 0)'");
	}

	LogManager::getInstance()->consoleMsg("=====> EXITING void LogScanner::initCommServer()");
	LogManager::getInstance()->consoleMsg();

	return allGood;
}

void LogScanner::commThread()
{
	LogManager::getInstance()->consoleMsg(">> commThread => Hello World");

	struct sockaddr remote;
//FIXTHIS!!!   All this...
	while (KEEP_LOOPING)
	{
		unsigned int sockLen = 0;

		memset(&remote, 0x00, sizeof(struct sockaddr));
LogManager::getInstance()->consoleMsg("DBG Going to accept with /m_fdSocketSrv,");
		if((m_fdAccept = accept(m_fdSocketSrv, (struct sockaddr*)&remote, &sockLen)) > 0)
		{
			LogManager::getInstance()->consoleMsg(("comm server accepting connections in " + std::string(m_SYS_SOCKET)).c_str());

			// We will get an empty string, 2 should be sufficient...
			char recvBuff[2] = { 0x00 };
			int  dataRecv    = recv(m_fdAccept, recvBuff, sizeof(recvBuff), 0);

			if(dataRecv > 0)
			{
KEEP_LOOPING = false;       //FIXTHIS!!!
//				if(send(m_fdAccept, line.data(), line.length(), 0) == -1)
//				{
//					LogManager::getInstance()->consoleMsg("**** ERROR ****");
//				}
			}

			close(m_fdAccept);
		}
		else
		{
			LogManager::getInstance()->consoleMsg("** accept error in LogScanner::commThread()");
		}
	}

	LogManager::getInstance()->consoleMsg(">> commThread => Bye bye cruel World");
}

void LogScanner::inptThread()
{
	// a.- The Input Thread:
	// ---------------------
	// This main thread starts as many "reading threads" as there are configuration files.
	LogManager::getInstance()->consoleMsg(">> inptThread => Hello World");

	std::vector<boost::thread *> allThreads;

	for(auto logfileData : m_logfileList)
	{
		// Start a "Reading Thread" for every logfile.
		boost::thread *thread = new boost::thread(&LogScanner::logReadingThread, this, logfileData);
		allThreads.push_back(thread);
	}

	for(boost::thread *thread : allThreads)
	{
		thread->join();
	}

	for(boost::thread *thread : allThreads)
	{
		delete thread;
	}

	LogManager::getInstance()->consoleMsg(">> inptThread => Bye bye cruel World");
}

void LogScanner::logReadingThread(ConfContainer *logfileData)
{
	// Each line read by the Input Thread is handed via queue to any available standby "processing thread".
	// The "processing thread" hands the line to all "regexp eval threads" and waits for the result of the regexp evaluation.
	LogManager::getInstance()->consoleMsg((">>>>  STARTING logReadingThread for " + logfileData->getLogFilePath().string()).c_str());

	boost::atomic<bool> restartThread = false;
//	std::unique_ptr<boost::thread> MonThread(new boost::thread(&LogScanner::monitorFirstLine, this, logfileData->getLogFilePath(), boost::ref(restartThread)));

	while(KEEP_LOOPING)
	{
		while(KEEP_LOOPING && !restartThread)
		{
			std::vector<boost::thread *>   allThreads;
			boost::mutex                   writeMutex;

			// Start reading threads for all "LUMP" components.
//			std::string  lumpComponent(logfileData->getLumpComponent());
//			if(lumpComponent != "")
//			{
//				do
//				{
//					boost::thread *thread = new boost::thread(&LogScanner::lumpReadingThread, this, &writeMutex, logfileData->getLogFilePath(), lumpComponent, boost::ref(restartThread));
//					allThreads.push_back(thread);

//					lumpComponent = logfileData->getLumpComponent();
//				}
//				while(lumpComponent != "");
//			}

			// Start m_totThreads "processing threads".
//			boost::atomic<bool> threadAlive = true;
//			for(int procThreadID = 0; procThreadID < m_totThreads; procThreadID++)
//			{
//				boost::thread *thread = new boost::thread(&LogScanner::processingThread, this, logfileData, procThreadID, boost::ref(threadAlive));
//				allThreads.push_back(thread);
//			}

			// Open log file to read.
			std::ifstream ifs(logfileData->getLogFilePath(), std::ios::in/* | std::ios_base::binary*/);

			if(ifs.is_open())
			{
				LogManager::getInstance()->consoleMsg(("File (logReadingThread) good\t=>\t" + logfileData->getLogFilePath().string()).c_str());

				// Find the end of the file.
				// We'll start a "catch up" thread to read from the beggining of the file to whatever the EOF is now.
				// New lines will be read immediatelly to catch ongoing threats.
//				ifs.seekg (0, ifs.end);
//				int lastBite = ifs.tellg();
//				boost::thread *thread = new boost::thread(&LogScanner::catchUpLogThread, this, logfileData, lastBite);
//				allThreads.push_back(thread);

				// Loop to read the log file.
				// Some "processing thread" will pick up the line.
				while(KEEP_LOOPING && !restartThread)
				{
					// Read incoming line...
					std::string line;
					while(KEEP_LOOPING && !restartThread && std::getline(ifs, line) && !ifs.eof())
					{
						// We only care for lines that have IP address.
//						if(hasIPaddress(line))
//						{
//							if(notEverSeen(line))//FIXTHIS!!!
//							{
LogManager::getInstance()->logEvent(("DBG_TIME_1_logReadingThread_GOT_from" + std::string(logfileData->getThreadName())).c_str(), (createTimestamp() + "\t" + line).c_str());  //FIXTHIS!!!
								// Send a line to logfileData->acceptFromReadThread(line);
								logfileData->deliverToProcThreads(line);
//							}
// 						}
//						else LogManager::getInstance()->logEvent(("DBG_logReadingThread" + std::string(logfileData->getThreadName()) + "_NO_IP").data(), line);  //FIXTHIS!!!
					}

					if(KEEP_LOOPING && !restartThread && ifs.eof())
					{
						ifs.clear();
						// Now wait until a new line is added to the file, but...
						// There is a race condition here.
						// A line can be added to the file before the inotify gets "installed".
						// AFAIK that condition is impossible to manage unless we can control the process that adds the lines.
						// And we can't control the process that adds the lines...
						// Ah well...
						// The line will sit there until another line is added...
LogManager::getInstance()->consoleMsg("DBG => calling inotifyLogfile from`logReadingThread");
						inotifyLogfile(logfileData->getLogFilePath());
LogManager::getInstance()->consoleMsg("DBG => returning from inotifyLogfile to`logReadingThread");
					}
				}
				ifs.close();
			}
			else
			{
				LogManager::getInstance()->consoleMsg(("*** FATAL *** unable to open logfile " + logfileData->getLogFilePath().string() + " or start comm sever for " +  + "...").c_str());
			}

			for(boost::thread *thisThread : allThreads)
			{
				thisThread->join();
			}

			for(boost::thread *thisThread : allThreads)
			{
				delete thisThread;
			}
		}

		restartThread = false;
	}

//	MonThread->join();

	LogManager::getInstance()->consoleMsg((">>>>  EXITING logReadingThread for " + logfileData->getLogFilePath().string()).c_str());
}

void LogScanner::lumpReadingThread(boost::mutex *writeMutex, std::filesystem::path logFilePath, std::string lumpComponent, boost::atomic<bool> &restartThread)
{
	LogManager::getInstance()->consoleMsg((">>>>  STARTING lumpReadingThread for " + logFilePath.string() + " => " + lumpComponent).c_str());

//	std::unique_ptr<boost::thread> MonThread(new boost::thread(&LogScanner::monitorFirstLine, this, logFilePath, boost::ref(restartThread)));

	std::ifstream ifs(lumpComponent, std::ios::in/* | std::ios_base::binary*/);
	if(ifs.is_open() && !restartThread)
	{
		LogManager::getInstance()->consoleMsg(("Lump component " + lumpComponent + " for " + logFilePath.string() + " is good.").c_str());

		while(KEEP_LOOPING && !restartThread)
		{
			// Read incoming line...
			std::string line;
			while(KEEP_LOOPING && !restartThread && std::getline(ifs, line))
			{
				// We only care for lines that have IP address.
//				if(hasIPaddress(line))
//				{
//LogManager::getInstance()->logEvent(("lumpReadingThread_writeLineToLumpFile_" + idstr + "_GOT").data(), line);  //FIXTHIS!!!
//					writeLineToLumpFile(writeMutex, lumpComponent + '	' + line,  logFilePath);
//				}
			}

			if(KEEP_LOOPING && !restartThread && ifs.eof())
			{
				ifs.clear();
				inotifyLogfile(lumpComponent);
			}
		}
		ifs.close();
	}

//	MonThread->join();

	LogManager::getInstance()->consoleMsg((">>>>  EXITING lumpReadingThread for " + logFilePath.string() + " => " + lumpComponent).c_str());
}

std::string LogScanner::createTimestamp()
{
	// It's highly unlikely but we can get a duplicated timestamp.
	char  timestamp[LogManager::TIMESTAMP_SIZE + 1];
	LogManager::getInstance()->nanosecTimestamp(timestamp);
//	boost::mutex::scoped_lock lock(m_mutex4Timestamp);
////FIXTHIS!!!   This function seems to be duplicated, see LogManager..?.
//	static const int TIMESTAMP_SIZE = 19;
//	std::string timestamp(std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count()));
////	std::string timestamp(std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count()));

	return timestamp;
}

void LogScanner::inotifyLogfile(std::filesystem::path logFilePath)
{
// SEE:
// https://stackoverflow.com/questions/32281277/too-many-open-files-failed-to-initialize-inotify-the-user-limit-on-the-total
// echo 2048 > /proc/sys/user/max_inotify_instances
//std::cout << LogManager::getInstance()->nanosecTimestamp() << '\t' << "::::::> DBG   ENTERING LogScanner::inotifyLogfile(" << logFilePath.string() << ")" << std::endl;
	// From: https://developer.ibm.com/tutorials/l-ubuntu-inotify/
	std::string logPath(logFilePath.parent_path().string());
	std::string logName(logFilePath.filename().string());
LogManager::getInstance()->consoleMsg(("DBG => inotifyLogfile => " + logPath + " => " + logName).c_str());  //FIXTHIS!!!
	int EVENT_SIZE = ( sizeof (struct inotify_event) );
	int BUF_LEN    = ( 1024 * ( EVENT_SIZE + 16 ) );

	int length, Idx = 0;
	int fd;
	int wd;
	char buffer[BUF_LEN];

	bool fileChanged = false;

	fd = inotify_init();

	if(fd < 0 ) {
		perror( "inotify_init" ); //FIXTHIS!!!
	}

	wd = inotify_add_watch( fd, logPath.data(), IN_MODIFY | IN_CREATE | IN_DELETE );
LogManager::getInstance()->consoleMsg("DBG => in inotifyLogfile and going to read( fd, buffer, BUF_LEN );");  //FIXTHIS!!!
	length = read( fd, buffer, BUF_LEN );
LogManager::getInstance()->consoleMsg("DBG => in inotifyLogfile and OUT of read( fd, buffer, BUF_LEN );");  //FIXTHIS!!!
	if(length < 0 ) {
		perror( "read" ); //FIXTHIS!!!
	}

	while(KEEP_LOOPING && Idx < length && !fileChanged)
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
/*END*/
