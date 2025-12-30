/*******************************************************************************
 * File: logscanner/NotifyEvent.h                                                *
 * Created:     Aug-06-25                                                      *
 * Last update: Aug-06-25                                                      *
 * Top level header file.                                                      *
 *******************************************************************************/

#ifndef NOTIFYEVENT_H
#define NOTIFYEVENT_H

#include <atomic>
#include <boost/thread.hpp>

class NotifyEvent
{
	public:
		NotifyEvent(NotifyEvent *forwardNotify = nullptr);
		~NotifyEvent();

		void   notifyNew();
		void   wait4NotifyNew();
		bool   recordsPending();

		NotifyEvent *forwardNotify();
	protected:
	private:
		boost::condition_variable m_condVar;

		// Flag to notify a thread that another thread added rows to a table.
		std::atomic<bool>  m_itemsInQueue;

		// Does LogScanner::execSqlInsert,need to notify anyomne after insert?
		NotifyEvent *m_forwardNotify;
};
#endif // NOTIFYEVENT_H
/*END*/
