/*******************************************************************************
 * File: logScanner/ConfContainer.cpp                                          *
 * Created:     Jan-07-25                                                      *
 * Last update: May-17-25                                                      *
 * Top level program file.                                                     *
 *******************************************************************************/

#include <iostream>

#include "LogManager.h"
#include "ConfContainer.h"

ConfContainer::ConfContainer(std::string logFilePath, std::map<char, std::string> dateExtrct, std::map<std::string, char> regexpList) :
	m_state(INVALID),
	m_gotLineFrmLog(false),
	m_lineDelivered(true),
	m_logFilePath(logFilePath),
	m_threadName(logFilePath),
	m_dateExtrct(dateExtrct)
{
	for(auto regexp : regexpList)
	{
		m_regexpSvrt.push_back(regexp.second);
		m_regexpText.push_back(regexp.first);
	}

	std::replace( m_threadName.begin(), m_threadName.end(), '/', '_');
	std::replace( m_threadName.begin(), m_threadName.end(), '.', '_');
	std::replace( m_threadName.begin(), m_threadName.end(), '-', '_');

	m_state = ConfContainer::HEALTHY;
}

ConfContainer::~ConfContainer()
{
}

std::string ConfContainer::getLumpComponent()
{
	return "";
}

ConfContainer::StatusVal ConfContainer::getObjStatus()
{
	return m_state;
}

const std::filesystem::path ConfContainer::getLogFilePath() const
{
	return m_logFilePath;
}

const char *ConfContainer::getThreadName()
{
	return m_threadName.data();
}

std::string ConfContainer::getDateExtract(DATE_EXTRACT key)
{
	std::string val;

	auto it = m_dateExtrct.find((char)key);

	if (it != m_dateExtrct.end())
	{
		val = it->second;
	}

	return val;
}

int ConfContainer::getRegexpCount()
{
	return m_regexpSvrt.size();
}

std::string ConfContainer::getRegexpSvrt(int rgxpThreadID)
{
	return std::string(1, m_regexpSvrt[rgxpThreadID]);
}

std::string ConfContainer::getRegexpText(int rgxpThreadID)
{
	return m_regexpText[rgxpThreadID];
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
/*END*/
