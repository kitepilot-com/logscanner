/*******************************************************************************
 * File: logscanner/NotifyIPC.cpp                                              *
 * Created:     Aug-06-25                                                      *
 * Last update: Aug-06-25                                                      *
 * Program start.                                                              *
 *******************************************************************************/

#include "LogManager.h"
#include "NotifyIPC.h"

NotifyIPC::NotifyIPC(NotifyIPC *forwardNotify) :
	m_itemsInQueue(false),
	m_forwardNotify(forwardNotify)
{
}

NotifyIPC::~NotifyIPC()
{
}

void NotifyIPC::notifyNew()
{
//LogManager::getInstance()->consoleMsg(("DBG_NTFY_ENTERING_notifyNew").c_str());  //FIXTHIS!!!
	m_itemsInQueue.store(true);
	m_condVar.notify_one();
//LogManager::getInstance()->consoleMsg(("DBG_NTFY_EXITING_notifyNew").c_str());  //FIXTHIS!!!
}

void NotifyIPC::wait4NotifyNew()
{
//LogManager::getInstance()->consoleMsg(("DBG_NTFY_ENTERING_wait4NotifyNew").c_str());  //FIXTHIS!!!
	boost::mutex                     condVarMutex;
	boost::unique_lock<boost::mutex> lock(condVarMutex);

	while(!m_itemsInQueue.load())
	{
	    m_condVar.wait(lock);
	}

	m_itemsInQueue.store(false);
//LogManager::getInstance()->consoleMsg(("DBG_NTFY_EXITING_wait4NotifyNew").c_str());  //FIXTHIS!!!
}

bool NotifyIPC::recordsPending()
{
	bool thereAreRecordsPending = m_itemsInQueue.load();
//LogManager::getInstance()->consoleMsg(("DBG_NTFY_ENTERING_recordsPending\tm_itemsInQueue => " + (m_itemsInQueue.load() ? "T" : "F")).c_str());  //FIXTHIS!!!
	m_itemsInQueue.store(false);
	return thereAreRecordsPending;
}

NotifyIPC *NotifyIPC::forwardNotify()
{
	return m_forwardNotify;
}
/*END*/
