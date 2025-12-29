/*******************************************************************************
 * File: logscanner/INotifyObj.h                                               *
 * Created:     Dec-17-25                                                      *
 * Last update: Dec-17-25                                                      *
 * Top level header file.                                                      *
 *******************************************************************************/

#ifndef INOTIFYOBJ_H
#define INOTIFYOBJ_H

#include <atomic>
#include <boost/thread.hpp>

class INotifyObj
{
	public:
		typedef enum {
			EVENT_INVALID = -1,
			EVENT_FALSE   = 0,
			EVENT_TRUE    = 1
		} Outcome;

		INotifyObj(std::string inotifyExitFlag = "inotifyExitFlag");
		virtual ~INotifyObj();

		// This function is used by the signal handler to remove the file and trigger the exit.
		static std::string getIinotifyExitFlag();

		static void intfThread(std::set<std::string> &inotifyTargetPaths);

		// Result below is used to loop over the condition variable with a number lower than 0.
		// If the file is released becuase it was renamed or a line was added result will hold 1.
		// If the file is released becuase the program is exiting or another file was renamed result will hold 0.
		INotifyObj::Outcome  monitorNewLineThread(std::string inotifyTarget);
		INotifyObj::Outcome  monitor4RenameThread(std::string inotifyTarget);

		// Functions below release the monitoring of files to force thread exit.
		void releaseMonitorNewLineThread(std::string inotifyTarget);
		void releaseMonitor4RenameThread(std::string inotifyTarget);
	protected:
		typedef enum {
			_IN_MODIFY = 0,
			_IN_DELETE = 1
		} MonType;

		void releaseAllMonitorsFor(std::string key2find);

		void monitorNewLine(std::string inotifyTarget, INotifyObj::Outcome *result, boost::condition_variable *condVar);
		void monitor4Rename(std::string inotifyTarget, INotifyObj::Outcome *result, boost::condition_variable *condVar);

		void addFile2inotify(const std::string &monType, std::string inotifyTarget, INotifyObj::Outcome *result, boost::condition_variable *condVar = nullptr);

		static void deleteMeAndNotifyOwner(std::multimap<std::string, std::pair<INotifyObj::Outcome *, boost::condition_variable *>>::iterator iterInotifyTargetFiles, INotifyObj::Outcome outcome);
	private:
		static std::multimap<std::string, std::pair<INotifyObj::Outcome *, boost::condition_variable *>>  m_inotifyTargets;
		static std::string                                                                                m_inotifyExitFlag;
		static boost::mutex                                                                               m_mutex;
};
#endif // INOTIFYOBJ_H
/*END*/
