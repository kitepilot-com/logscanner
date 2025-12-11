/*******************************************************************************
 * File: logScanner/ConveyorBelt.cpp                                           *
 * Created:     Sep-28-25                                                      *
 * Last update: Sep-28-25                                                      *
 * Top level program file.                                                     *
 *******************************************************************************/

#include <thread>
#include <chrono>

#include "LogManager.h"
#include "ConveyorBelt.h"

// To resolve "100ms" ...
using namespace std::chrono_literals;

// This set holds the semaphore names because they have to be unique sstemwide and we keep a tab here.
std::set<std::string>  ConveyorBelt::m_listOfSemaphores;

ConveyorBelt::ConveyorBelt(std::string uniqueSemaphoreName, int maxQueueSize) :
	m_maxQueueSize(maxQueueSize),
	m_loopSemaphoreName("loop_" + uniqueSemaphoreName),
	m_exitSemaphoreName("exit_" + uniqueSemaphoreName)
{
	createSemaphore(m_loopSemaphoreName, &m_loopSemaphore);
	createSemaphore(m_exitSemaphoreName, &m_exitSemaphore);
}

ConveyorBelt::~ConveyorBelt()
{
	boost::interprocess::named_semaphore::remove(m_loopSemaphoreName.c_str());
	m_listOfSemaphores.erase(m_loopSemaphoreName);
    delete m_loopSemaphore;

	boost::interprocess::named_semaphore::remove(m_exitSemaphoreName.c_str());
	m_listOfSemaphores.erase(m_exitSemaphoreName);
    delete m_exitSemaphore;
}

void ConveyorBelt::deliverToProcThreads(std::string &line)
{
	boost::mutex                     condVarMutex;
	boost::unique_lock<boost::mutex> lock(condVarMutex);

	while(m_lineBuffer.size() == m_maxQueueSize)
	{
		m_deliverConditionVariable.wait(lock);
	}

	m_queueGuard.lock();
	m_lineBuffer.push(line);
	m_queueGuard.unlock();

	m_loopSemaphore->post();
}

void ConveyorBelt::acceptFromReadThread(std::string &line)
{
	m_loopSemaphore->wait();

	m_queueGuard.lock();
	line = m_lineBuffer.front();
	m_lineBuffer.pop();
	m_queueGuard.unlock();

	m_deliverConditionVariable.notify_one();
}

void ConveyorBelt::shutDown()
{
	// Wait until all the readers are idle.
	while(m_lineBuffer.size() > 0)
	{
		// This is a one-time deal and it's easier and cheaper than risking race conditions...
		// So, sleep.   Zzzzzzzzzzzzzzzzzzzzzzzzzz....
		std::this_thread::sleep_for(100ms);
	}

	std::string line("EXIT");
	for(std::size_t procThreadID = 0; procThreadID < m_maxQueueSize; procThreadID++)
	{
		deliverToProcThreads(line);
		m_exitSemaphore->wait();
	}
}

void ConveyorBelt::notifyExit()
{
	m_exitSemaphore->post();
}

void ConveyorBelt::createSemaphore(std::string &uniqueSemaphoreName, boost::interprocess::named_semaphore **semaphore)
{
	if(m_listOfSemaphores.find(uniqueSemaphoreName) == m_listOfSemaphores.end())
	{
		LogManager::getInstance()->consoleMsg(("====> INSERTING semaphore name " + std::string(uniqueSemaphoreName)).c_str());
		m_listOfSemaphores.insert(uniqueSemaphoreName);
	}
	else
	{
		LogManager::getInstance()->consoleMsg(("*** FATAL *** ()\tDUPLICATE semaphore name " + std::string(uniqueSemaphoreName)).c_str());
		throw;
	}

	try
	{
		boost::interprocess::named_semaphore::remove(uniqueSemaphoreName.c_str());
		boost::interprocess::named_semaphore(boost::interprocess::create_only_t(), uniqueSemaphoreName.c_str(), 0);

		*semaphore = new boost::interprocess::named_semaphore(boost::interprocess::open_only_t(), uniqueSemaphoreName.c_str());
	}
	catch(...)
	{
		LogManager::getInstance()->consoleMsg(("*** FATAL *** Unable to open semaphore " + uniqueSemaphoreName).c_str());
		throw;
	}
}
/*END*/
