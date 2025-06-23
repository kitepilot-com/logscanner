/*******************************************************************************
 * File: logScanner/LumpContainer.cpp                                          *
 * Created:     Apr-14-25                                                      *
 * Last update: Apr-14-25                                                      *
 * Top level program file.                                                     *
 *******************************************************************************/

#include <fstream>
#include <iostream>
#include <boost/regex.hpp>

#include "LogManager.h"
#include "LumpContainer.h"

LumpContainer::LumpContainer(std::string logFilePath, std::map<char, std::string> extrctDate, std::map<std::string, char> regexpList) :
	ConfContainer(logFilePath, extrctDate, regexpList)
{
	// This function will load the names of all the logfiles
	// to read for feeding the LUMP file.
	// It will create/truncate the LUMP file.
//	m_allGood = false;
	m_state = ConfContainer::INVALID;

	std::string logname(getLogFilePath().string());

	// Let's get directory and build rexexp.
	std::size_t found = logname.find_last_of("/");
	std::string strPath(logname.substr(0,found));
	std::string strRexp(logname.substr(found + 1));

	// Validate that we have LUMP in the regexp that we are cooking...
	size_t start_pos = strRexp.find(m_LUMP);
	if(start_pos != std::string::npos)
	{
		strRexp.replace(start_pos, m_SIZE, ".*\\");
		strRexp += "$";

		LogManager::getInstance()->consoleMsg();
		LogManager::getInstance()->consoleMsg("\tLooking for files named as '" + strRexp + "' in directory '" + strPath + "'");

		boost::match_results<std::string::const_iterator> result;
		boost::regex                                      regexp(strRexp);
		const std::filesystem::path                       FsPath{strPath};

		try
		{
			int logCount = 0;

			for (auto const& dir_entry : std::filesystem::directory_iterator{FsPath})
			{
				if (std::filesystem::is_regular_file(dir_entry.path()))
				{
					if(boost::regex_search(dir_entry.path().string(), result, regexp))
					{
						// The regexp will pick up the LUMP file,
						// exclude it!
						start_pos = dir_entry.path().filename().string().find(m_LUMP);
						if(start_pos == std::string::npos)
						{
							std::string   file2test(FsPath.string() + "/" + dir_entry.path().filename().string());
							std::ifstream readTest(file2test);

							if(readTest.is_open())
							{
								readTest.close();

								m_logList.insert(file2test);
//								LogManager::getInstance()->consoleMsg("\t" + file2test);
							}
							else
							{
								LogManager::getInstance()->consoleMsg("*** FATAL *** Unable to read file " + file2test);
								m_state = ConfContainer::READLOGS_ERR;
							}

							logCount++;
						}
					}
				}
			}

			if(logCount > 0)
			{
				LogManager::getInstance()->consoleMsg("\t" + std::to_string(logCount) + " files will write to " + logname + "'");
				LogManager::getInstance()->consoleMsg("\tCreating or truncating special file: '" + logname + "'");
//				LogManager::getInstance()->consoleMsg();

				std::ofstream  myfile;
				myfile.open(logname.data(), std::ios_base::out | std::ios_base::trunc);
				if(myfile.is_open())
				{
					myfile.close();
//					m_allGood = true;
					if(m_state == INVALID)
					{
						m_state = ConfContainer::HEALTHY;
					}
				}
				else
				{
					LogManager::getInstance()->consoleMsg("*** FATAL *** => No logs found to feed '" + logname + "'");
					m_state = ConfContainer::FEED_ERR;
				}

			}
			else
			{
				LogManager::getInstance()->consoleMsg("*** FATAL *** => unable to create/truncate '" + logname + "'" + strRexp + "'");
				m_state = ConfContainer::LUMP_ERR;
			}
		}
		catch (std::filesystem::filesystem_error const& ex)
		{
			LogManager::getInstance()->consoleMsg("*** FATAL *** => Unable to read directory '" + strPath + "'");
			m_state = ConfContainer::READDIR_ERR;
		}
	}
	else
	{
		LogManager::getInstance()->consoleMsg("I can't find string '" + std::string(m_LUMP) + "' in <" + strPath + ">");
		m_state = ConfContainer::LUMFLAG_ERR;
	}

	m_logListIter = m_logList.begin();
}

LumpContainer::~LumpContainer()
{
}
/*
bool LumpContainer::allIsGood()
{
	return m_allGood;
}
*/
std::string LumpContainer::getLumpComponent()
{
	std::string   log2go;

	if(m_logListIter == m_logList.end())
	{
		m_logListIter = m_logList.begin();
	}
	else
	{
		log2go = *m_logListIter;
		m_logListIter++;
	}

	return log2go;
}
/**END*/
