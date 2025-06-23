/*******************************************************************************
 * File: logscanner/Workarea.h                                                 *
 * Created:     Jan-07-25                                                      *
 * Last update: May-17-25                                                      *
 * Top level header file.                                                      *
 *******************************************************************************/

#ifndef WORKAREA_H
#define WORKAREA_H

#include <string>
#include <vector>
#include <array>

class ConfContainer;
class Workarea
{
	// There is one Workarea for every log file processingThread.
	public:
		Workarea(ConfContainer *logfileData, int procThreadID);
		virtual ~Workarea();

		void           saveIPaddreess(std::string IPaddr);
		void           rcvrIPaddreess(std::array<std::string, 4> &ipAddr);
		std::string    getThreadName();
		std::string    getProcThreadID();
//		void           setTimestamp(std::string timestamp);
//		std::string   &getTimestamp();
		std::string    getRegexpText(int rgxpThreadID);
		bool          *getRegexpPtrResult(int rgxpThreadID);
		void           addMatchCount();
		int            getMatchCount();
		void           clearMatchCount();
		void           setWhitelisted(bool whitelisted);
		bool           isWhitelisted();
	protected:
	private:
	ConfContainer               *m_logfileData;
	mutable boost::mutex         m_regexpSyncMtx;
	std::string                  m_procThreadID;
	bool                        *m_regexpResults;
	std::array<std::string, 4>   m_ipAddr;
//	std::string                  m_timestamp;
	int                          m_matchCount;
	bool                         m_whitelisted;
};

#endif // WORKAREA_H
/*END*/
