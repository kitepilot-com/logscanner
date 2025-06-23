/*******************************************************************************
 * File: logscanner/LogManager.h                                               *
 * Created:     Mar-30-25                                                      *
 * Last update: Mar-30-25                                                      *
 * Top level header file.                                                      *
 *******************************************************************************/

#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <iostream>
#include <boost/thread.hpp>

class LogManager
{
	public:
		~LogManager();

		static void        init();
		static LogManager *getInstance();

		std::string  nanosecTimestamp();
		void         logNoMatch(std::string log, std::string line);
		void         logEvent(const char *log, std::string msg = "");
		void         consoleMsg(std::string msg = "");
	protected:
	private:
		LogManager();

		static LogManager *m_thisObj;

		mutable boost::mutex       m_linMutex;
		mutable boost::mutex       m_evtMutex;
		mutable boost::mutex       m_msgMutex;
};

#endif // LOGMANAGER_H
/*END*/
