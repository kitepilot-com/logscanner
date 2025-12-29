/*******************************************************************************
 * File: logscanner/LogManager.cpp                                             *
 * Created:     Jan-07-25                                                      *
 * Last update: Jan-07-25                                                      *
 * Program start.                                                              *
 *******************************************************************************/

#include <fstream>
#include <chrono>

#include "LogManager.h"

LogManager *LogManager::m_thisObj = nullptr;

LogManager::LogManager()
{
int JUNK_ADD_PARENTS_AND_LEVELS_TO_REPORTS; //FIXTHIS!!!
}

LogManager::~LogManager()
{
}

void LogManager::init()
{
	if(m_thisObj == nullptr)
	{
		m_thisObj = new LogManager;;
	}
	else
	{
		throw; //FIXTHIS!!!   Any better way to do this?
	}

}

LogManager *LogManager::getInstance()
{
	return m_thisObj;
}

void LogManager::nanosecTimestamp(char *timestamp)
{
//	static const int  TIMESTAMP_SIZE = 19;
	struct            timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);

	//  It is *YOUR* responsibility to send a 'timestamp' big enough for this...
	snprintf(timestamp, TIMESTAMP_SIZE + 1, "%lld", (long long int)(ts.tv_sec) * (long long int)1000000000 + (long long int)(ts.tv_nsec));
}

void LogManager::logEvent(const char *log, std::string *str)
{
	const char *msg = nullptr;

	if(str != nullptr)
	{
		msg = str->c_str();
	}

	logEvent(log, msg);
}

void LogManager::logEvent(const char *log, const char *msg)
{
	m_evtMutex.lock();

	static const std::string logPath("run/logs/exec_");
	std::string              logName(logPath + log + ".log");
	char                     timestamp[20];

	nanosecTimestamp(timestamp);

	std::ofstream  myfile;
	myfile.open(logName.data(), std::ios_base::app);
	myfile << timestamp << "\t" << log << "\t" << msg << std::endl;
	myfile.close();

	m_evtMutex.unlock();
}

void LogManager::consoleMsg(std::string *str)
{
	const char *msg = nullptr;

	if(str != nullptr)
	{
		msg = str->c_str();
	}

	consoleMsg(msg);
}

void LogManager::consoleMsg(const char *msg)
{
	m_msgMutex.lock();

	char timestamp[20];

	nanosecTimestamp(timestamp);
	std::cout << timestamp;

	if(msg != nullptr && strlen(msg) > 0)
	{
		std::cout << '\t' << msg << "\t(" + std::to_string((pid_t)syscall(__NR_gettid)) << ")";
	}
	std::cout  << std::endl;

	m_msgMutex.unlock();
}
/*END*/
