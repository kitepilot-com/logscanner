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

		void                saveIPaddreess(std::array<std::string, 4> &ipAddr, bool whitelisted);
		void                getIPaddrOctects(std::array<std::string, 4> &ipAddr);
		const std::string  &getThreadName();
		const std::string  &getProcThreadID();
		const std::string  &getRegexpText(int rgxpThreadID);
		bool               *getRegexpPtrResult(int rgxpThreadID);
		int                 getMatchCount();
		void                clearMatchCount();
//		void                setWhitelisted(bool whitelisted);
		bool                isWhitelisted();
	protected:
//		void                addMatchCount();
	private:
	ConfContainer               *m_logfileData;
	mutable boost::mutex         m_regexpSyncMtx;
	std::string                  m_procThreadID;
	bool                        *m_regexpResults;
	std::array<std::string, 4>   m_ipAddr;
	int                          m_matchCount;
	bool                         m_whitelisted;
};

#endif // WORKAREA_H
/*END*/
