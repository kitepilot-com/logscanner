/*******************************************************************************
 * File: logScanner/Workarea.cpp                                          *
 * Created:     Jan-07-25                                                      *
 * Last update: May-17-25                                                      *
 * Top level program file.                                                     *
 *******************************************************************************/

#include <iostream>

#include "LogManager.h"
#include "Workarea.h"
#include "ConfContainer.h"

Workarea::Workarea(ConfContainer *logfileData, int procThreadID) :
	m_logfileData(logfileData),
	m_procThreadID("0" + std::to_string(procThreadID)),
	m_regexpResults(new bool[m_logfileData->getRegexpCount()]),
	m_ipAddr( { "0", "0", "0", "0" } ),
	m_matchCount(0)
{
	m_procThreadID = m_procThreadID.substr(m_procThreadID.length() - 2, 2);

	for(int Idx = 0; Idx < m_logfileData->getRegexpCount(); Idx++)
	{
		m_regexpResults[Idx] = false;
	}
}

Workarea::~Workarea()
{
	delete [] m_regexpResults;
}

void Workarea::saveIPaddreess(std::string IPaddr)
{
	std::stringstream ss(IPaddr);

	for(int Idx = 0; Idx < 4; Idx++)
	{
		getline(ss, m_ipAddr[Idx], '.');
	}
}

void Workarea::rcvrIPaddreess(std::array<std::string, 4> &ipAddr)
{
	ipAddr[0] = m_ipAddr[0];
	ipAddr[1] = m_ipAddr[1];
	ipAddr[2] = m_ipAddr[2];
	ipAddr[3] = m_ipAddr[3];
}

//std::string Workarea::getOctect(int Idx)
//{
//	return m_ipAddr[Idx];
//}

std::string Workarea::getThreadName()
{
	return m_logfileData->getThreadName();
}

std::string Workarea::getProcThreadID()
{
	return m_procThreadID;
}

//void Workarea::setTimestamp(std::string timestamp)
//{
//	m_timestamp = timestamp;
//}

//std::string &Workarea::getTimestamp()
//{
//	return m_timestamp;
//}

std::string Workarea::getRegexpText(int rgxpThreadID)
{
	return m_logfileData->getRegexpText(rgxpThreadID);
}

bool *Workarea::getRegexpPtrResult(int rgxpThreadID)
{
	return &(m_regexpResults[rgxpThreadID]);
}

void Workarea::addMatchCount()
{
	boost::mutex::scoped_lock lock(m_regexpSyncMtx);
	m_matchCount++;
}

int Workarea::getMatchCount()
{
	return m_matchCount;
}

void Workarea::clearMatchCount()
{
	m_matchCount = 0;
}

void Workarea::setWhitelisted(bool whitelisted)
{
	m_whitelisted = whitelisted;
}

bool Workarea::isWhitelisted()
{
	return m_whitelisted;
}
/*END*/
