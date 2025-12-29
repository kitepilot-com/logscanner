/*******************************************************************************
 * File: logscanner/NotifyIPC.h                                                *
 * Created:     Aug-06-25                                                      *
 * Last update: Aug-06-25                                                      *
 * Top level header file.                                                      *
 *******************************************************************************/

#ifndef NOTIFYIPC_H
#define NOTIFYIPC_H

#include <atomic>
#include <boost/thread.hpp>

class NotifyIPC
{
	public:
		NotifyIPC(NotifyIPC *forwardNotify = nullptr);
		~NotifyIPC();

		void   notifyNew();
		void   wait4NotifyNew();
		bool   recordsPending();

		NotifyIPC *forwardNotify();
	protected:
	private:
		boost::condition_variable m_condVar;

		// Flag to notify a thread that another thread added rows to a table.
		std::atomic<bool>  m_itemsInQueue;

		// Does LogScanner::execSqlInsert,need to notify anyomne after insert?
		NotifyIPC *m_forwardNotify;
};
#endif // NOTIFYIPC_H
/*END*/
