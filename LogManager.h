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

		static constexpr const int  TIMESTAMP_SIZE = 19;

		static void        init();
		static LogManager *getInstance();

		void  nanosecTimestamp(char *timestamp);
//		void  logNoMatch(std::string log, std::string line);
		void  logEvent(const char *log, std::string *str);
		void  logEvent(const char *log, const char *msg = nullptr);
		void  consoleMsg(std::string *str);
		void  consoleMsg(const char *msg = nullptr);
	protected:
	private:
		LogManager();

		static LogManager *m_thisObj;

		mutable boost::mutex  m_evtMutex;
		mutable boost::mutex  m_msgMutex;
};

#endif // LOGMANAGER_H
/*END*/
