/*******************************************************************************
 * File: logscanner/ConfContainer.h                                            *
 * Created:     Jan-07-25                                                      *
 * Last update: May-17-25                                                      *
 * Top level header file.                                                      *
 *                                                                             *
 * This class contains all the information about a specific log file.          *
 * There is a instance of this clas foe every logfile specified in the         *
 " "config" directory.                                                         *
 *******************************************************************************/

#ifndef CONF_CONTAINER_H
#define CONF_CONTAINER_H

#include <filesystem>
#include <string>
#include <vector>
#include <map>
#include <boost/thread.hpp>

class ConfContainer
{
	public:
		enum StatusVal {
			INVALID = -1,
			READLOGS_ERR,
			FEED_ERR,
			LUMP_ERR,
			READDIR_ERR,
			LUMFLAG_ERR,
			HEALTHY
		};

		typedef enum {
			REGEXP = 'R',
			BLDMAP = 'M',
			FORMAT = 'F'
		} DATE_EXTRACT;

		ConfContainer(std::string logFilePath, std::map<char, std::string> dateExtrct, std::map<std::string, char> regexpList);
		virtual ~ConfContainer();

//		virtual bool        allIsGood();
		virtual std::string getLumpComponent();

		StatusVal getObjStatus();

		const std::filesystem::path      getLogFilePath() const;
		const char                      *getThreadName();
		std::string                      getDateExtract(DATE_EXTRACT key);
		int                              getRegexpCount();
		std::string                      getRegexpSvrt(int rgxpThreadID);
		std::string                      getRegexpText(int rgxpThreadID);

		void                             deliverToProcThreads(std::string &line);
		void                             acceptFromReadThread(std::string &line);
	protected:
		StatusVal m_state;
	private:
		// Stop reading loop un til last buffer has been read.
		mutable boost::mutex      m_lineExchangeMutex;
		boost::condition_variable m_deliverConditionVariable;
		boost::condition_variable m_acceptLConditionVariable;
		bool                      m_gotLineFrmLog;
		bool                      m_lineDelivered;

		std::string                    m_lineBuffer;
		std::filesystem::path          m_logFilePath;
		std::string                    m_threadName;
		std::map<char, std::string>    m_dateExtrct;
		std::vector<char>              m_regexpSvrt;
		std::vector<std::string>       m_regexpText;
};

#endif // CONF_CONTAINER_H
/*END*/
