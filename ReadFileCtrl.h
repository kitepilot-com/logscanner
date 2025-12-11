/*******************************************************************************
 * File: logscanner/ReadFileCtrl.h                                             *
 * Created:     Oct-01-25                                                      *
 * Last update: Oct-01-25                                                      *
 * Top level header file.                                                      *
 *                                                                             *
 * This class contains all the information to live-read a log file.            *
 *******************************************************************************/

#ifndef READFILECTRL_H
#define READFILECTRL_H

#include <fstream>
#include <string>

#include "LogManager.h"

class ReadFileCtrl
{
	public:
		ReadFileCtrl(std::string logFilePath);
		virtual ~ReadFileCtrl();

		bool initReadLogThread();
		void stopReadLogThread();

		bool initCatchUpThread();
		void stopCatchUpThread();

		bool  fileHasBackLog();
        bool  getNextLogLine(std::string &line);
        bool  getCatchupLine(std::string &line);
		void  writePosOfNextLogline2Read();
		void  writePosOfNextCatchup2Read();
		bool  eofLiveLog();
		bool  eofCatchUp();
	protected:
		bool  refreshCtrlValOf(std::streampos &ctrlVal);
		bool  setCtrlValOf(std::string ctrlFile, std::streampos &ctrlVal);
		bool  getCtrlValOf(std::string ctrlFile, std::streampos *ctrlVal = nullptr);
	private:
		void  removeAllControlFiles();

		// Control files names.
		static constexpr char m_CONTROL_FILES_PATH[] = "run/ctrl";

		static std::string m_TIMESTAMP_FILES_REGEX;

		static constexpr char m_FILEORSEGMENTEOF_CTRL[] = "/fileOrSegmentEOF.ctrl";
		static constexpr char m_NEXTLOGLINE2READ_CTRL[] = "/nextLogline2Read.ctrl";
		static constexpr char m_NEXTCATCHUP2READ_CTRL[] = "/nextCatchup2Read.ctrl";

		// Reading file control stuff...
		std::ifstream   m_ifsReadLog;
		std::ifstream   m_ifsCatchUp;
		std::string     m_logFilePath;
		std::string     m_ctrlBaseName;
		std::streampos  m_fileOrSegmentEOF;
		std::streampos  m_nextLogline2Read;
		std::streampos  m_nextCatchup2Read;
		bool            m_doneCatchingUp;

		std::set<std::string> m_filestampFileList;

		boost::mutex    m_writeMutexNextLogline;
		std::streampos  m_lastLoglinePosWritten;
		boost::mutex    m_writeMutexNextCatchup;
		std::streampos  m_lastCatchupPosWritten;
};

#endif // READFILECTRL_H
/*END*/
