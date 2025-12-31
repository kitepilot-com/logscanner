/*******************************************************************************
 * File: logscanner/ConveyorBelt.h                                             *
 * Created:     Sep-28-25                                                      *
 * Last update: Sep-28-25                                                      *
 * Top level header file.                                                      *
 *                                                                             *
 * This class contains the elements to transport lines between threads.        *
 *******************************************************************************/

#ifndef CONVEYOR_BELT_H
#define CONVEYOR_BELT_H

#include <set>
#include <queue>
#include <string>
#include <atomic>
#include <boost/thread.hpp>
#include <boost/interprocess/sync/named_semaphore.hpp>

class ConveyorBelt
{
	public:
		ConveyorBelt(std::string uniqueSemaphoreName, int maxQueueSize);
		virtual ~ConveyorBelt();

		void  deliverToProcThreads(std::string &line);
		void  acceptFromReadThread(std::string &line);
		void  shutDown();
		void  notifyExit();
	protected:
		void  createSemaphore(std::string &uniqueSemaphoreName, boost::interprocess::named_semaphore **semaphore);
	private:
		// Validate that we don't have a duplicate name.
		static std::set<std::string>  m_listOfSemaphores;

		// Max readers count.
		std::size_t  m_maxQueueSize;

		// Line buffer.
		std::queue<std::string> m_lineBuffer;

		// Stuff to control flow.
		mutable boost::mutex      m_queueGuard;
		boost::condition_variable m_deliverConditionVariable;

		std::string                           m_loopSemaphoreName;
		boost::interprocess::named_semaphore *m_loopSemaphore;

		std::string                           m_exitSemaphoreName;
		boost::interprocess::named_semaphore *m_exitSemaphore;
};
#endif // CONVEYOR_BELT_H
/*END*/
