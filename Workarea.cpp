/*******************************************************************************
 * File: logScanner/Workarea.cpp                                          *
 * Created:     Jan-07-25                                                      *
 * Last update: May-17-25                                                      *
 * Top level program file.                                                     *
 *******************************************************************************/

#include <iostream>
#include <boost/thread.hpp>

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

void Workarea::saveIPaddreess(std::array<std::string, 4> &ipAddr, bool whitelisted)
{
	// We will only hit this function in parallel when 2 regexps match the same line.
	// That sould not happen, fix the regexp, the line will be flagged in "lines seen".
	boost::mutex regexpSyncMtx;
	regexpSyncMtx.lock();

	m_ipAddr[0] = ipAddr[0];
	m_ipAddr[1] = ipAddr[1];
	m_ipAddr[2] = ipAddr[2];
	m_ipAddr[3] = ipAddr[3];
	m_matchCount++;

	m_whitelisted = whitelisted;

	regexpSyncMtx.unlock();
}

void Workarea::getIPaddrOctects(std::array<std::string, 4> &ipAddr)
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

const std::string &Workarea::getThreadName()
{
	return m_logfileData->getThreadName();
}

const std::string &Workarea::getProcThreadID()
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

const std::string &Workarea::getRegexpText(int rgxpThreadID)
{
	return m_logfileData->getRegexpText(rgxpThreadID);
}

bool *Workarea::getRegexpPtrResult(int rgxpThreadID)
{
	return &(m_regexpResults[rgxpThreadID]);
}

int Workarea::getMatchCount()
{
	// It is safe to NOT mutex here because this is never accessed until
	// it is set and all the LogScanner::regexpExecThread are done.
	return m_matchCount;
}

void Workarea::clearMatchCount()
{
	// It is safe to NOT mutex here because this is never accessed until
	// it is set and all the LogScanner::regexpExecThread are done.
	m_matchCount = 0;
}
/*
void Workarea::setWhitelisted(bool whitelisted)
{
	m_whitelisted = whitelisted;
}
*/
bool Workarea::isWhitelisted()
{
	return m_whitelisted;
}
/*END*/
