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

std::string LogManager::nanosecTimestamp()
{
//	using namespace std::chrono;
//	return duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();
	static const int TIMESTAMP_SIZE = 19;

	std::string timestamp(std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count()));

	while(timestamp.length() < TIMESTAMP_SIZE)
	{
		timestamp += "0";
	}

	return timestamp;
}

void LogManager::logNoMatch(std::string log, std::string line)
{
	static const std::string logPath("run/logs/noMatch");

	boost::mutex::scoped_lock lock(m_linMutex);
	std::string               logName(logPath + log + ".txt");

	std::ofstream  myfile;
	myfile.open(logName.data(), std::ios_base::app);
	myfile << line << std::endl;
	myfile.close();
}

void LogManager::logEvent(const char *log, std::string msg)
{
	static const std::string logPath("run/logs/exec_");

	boost::mutex::scoped_lock lock(m_evtMutex);

	std::string timestamp(nanosecTimestamp());
	std::string logName(logPath + log + ".log");

	std::ofstream  myfile;
	myfile.open(logName.data(), std::ios_base::app);
	myfile << timestamp << "\t" << log << "\t" << msg << std::endl;
	myfile.close();
}

void LogManager::consoleMsg(std::string msg)
{
	boost::mutex::scoped_lock lock(m_msgMutex);
	std::cout << nanosecTimestamp();
	if(msg.length() > 0)
	{
		std::cout << '\t';
	}
	std::cout << msg << std::endl;
}
/*END*/
