/*******************************************************************************
 * File: LogScanner/LogScanner.cpp                                             *
 * Created:     Jan-07-25                                                      *
 * Last update: Jun-30-25                                                      *
 * Top level program file.                                                     *
 *******************************************************************************/

#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <ctime>
#include <vector>
#include <signal.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <iostream>
#include <filesystem>
#include <boost/regex.hpp>
#include <boost/thread.hpp>

#include "LogManager.h"
#include "LogScanner.h"
#include "LumpContainer.h"
#include "INotifyObj.h"
#include "ConveyorBelt.h"

std::atomic<bool>  LogScanner::KEEP_LOOPING(false);

std::string LogScanner::m_BASE_SEMAPHORE_NAME = "logscanner_SEMAPHORE_";
std::string LogScanner::m_QUOTE               = "\"";

LogScanner::LogScanner() :
	m_url(m_DB_URL),
	m_properties({{"user", m_DB_USER}, {"password", m_DB_PSWD}}),
	m_driver(sql::mariadb::get_driver_instance()),
	m_notifyEval(NotifyEvent()),
	m_notifyDrop(NotifyEvent())
{
int JUNK_GET_INIT_OUT_OF_CONSTRUCTORS_0; int JUNK_GET_INIT_OUT_OF_CONSTRUCTORS_1; int JUNK_GET_INIT_OUT_OF_CONSTRUCTORS_2; int JUNK_GET_INIT_OUT_OF_CONSTRUCTORS_3; //FIXTHIS!!!
	LogManager::init();

	LogManager::getInstance()->consoleMsg("*********************************************");
	LogManager::getInstance()->consoleMsg("****   Initiating configuration upload   ****");
	LogManager::getInstance()->consoleMsg("*********************************************");
	LogManager::getInstance()->consoleMsg();
int JUNK_FIX_NAMING_AND_STRUCTURE_OF_DIRECTORIES; //FIXTHIS!!!
//	std::filesystem::create_directories("exec-log");
	std::filesystem::create_directories("run/ctrl");  	//FIXTHIS!!!   Use variables..
	std::filesystem::create_directories("run/logs"); 	//FIXTHIS!!!   Use variables..
	std::filesystem::create_directories("run/rules");	//FIXTHIS!!!   Use variables..
	std::filesystem::create_directories("run/sql");  	//FIXTHIS!!!   Use variables..
	std::filesystem::create_directories("run/sql_err"); //FIXTHIS!!!   Use variables..
	std::filesystem::create_directories("run/tmp");  	//FIXTHIS!!!   Use variables..

	uploadConfiguration();
	uploadEvalRegexps();

	// if m_state == LogScanner::INVALID, then we didn't find any problems.
	if(m_state == LogScanner::INVALID && initDB() == true)
	{
		// Start app's comm server.
		m_fdSocketSrv = 0;
		m_fdAccept    = 0;
		if(initCommServer(m_fdSocketSrv, m_fdAccept, m_SYS_SOCKET, m_SYS_CONNECTS))
		{
			m_state = LogScanner::HEALTHY;
			KEEP_LOOPING.store(true);
		}
	}
}

LogScanner::~LogScanner()
{
}

int LogScanner::exec()
{
	if(m_state == LogScanner::HEALTHY)
	{
		LogManager::getInstance()->consoleMsg("=====> ENTERING LogScanner::exec()");

		LogManager::getInstance()->consoleMsg();
		LogManager::getInstance()->consoleMsg("**********************************");
		LogManager::getInstance()->consoleMsg("****    logscanner STARTED    ****");
		LogManager::getInstance()->consoleMsg("**********************************");
		LogManager::getInstance()->consoleMsg();

		LogManager::getInstance()->consoleMsg("==> LogScanner says: Hello World...");

		// Initialize INotifyObj`
		// We need to create this here before we ever try to use it because
		// this is a monostate class and it has to be initialized.
int JUNK_FIX_NAMING_AND_STRUCTURE_OF_DIRECTORIES; //FIXTHIS!!!
		INotifyObj iNotifyObj("run/ctrl/intfThreadExit.flag");

		std::shared_ptr<boost::thread> comm(std::make_shared<boost::thread>(&LogScanner::commThread, this));
		std::shared_ptr<boost::thread> sqlt(std::make_shared<boost::thread>(&LogScanner::SQL_Thread, this));
		std::shared_ptr<boost::thread> drop(std::make_shared<boost::thread>(&LogScanner::dropThread, this));
		std::shared_ptr<boost::thread> eval(std::make_shared<boost::thread>(&LogScanner::evalThread, this));
		std::shared_ptr<boost::thread> intf(std::make_shared<boost::thread>(&LogScanner::intfThread, this));
		std::shared_ptr<boost::thread> inpt(std::make_shared<boost::thread>(&LogScanner::inptThread, this));

		loadDroppedIPs();

		// The inptThread is the "master" thread.
		// When it exits life is over, that's why we "join" it first
		// and kill everyone else upon exit...
		inpt->join();

		// Wait for inotify.
		intf->join();

		// Release the NotifyEvent that are locked.
		LogManager::getInstance()->consoleMsg("LogScanner::inptThread sending sending notifyNew to everybody...");
		m_notifyEval.notifyNew();
		m_notifyDrop.notifyNew();

		// Release al execSqlInsert threads.
		for(auto notifyEvent : m_NotifyEvent_list)
		{
			notifyEvent.second.get()->notifyNew();
		}

		LogManager::getInstance()->consoleMsg("ALL notifyNew sent!");
		eval->join();
		drop->join();
		sqlt->join();
int JUNK_SEND_A_MSG_TO_SOCKET_INSTEAD_OF_KILL; //FIXTHIS!!!
		LogManager::getInstance()->consoleMsg("Killing LogScanner::commThread...");
		boost::thread::native_handle_type commThreadID = comm->native_handle();
		pthread_kill(commThreadID, SIGUSR1);
		comm->join();

		LogManager::getInstance()->consoleMsg("==> All threads finished...");
		LogManager::getInstance()->consoleMsg("==> Bye bye cruel World");

		LogManager::getInstance()->consoleMsg("logscanner is exiting gracefully...");
	}
	else
	{
		LogManager::getInstance()->consoleMsg();
		LogManager::getInstance()->consoleMsg("********************************************************");
		LogManager::getInstance()->consoleMsg("****    Something went wrong starting up...         ****");
		LogManager::getInstance()->consoleMsg("****    Ensure that no other instance is running    ****");
		LogManager::getInstance()->consoleMsg("****    Execute as 'root'                           ****");
		LogManager::getInstance()->consoleMsg("********************************************************");
		LogManager::getInstance()->consoleMsg("Do a search for '*** FATAL ***' in this file for more info...");
	}

	return m_state;
}

void LogScanner::loadDroppedIPs()
{
	// Read from iptables-save all dropped addresses.
	std::unique_ptr<sql::Connection> conn(m_driver->connect(m_url, m_properties));

	std::string sysCmd("bin/get_DROPped_IPs.sh");
	std::string sysLog("run/logs/get_DROPped_IPs.log");
	std::string sysRes("run/tmp/dropped-IPs.txt");
	std::string sysExe(sysCmd + " > " + sysLog + " 2>&1");

	LogManager::getInstance()->consoleMsg(("EXEC =>\tExecuting " + sysCmd + " in directory " + std::filesystem::current_path().string()).c_str());
	LogManager::getInstance()->consoleMsg(("EXEC =>\tSee log at " + sysLog + " and result in " + sysRes).c_str());
	LogManager::getInstance()->consoleMsg(("EXEC =>\t" + sysExe).c_str());

	std::system(sysExe.data());

	std::ifstream    droppedIPs(sysRes.data());
	std::string      IPaddr;

	if(droppedIPs.is_open())
	{
		LogManager::getInstance()->consoleMsg("Reading currently blocked IPs from iptables...");

		int IPcounter = 0;
		while(std::getline(droppedIPs, IPaddr))
		{
			std::array<std::string, 4> IPaArr;
			storeIPaddrInArray(IPaddr.data(), boost::ref(IPaArr));

			insertIntoDropTable(boost::ref(IPaArr), "LogScanner::initDB()", nullptr, 'D');
			IPcounter++;
		}
		droppedIPs.close();

		LogManager::getInstance()->consoleMsg(("Added " + std::to_string(IPcounter) + " currently blocked IPs from iptables to table...").c_str());
	}

	conn->close();
}

void LogScanner::uploadConfiguration(std::filesystem::path confPath)
{
	// This function loads files from m_CONF_PATH
	// and parses (loadAndParseConfFiles) configuration files
	LogManager::getInstance()->consoleMsg("=====> ENTERING void LogScanner::uploadConfiguration()");
	LogManager::getInstance()->consoleMsg();

	try
	{
		if(exists(confPath) && is_directory(confPath))
		{
			// m_confCount is used to count EXISTING files in this first loop.
			std::set<std::filesystem::path> emptyDirs;
			std::set<std::filesystem::path> sortedTree;
			readDirectoryTree(confPath, sortedTree, emptyDirs);

			if(m_confCount > 0)
			{
				// The next loop validates the integrity of the configuration.
				// m_confCount is used to count CONSISTENT files in this second loop.
				m_confCount = 0;

				loadAndParseConfFiles(sortedTree, emptyDirs);
				LogManager::getInstance()->consoleMsg(("Found " + std::to_string(m_confCount) + " configration files").c_str());

				// std::vector<ConfContainer *>   m_ConfContainerList;
				// This loop validates and outputs to stdout the final configuration.
int JUNK_RESTORE_SUMMARY_MESSAGE; //FIXTHIS!!!   There will be more opcodes...    Make something decent!
//				LogManager::getInstance()->consoleMsg();
//				LogManager::getInstance()->consoleMsg("**********************************************************");
//				LogManager::getInstance()->consoleMsg("vvvv => BEGIN CONFIGURATION VALIDATION AND SUMMARY <= vvvv");
//				LogManager::getInstance()->consoleMsg("**********************************************************");
				for(auto logfileData : m_ConfContainerList)
				{
					LogManager::getInstance()->consoleMsg();
					LogManager::getInstance()->consoleMsg(("==> Begin " + logfileData->getLogFilePath().string()).c_str());

					if(logfileData->getObjStatus() == ConfContainer::HEALTHY)
					{
						std::ifstream    readTest(logfileData->getLogFilePath());

						if(exists(logfileData->getLogFilePath()) && is_regular_file(logfileData->getLogFilePath()) && readTest.is_open())
						{
							LogManager::getInstance()->consoleMsg(("Logfile => " + logfileData->getLogFilePath().string() + " <= is readable.").c_str());
							LogManager::getInstance()->consoleMsg();

							readTest.close();

							LogManager::getInstance()->consoleMsg(("Date REGEXP\t=>" + logfileData->getDateExtract(ConfContainer::REGEXP) + "<=").c_str());
							LogManager::getInstance()->consoleMsg(("Date BLDMAP\t=>" + logfileData->getDateExtract(ConfContainer::BLDMAP) + "<=").c_str());
							LogManager::getInstance()->consoleMsg(("Date FORMAT\t=>" + logfileData->getDateExtract(ConfContainer::FORMAT) + "<=").c_str());
							LogManager::getInstance()->consoleMsg();

							for(int Idx = 0; Idx < logfileData->getRegexpCount(); Idx++)
							{
								LogManager::getInstance()->consoleMsg(("Rxg ->\t" + std::string(1, logfileData->getRegexpSvrt(Idx)) + "\t -\t=>" + logfileData->getRegexpText(Idx) + "<=").c_str());
							}

							std::string  lumpComponent(logfileData->getLumpComponent());
							if(lumpComponent != "")
							{
								LogManager::getInstance()->consoleMsg();
								LogManager::getInstance()->consoleMsg(">>\tBEGIN list of LUMP components:");

								do
								{
									LogManager::getInstance()->consoleMsg(("\t>>\t" + lumpComponent).c_str());
									lumpComponent = logfileData->getLumpComponent();
								}
								while(lumpComponent != "");

								LogManager::getInstance()->consoleMsg(">>\tEND list of LUMP components.");
							}
						}
						else
						{
							LogManager::getInstance()->consoleMsg(("*** FATAL *** Unable to open " + logfileData->getLogFilePath().string() + "for reading.").c_str());
							m_state = LogScanner::OPEN_ERR;
						}
					}
					else
					{
						LogManager::getInstance()->consoleMsg(("*** FATAL *** Unable to init ConfContainer: for " + logfileData->getLogFilePath().string()).c_str());
						m_state = LogScanner::INIT_ERR;
					}
					LogManager::getInstance()->consoleMsg(("==> End " + logfileData->getLogFilePath().string()).c_str());
				}

				uploadWhitelistedIPs();
				if(m_whitelist.empty())
				{
					LogManager::getInstance()->consoleMsg("There are no whitelisted IP addresses");
				}
				else
				{
					LogManager::getInstance()->consoleMsg("Whitelisted Ips:");

					for(auto ipInfo : m_whitelist)
					{
						LogManager::getInstance()->consoleMsg(("=>\t{" + ipInfo.first + "}\t-\t{" + ipInfo.second + "}").c_str());
					}
				}
//				LogManager::getInstance()->consoleMsg("********************************************************");
//				LogManager::getInstance()->consoleMsg("^^^^ => END CONFIGURATION VALIDATION AND SUMMARY <= ^^^^");
//				LogManager::getInstance()->consoleMsg("********************************************************");
			}

			// For the reasons above, this "if" cannot be an "else" for the "if" above.
			// The "if" below means that "we don't consistent or phisycal files".
			if(m_confCount == 0)
			{
				LogManager::getInstance()->consoleMsg("*** FATAL *** No valid configuration files found.");
				m_state = LogScanner::CONFIG_ERR;
			}
		}
		else
		{
			LogManager::getInstance()->consoleMsg(("*** FATAL *** File: '" + confPath.string() + "' does not exist or is not a directory.").c_str());
			m_state = LogScanner::EXIST_ERR;
		}
	}
	catch (const std::filesystem::filesystem_error &ex)
	{
		LogManager::getInstance()->consoleMsg(("*** FATAL *** Something crashed while reading " + confPath.string() + " in uploadConfiguration." + ex.what()).c_str());
		m_state = LogScanner::CRASH_ERR;
		throw;
	}

	LogManager::getInstance()->consoleMsg("=====> FINISHING void LogScanner::uploadConfiguration()");
	LogManager::getInstance()->consoleMsg();
}

void LogScanner::uploadEvalRegexps(std::filesystem::path confPath)
{
	LogManager::getInstance()->consoleMsg("=====> ENTERING void LogScanner::uploadEvalRegexps()");
	LogManager::getInstance()->consoleMsg();

	bool allGood = true;

	try
	{
		LogManager::getInstance()->consoleMsg(("Parsing " + confPath.string()).c_str());

		if(exists(confPath) && is_regular_file(confPath))
		{
			std::ifstream  fileHandler(confPath);
			std::string    line;

			if(fileHandler.is_open())
			{
				// Read the whole file and store good lines in 'confLines'.
				std::set<std::string> dupRegexpChl;
				int lineCtr = 1;
				while(std::getline(fileHandler, line))
				{
					char *fistChar = nullptr;
					char *lastChar = nullptr;
					if(fullTrimmedLine(line, &fistChar, &lastChar) > 0)
					{
						// Is it a comment?
						if(*fistChar != '#')
						{
							// OK, it's not a comment..
							// We need AT LEAST 3 bytes
							if(lastChar - fistChar > 8)
							{
								// Here we enforce that configuration lines
								// start at the beginning of the line.
								std::vector<std::string> strings;
								std::istringstream f(line);
								std::string s;

								// Split the line using the TABs (\t)
								while (std::getline(f, s, '\t'))
								{
									strings.push_back(s);
								}

								if(strings.size() == 3 && !(strings[0].empty() || strings[1].empty() || strings[2].empty()))
								{
									std::string opCode(strings[0]);
									std::string factor(strings[1]);
									std::string regexp(strings[2]);
int JUNK_ADD_OPCODES; //FIXTHIS!!!   There will be more opcodes...    Make something decent!
									if((opCode == "SEEN_MANY_STRINGS" || opCode == "TOO_MANY_ATTEMPTS") && std::all_of(factor.begin(), factor.end(), [](char c) { return std::isdigit(c); }))
									{
										if(dupRegexpChl.find(opCode + regexp) == dupRegexpChl.end())
										{
											dupRegexpChl.insert(opCode + regexp);
											m_evalRegexps.insert({ {opCode, regexp}, atoi(factor.data()) });
										}
										else
										{
											LogManager::getInstance()->consoleMsg(("*** FATAL *** DUPLICATE rexexp in file " + confPath.string() + " line " + std::to_string(lineCtr)).c_str());
											m_state = LogScanner::DUPRGXP_ERR;
											allGood = false;
										}
									}
									else
									{
										LogManager::getInstance()->consoleMsg(("*** FATAL *** Line #" + std::to_string(lineCtr) + " bad content (opCode, factor, regexp) in file " + confPath.string()).c_str());
										m_state = LogScanner::CONTENT_ERR;
										allGood = false;
									}
								}
								else
								{
									LogManager::getInstance()->consoleMsg(("*** FATAL *** Line #" + std::to_string(lineCtr) + " doesn't have 3 tokens in file " + confPath.string()).c_str());
									m_state = LogScanner::TOOSHORT_FEW;
									allGood = false;
								}
							}
							else
							{
								LogManager::getInstance()->consoleMsg(("*** FATAL *** Line #" + std::to_string(lineCtr) + " is too short in file " + confPath.string()).c_str());
								m_state = LogScanner::TOOSHORT_ERR;
								allGood = false;
							}
						}
					}

					lineCtr++;
				}
				fileHandler.close();

				if(allGood)
				{
					if(m_evalRegexps.size() == 0)
					{
						LogManager::getInstance()->consoleMsg(("*** FATAL *** No rexeps found in " + confPath.string()).c_str());
						m_state = LogScanner::NORXGP_ERR;
					}
					else
					{
						LogManager::getInstance()->consoleMsg(("Found " + std::to_string(m_evalRegexps.size()) + " eval regexps:").c_str());

						for(auto evalRegex : m_evalRegexps)
						{
//							m_evalRegexps.insert({ {opCode, regexp}, atoi(factor.data()) });
							std::string opCode(evalRegex.first.first);
							int         factor(evalRegex.second);
							std::string regexp(evalRegex.first.second);

							LogManager::getInstance()->consoleMsg(("\tEVAL =>\t" + opCode + "\t" + std::to_string(factor) + "\t=>" + regexp + "<=").c_str());
						}
					}
				}

				LogManager::getInstance()->consoleMsg(("DONE parsing " + confPath.string()).c_str());
				LogManager::getInstance()->consoleMsg();
			}
			else
			{
				LogManager::getInstance()->consoleMsg(("*** FATAL *** Unable to open " + confPath.string()).c_str());
				m_state = LogScanner::FILE_ERR;
			}
		}
		else
		{
			LogManager::getInstance()->consoleMsg(("*** WARNING *** File: '" + confPath.string() + "' does not exist or is not a regular file.").c_str());
		}
	}
	catch (const std::filesystem::filesystem_error &ex)
	{
		LogManager::getInstance()->consoleMsg(("*** FATAL *** Something crashed while reading " + confPath.string() + " in uploadEvalRegexps" + ex.what()).c_str());
		m_state = LogScanner::CRASH_ERR;
		throw;
	}

	LogManager::getInstance()->consoleMsg("=====> FINISHING void LogScanner::uploadEvalRegexps()");
	LogManager::getInstance()->consoleMsg();
}

int LogScanner::readDirectoryTree(std::filesystem::path confPath, std::set<std::filesystem::path> &sortedTree, std::set<std::filesystem::path> &emptyDirs)
{
	// This function will read the directory m_CONF_PATH to load a sorted tree.
	int fileCount = 0;
	for (const std::filesystem::directory_entry& child : std::filesystem::directory_iterator(confPath))
	{
		if(is_regular_file(child))
		{
//std::cout << "**** FILE  - " << child << std::endl; //FIXTHIS!!!
			sortedTree.insert(child);
			m_confCount++;
			fileCount++;
		}
		else if(is_directory(child))
		{
//std::cout << "**** DIR   - " << child; //FIXTHIS!!!
			sortedTree.insert(child);

			int  childCount = readDirectoryTree(child, sortedTree, emptyDirs);

			if(childCount == 0)
			{
				LogManager::getInstance()->consoleMsg(("*** WARNING *** Empty directory " + child.path().string()).c_str());
				emptyDirs.insert(child);
			}
			fileCount++;
//std::cout <<  std::endl; //FIXTHIS!!!
		}
		else
		{
			LogManager::getInstance()->consoleMsg(("*** FATAL *** " + child.path().string() + " exists, but is not a regular file or directory").c_str());
			m_state = LogScanner::FILETYPE_ERR;
		}
	}

	return fileCount;
}

void LogScanner::loadAndParseConfFiles(std::set<std::filesystem::path> &sortedTree, std::set<std::filesystem::path> &emptyDirs)
{
	/*******************************************************************************
	 * This function traverses the ./config directory reading and validating       *
	 * config files.                                                               *
	 *******************************************************************************/
	try
	{
		for (std::filesystem::path confPath : sortedTree)
		{
			if(is_regular_file(confPath))
			{
				parseConfOf(confPath);
			}
			else
			{
				if(emptyDirs.find(confPath) != emptyDirs.end())
				{
					LogManager::getInstance()->consoleMsg(("\t\t\t*** WARNING *** Empty directory " + confPath.string()).c_str());
					m_state = LogScanner::EMPTY_ERR;
				}
			}
		}
	}
	catch (const std::filesystem::filesystem_error& ex)
	{
		LogManager::getInstance()->consoleMsg((std::string("*** FATAL *** Something crashed while parsing config in loadAndParseConfFiles") + ex.what()).c_str());
		m_state = LogScanner::CRASH_ERR;
		throw;
	}
}

void LogScanner::parseConfOf(std::filesystem::path confPath)
{
	static const int basePathSize = strlen(m_CONF_PATH);
	static const int fileTypeSize = strlen(m_FILE_TYPE);

	bool allGood = true;

	try
	{
		LogManager::getInstance()->consoleMsg(("Parsing " + confPath.string()).c_str());

		std::map<char, std::string>  dateExtrct;
		std::map<std::string, char>  regexpList;
		std::string                  regexp;

		std::ifstream                fileHandler(confPath);
		std::string                  line;
		int                          regexpCount = 0;
		int                          extrctCount = 0;

		int lineCtr = 1;
		if(fileHandler.is_open())
		{
			// Read the whole file and store good lines in 'confLines'.
			while(std::getline(fileHandler, line))
			{
				char *fistChar = nullptr;
				char *lastChar = nullptr;
				if(fullTrimmedLine(line, &fistChar, &lastChar) > 0)
				{
					// Is it a comment?
					if(*fistChar != '#')
					{
						// OK, it's not a comment..
						// We need AT LEAST 3 bytes
						if(lastChar - fistChar > 2)
						{
							// Here we enforce that configuration lines
							// start at the beginning of the line.
							const char *cursor = line.data();
							if((*cursor == 'R' || *cursor == 'M' || *cursor == 'F' || isdigit(*cursor)) && *(cursor + 1) == '\t')
							{
								// Good line, store it.
								if(isdigit(*cursor))
								{
									// Got a regexp...
									std::string regexp(cursor + 2);
									LogManager::getInstance()->consoleMsg(("\t-> Got rexexp => '" + line.substr(0, 1) + "' - " +  regexp+ "<=").c_str());

									if(regexpList.find(regexp) == regexpList.end())
									{
										regexpList.insert({regexp, *cursor});
										regexpCount++;
									}
									else
									{
										LogManager::getInstance()->consoleMsg(("*** FATAL *** DUPLICATE rexexp in file " + confPath.string() + " line " + std::to_string(lineCtr)).c_str());
										m_state = LogScanner::DUPRGXP_ERR;
										allGood = false;
									}
								}
								else
								{
									// Got information to extract date from logfile...
									std::string extrctInfo(cursor + 2);
									LogManager::getInstance()->consoleMsg(("\t-> Got date extract info => '" + line.substr(0, 1) + "' - " + extrctInfo + "<=").c_str());

									if(dateExtrct.find(*cursor) == dateExtrct.end())
									{
										if(line.substr(0, 1) == "M")
										{
											// Clean up all the commas from format...
											extrctInfo.erase(std::remove(extrctInfo.begin(), extrctInfo.end(), ','), extrctInfo.end());
										}

										dateExtrct.insert({*cursor, extrctInfo});
										extrctCount++;
									}
									else
									{
										LogManager::getInstance()->consoleMsg(("*** FATAL *** DUPLICATE date extract info in file " + confPath.string() + " line " + std::to_string(lineCtr)).c_str());
										m_state = LogScanner::DUPDTINFO_ERR;
										allGood = false;
									}
								}
							}
							else
							{
								LogManager::getInstance()->consoleMsg(("*** FATAL *** Line #" + std::to_string(lineCtr) + " unrecognized char 1 or not TAB in char 2 in file " + confPath.string()).c_str());
								m_state = LogScanner::STRUCT_ERR;
								allGood = false;
							}
						}
						else
						{
							LogManager::getInstance()->consoleMsg(("*** FATAL *** Line #" + std::to_string(lineCtr) + " is too short in file " + confPath.string()).c_str());
							m_state = LogScanner::TOOSHORT_ERR;
							allGood = false;
						}
					}
				}

				lineCtr++;
			}
			fileHandler.close();

			if(allGood)
			{
				if(extrctCount == 3)
				{
					if(regexpCount > 0)
					{
						std::shared_ptr<ConfContainer> logfileData(nullptr);

						// Line below converts the name in the "config" directory to the name of the logfile to read.
						std::string logfilePath(confPath.string().substr(basePathSize, confPath.string().length() - basePathSize - fileTypeSize));
						LogManager::getInstance()->consoleMsg(("\t\tStoring " + logfilePath + " and " + std::to_string(regexpList.size()) + " regexp(s).").c_str());

						// We need to discern whether we have or not a LUMP configuration file:
						if(logfilePath.find(LumpContainer::m_LUMP) == std::string::npos)
						{
							// This is a plain vanilla logfile.
							logfileData.reset(new ConfContainer(logfilePath, dateExtrct, regexpList));
						}
						else
						{
							// This is a LUMP logfile.
							logfileData.reset(new LumpContainer(logfilePath, dateExtrct, regexpList));
						}

						if(logfileData->getObjStatus() == ConfContainer::HEALTHY)
						{
							m_ConfContainerList.push_back(logfileData);
#pragma message("//FIXTHIS!!! SHOW m_inotifyTargetPaths AT STARTUP")
							std::filesystem::path  fullFilePath(logfileData->getLogFilePath());
							std::string            patths4inotify(fullFilePath.parent_path());
							if(m_inotifyTargetPaths.find(patths4inotify) == m_inotifyTargetPaths.end())
							{
								m_inotifyTargetPaths.insert(patths4inotify);
							}

							m_confCount++;
						}
						else
						{
							m_state = LogScanner::CONTNER_ERR;
						}
					}
					else
					{
						LogManager::getInstance()->consoleMsg(("*** FATAL *** No rexeps found in " + confPath.string()).c_str());
						m_state = LogScanner::NORXGP_ERR;
					}
				}
				else
				{
					LogManager::getInstance()->consoleMsg(("*** FATAL *** " + std::to_string(3 - extrctCount) + " 'date extract {R(exgexp), M(ap), or F(ormat)}' items missing in " + confPath.string()).c_str());
					m_state = LogScanner::DATEEXT_ERR;
				}
			}

			regexpList.clear();

			LogManager::getInstance()->consoleMsg(("DONE parsing " + confPath.string()).c_str());
			LogManager::getInstance()->consoleMsg();
		}
		else
		{
			LogManager::getInstance()->consoleMsg(("*** FATAL *** Unable to open " + confPath.string()).c_str());
			m_state = LogScanner::FILE_ERR;
		}
	}
	catch (const std::filesystem::filesystem_error& ex)
	{
		LogManager::getInstance()->consoleMsg(("*** FATAL *** Unable to parse " + confPath.string() + " => " + ex.what()).c_str());
		m_state = LogScanner::CRASH_ERR;
	}
}

void LogScanner::uploadWhitelistedIPs()
{
	std::filesystem::path confPath("data/whitelist.txt");

	if(exists(confPath) && is_regular_file(confPath))
	{
		LogManager::getInstance()->consoleMsg();
		LogManager::getInstance()->consoleMsg((">> Reading Whitelist file " + confPath.string()).c_str());

		std::ifstream                fileHandler(confPath);
		std::string                  line;

		if(fileHandler.is_open())
		{
			// Get those whitelisted...
			while(std::getline(fileHandler, line))
			{
				char *fistChar = nullptr;
				char *lastChar = nullptr;
				if(fullTrimmedLine(line, &fistChar, &lastChar) > 0)
				{
					// Is it a comment?
					if(*fistChar != '#')
					{
						// OK, it's not a comment..
						char *leftChar = fistChar;
						char *riteChar = fistChar + 1;

						// Find end of IP address.
						while(riteChar < lastChar && !isspace(*riteChar))
						{
							riteChar++;
						}
						std::string IPaddr(leftChar, riteChar - leftChar);

						// Now find begin of "Comment"
						leftChar = riteChar + 1;
						while(leftChar < lastChar && isspace(*leftChar) )
						{
							leftChar++;
						}

//						// and find end of "Comment"
//						riteChar = leftChar + 1;
//						while(!isspace(*riteChar) && riteChar < lastChar)
//						{
//							riteChar++;
//						}
//						std::string IPcomment(leftChar, riteChar - leftChar);
						std::string IPcomment(leftChar, lastChar - leftChar + 1);

						if(m_whitelist.find(IPaddr) == m_whitelist.end())
						{
							m_whitelist.insert({ IPaddr, IPcomment});
						}
						else
						{
							LogManager::getInstance()->consoleMsg(("*** FATAL *** Duplicated entry " + IPaddr + "in whitelist file " + confPath.string()).c_str());
							m_state = LogScanner::WHITE_ERR;
						}
					}
				}
			}
			fileHandler.close();
		}
		else
		{
			LogManager::getInstance()->consoleMsg(("*** FATAL *** Unable to read whitelist " + confPath.string()).c_str());
			m_state = LogScanner::WHITE_ERR;
		}
	}
	else
	{
		LogManager::getInstance()->consoleMsg(("Whitelist file " + confPath.string() + " not found...").c_str());
		LogManager::getInstance()->consoleMsg();
	}
}

bool LogScanner::initCommServer(int &fdSocketSrv, int &fdAccept, const char* socketPath, int maxConnections)
{
	// The main purpose of this function is to prevent 2 instances of the program running.
	// But it could be used for other things...
	LogManager::getInstance()->consoleMsg("==> ENTERING void LogScanner::initCommServer()");
	LogManager::getInstance()->consoleMsg();

	struct sockaddr_un local;
	int                dataSize = 0;
	bool               allGood  = false;
int JUNK_FIX_initCommServer;//FIXTHIS!!!
	fdSocketSrv = socket(AF_UNIX, SOCK_STREAM, 0);

	if(fdSocketSrv > -1)
	{
		memset(&local, 0x00, sizeof(struct sockaddr_un));
		local.sun_family = AF_UNIX;
		strcpy(&local.sun_path[1], socketPath);
		dataSize = strlen(&local.sun_path[1]) + sizeof(local.sun_family) + 1;

		if(bind(fdSocketSrv, (struct sockaddr*)&local, dataSize) == 0)
		{
			if(listen(fdSocketSrv, maxConnections) == 0 )
			{
				LogManager::getInstance()->consoleMsg(("Socket " + std::string(socketPath) + " is listening.").c_str()); //FIXTHIS!!!  Do I want this here?
				allGood = true;
			}
			else
			{
				LogManager::getInstance()->consoleMsg("*** FATAL *** Error calling 'listen(fdSocketSrv, connections)'");
			}
		}
		else
		{
			LogManager::getInstance()->consoleMsg("*** FATAL *** Error calling 'bind(fdSocketSrv, (struct sockaddr*)&local, dataSize)'");
		}
	}
	else
	{
		LogManager::getInstance()->consoleMsg("*** FATAL *** Error calling 'fdSocketSrv = socket(AF_UNIX, SOCK_STREAM, 0)'");
	}

	LogManager::getInstance()->consoleMsg("==> FINISHING void LogScanner::initCommServer()");
	LogManager::getInstance()->consoleMsg();

	return allGood;
}

bool LogScanner::initDB()
{
	bool allGood = true;

	LogManager::getInstance()->consoleMsg("==> ENTERING void LogScanner::initDB()");
	LogManager::getInstance()->consoleMsg();

	// Test Connection
	std::unique_ptr<sql::Connection> conn(m_driver->connect(m_url, m_properties));

	if(conn->isValid())
	{
		// Read table names to create NotifyEvent objects.
		try
		{
			std::unique_ptr<sql::Statement> stmt(conn->createStatement());
			std::unique_ptr<sql::ResultSet> res (stmt->executeQuery("SHOW TABLES;")); // Or SHOW FULL TABLES;

			LogManager::getInstance()->consoleMsg("List of Notify semaphores:");
			while (res->next())
			{
				std::string tableName(res->getString(1));

				if(tableName.substr(0, 1) == "T")
				{
					std::string IPCnotifyKey(tableName);

					std::filesystem::create_directories(("run/sql/" + tableName).c_str());  	//FIXTHIS!!!   Use variables..

					// I don't like this but...
					NotifyEvent *notyfyAfterInsert;
					if(tableName == "T010_Eval")
					{
						notyfyAfterInsert = &m_notifyEval;
					}
					else if(tableName == "T030_Drop")
					{
						notyfyAfterInsert = &m_notifyDrop;
					}

					// std::map<std::string, std::shared_ptr<NotifyEvent>> m_NotifyEvent_list;
					std::shared_ptr<NotifyEvent> tempPtr(new NotifyEvent(notyfyAfterInsert));
					m_NotifyEvent_list.insert( {IPCnotifyKey, tempPtr} );
				}
			}

			for(auto notifyEvent : m_NotifyEvent_list)
			{
//				std::map<std::string, NotifyEvent *>   m_NotifyEvent_list;
				LogManager::getInstance()->consoleMsg(("=>\t" + notifyEvent.first).c_str());
			}

			LogManager::getInstance()->consoleMsg();
		}
		catch (sql::SQLException &e)
		{
			std::string errmsg('\t' + std::string(e.what()) + "\tshow tables;");
			LogManager::getInstance()->logEvent("initDB_CATCH", errmsg.c_str());
			LogManager::getInstance()->consoleMsg((std::string("*** FATAL *** while loading table names in initDB()\t" + errmsg)).c_str());
			m_state = LogScanner::CRASH_ERR;
//			std::cerr << "SQLException: " << e.what();
//			std::cerr << " (MySQL error code: " << e.getErrorCode();
//			std::cerr << ", SQLState: " << e.getSQLState() << ")" << std::endl;
		}
//		loadNotifyEvent_list();

		conn->close();
	}
	else
	{
		LogManager::getInstance()->consoleMsg("*** FATAL ***  Can't connect to database");
		m_state = LogScanner::CONN_ERR;
		allGood = false;
	}

	LogManager::getInstance()->consoleMsg("==> FINISHING void LogScanner::initDB()");
	LogManager::getInstance()->consoleMsg();

	return allGood;
}

void LogScanner::commThread()
{
	LogManager::getInstance()->consoleMsg(">> STARTING commThread => Hello World");

	struct sockaddr remote;
//FIXTHIS!!!   All this...
	while (KEEP_LOOPING.load())
	{
		unsigned int sockLen = 0;

		memset(&remote, 0x00, sizeof(struct sockaddr));

		if((m_fdAccept = accept(m_fdSocketSrv, (struct sockaddr*)&remote, &sockLen)) > 0)
		{
			LogManager::getInstance()->consoleMsg(("comm server accepting connections in " + std::string(m_SYS_SOCKET)).c_str());

			// We will get an empty string, 2 should be sufficient...
			char recvBuff[2] = { 0x00 };
			int  dataRecv    = recv(m_fdAccept, recvBuff, sizeof(recvBuff), 0);

			if (dataRecv == -1)
			{
				if (errno == EINTR)
				{
					LogManager::getInstance()->consoleMsg("===> LogScanner::commThread: read interrupted by signal, exiting...");
				}
				else
				{
					LogManager::getInstance()->consoleMsg(("===> Read error in LogScanner::commThread => " + std::string(strerror(errno))).c_str());
				}
			}
			else if(dataRecv > 0)
			{
int JUNK_commThread; LogManager::getInstance()->consoleMsg("End of the ride message received..."); //FIXTHIS!!!
KEEP_LOOPING.store(false);       //FIXTHIS!!!
//				if(send(m_fdAccept, line.data(), line.length(), 0) == -1)
//				{
//					LogManager::getInstance()->consoleMsg("**** ERROR ****");
//				}
			}

			close(m_fdAccept);
		}
		else
		{
			LogManager::getInstance()->consoleMsg("** accept error in LogScanner::commThread()");
		}
	}

	LogManager::getInstance()->consoleMsg(">> EXITING commThread => Bye bye cruel World");
}

void LogScanner::intfThread()
{
	INotifyObj iNotifyObj;
	iNotifyObj.intfThread(boost::ref(m_inotifyTargetPaths));
}

void LogScanner::SQL_Thread()
{
	// This thread creates a thread for every SQL table.
	// Each of those threads wait for notifications that text files have been created,
	// then it reads those text files and executes the SQL statements on them.
//	std::map<std::string, NotifyEvent *>::iterator notifyEvent/
	LogManager::getInstance()->consoleMsg(">> STARTING SQL_Thread => Hello World");

	std::vector<std::shared_ptr<boost::thread>> allThreads;

	for(auto notifyEvent : m_NotifyEvent_list)
	{
		// Extract the table name.
		std::string tableName(notifyEvent.first);

		// Start a "execSqlInsert Thread" for every SQL table.
		allThreads.push_back(std::make_shared<boost::thread>(&LogScanner::execSqlInsert, this, tableName, notifyEvent.second.get()));
	}

	for(auto thread : allThreads)
	{
		thread->join();
	}

	LogManager::getInstance()->consoleMsg(">> EXITING SQL_Thread => Bye bye cruel World");
}

void LogScanner::inptThread()
{
	// a.- The Input Thread:
	// ---------------------
	// This main thread starts as many "reading threads" as there are configuration files.
	LogManager::getInstance()->consoleMsg(">> STARTING inptThread => Hello World");

	std::vector<std::shared_ptr<boost::thread>> allThreads;

	for(auto logfileData : m_ConfContainerList)
	{
		// Start a "Reading Thread" for every logfile.
		std::shared_ptr<boost::thread> thread(std::make_shared<boost::thread>(&LogScanner::logReadingThread, this, logfileData.get()));
		allThreads.push_back(thread);
	}

	for(auto thread : allThreads)
	{
		thread->join();
	}

	LogManager::getInstance()->consoleMsg(">> EXITING inptThread => Bye bye cruel World");
}

void LogScanner::evalThread()
{
	LogManager::getInstance()->consoleMsg(">> STARTING evalThread => Hello World");
// SELECT DISTINCT MIN(LogTmstmp) AS frstSeen, MAX(LogTmstmp) AS lastSeen, CONCAT(Octect_1, '.', Octect_2, '.', Octect_3, '.', Octect_4) AS IPaddr, COUNT(*) AS hitCount FROM T010_Eval GROUP BY IPaddr ORDER BY hitCount DESC;
// INSERT INTO T040_Hist VALUES (Octect_1=Arr[1], ..., Timestmp=EvalTimestamp), (Octect_1=Arr[1], ..., Timestmp=EvalTimestamp), (Octect_1=Arr[1], ..., Timestmp=EvalTimestamp);
	while(KEEP_LOOPING.load())
	{
		m_notifyEval.wait4NotifyNew();

		while(KEEP_LOOPING.load() && m_notifyEval.recordsPending())
		{
			// We have rows to inspect...
			std::unique_ptr<sql::Connection> conn(connectToDB());

			std::vector<std::array<std::string, 4> > allDroppedIPs;
			std::vector<std::array<std::string, 4> > allDelEvalIPs;
			std::vector<int>                         allFrstTmStmp;
			std::vector<int>                         allLastTmStmp;

// SELECT DISTINCT MIN(LogTmstmp) AS frstSeen, MAX(LogTmstmp) AS lastSeen, MIN(Severity_val) AS Severity_val, COUNT(*) AS hitCount, CONCAT(Octect_1, '.', Octect_2, '.', Octect_3, '.', Octect_4) AS IPaddr FROM T010_Eval GROUP BY IPaddr ORDER BY hitCount DESC;
			std::string sql2go("SELECT DISTINCT MIN(LogTmstmp)                                                AS frstSeen, "
											   "MAX(LogTmstmp)                                                AS lastSeen, "
											   "MIN(Severity_val)                                             AS Severity_val, "
											   "COUNT(*)                                                      AS hitCount, "
											   "CONCAT(Octect_1, '.', Octect_2, '.', Octect_3, '.', Octect_4) AS IPaddr "
											   "FROM T010_Eval "
											   "GROUP BY IPaddr "
											   "ORDER BY hitCount DESC;");

			try
			{
				std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(sql2go.data()));
				std::unique_ptr<sql::ResultSet>         resulSelect(stmnt->executeQuery());

				if(resulSelect != nullptr && resulSelect->rowsCount() > 0)
				{
					// Force the loop entry...
					int hitCount = m_MAX_HITCOUNT_TO_CHK + 1;

					while(resulSelect->next() && hitCount > m_MAX_HITCOUNT_TO_CHK)
					{
						hitCount = atoi(resulSelect->getString("hitCount"));

						if(hitCount > m_MAX_HITCOUNT_TO_CHK)
						{
							int frstSeen = atoi(resulSelect->getString("frstSeen"));
							int lastSeen = atoi(resulSelect->getString("lastSeen"));

							float Severity_val = atoi(resulSelect->getString("Severity_val"));
							float interval     = lastSeen - frstSeen;

							std::array<std::string, 4> IPaArr;
							storeIPaddrInArray(resulSelect->getString("IPaddr").c_str(), boost::ref(IPaArr));
//							std::stringstream ss(resulSelect->getString("IPaddr").c_str());

//							for(int Idx = 0; Idx < 4; Idx++)
//							{
//								getline(ss, IPaddr[Idx], '.');
//							}
/*
							std::string sql2go("SELECT DISTINCT MIN(Severity_val) AS Severity_val "
											   "FROM T010_Eval "
											   "WHERE Octect_1=" + IPaddr[0] + " "
											   "AND   Octect_2=" + IPaddr[1] + " "
											   "AND   Octect_3=" + IPaddr[2] + " "
											   "AND   Octect_4=" + IPaddr[3] + ";");

							std::unique_ptr<sql::PreparedStatement> sevStmnt(conn->prepareStatement(sql2go.data()));
							std::unique_ptr<sql::ResultSet>         resultSeverity(sevStmnt->executeQuery());

							resultSeverity->next();
							unsigned int Severity_val = atoi(resultSeverity->getString("Severity_val"));*/

//if(lastSeen > frstSeen) LogManager::getInstance()->logEvent("DBG_lastSeen_GT_frstSeen", "B4 if => " + std::string(resulSelect->getString("IPaddr").c_str()) + " => ((hitCount " + std::string(resulSelect->getString("hitCount").c_str()) + " / interval " + std::to_string((int)interval) + ") > Severity_val " + std::string(resulSelect->getString("Severity_val").c_str()) + " || hitCount " + std::string(resulSelect->getString("hitCount").c_str()) /*+ " > " + std::to_string(m_MAX_SECONDS_TO_DROP)*/ + ")");  //FIXTHIS!!!
							// HERE is where we decide to drop....
							if(lastSeen > frstSeen && ((hitCount / interval) > Severity_val || chkEvalRegexps(boost::ref(IPaArr))/* || hitCount > m_MAX_SECONDS_TO_DROP*/))
							{
								// This IP will be inserted in DROP and the lines in Eval will be moved to Hist;
								allDroppedIPs.push_back(IPaArr);
//LogManager::getInstance()->logEvent("DBG_lastSeen_GT_frstSeen", "IN if => " + std::string(resulSelect->getString("IPaArr").c_str()) + " => ((hitCount " + std::string(resulSelect->getString("hitCount").c_str()) + " / interval " + std::to_string((int)interval) + ") > Severity_val " + std::string(resulSelect->getString("Severity_val").c_str()) + " || hitCount " + std::string(resulSelect->getString("hitCount").c_str()) +/* " > " + std::to_string(m_MAX_SECONDS_TO_DROP) +*/ "  ****  IP_DROPPED  ****");  //FIXTHIS!!!
							}
							else
							{
								// Any record of this IP (lastSeen - m_MAX_SECONDS_TO_KEEP seconds) will be deleted from Eval.
								allDelEvalIPs.push_back(IPaArr);
								allFrstTmStmp.push_back(frstSeen);
								allLastTmStmp.push_back(lastSeen);
							}
						}
					}
				}
			}
			catch(sql::SQLException &e)
			{
				std::string errmsg('\t' + std::string(e.what()) + '\t' + sql2go);
				LogManager::getInstance()->logEvent("evalThread_1_CATCH", errmsg.c_str());
//				LogManager::getInstance()->consoleMsg(("evalThread_1_CATCH *DEAD*\t" + errmsg).c_str());
			}

			conn->close();

			if(allDroppedIPs.size() > 0)
			{
				for(auto dropIP : allDroppedIPs)
				{
					// This IP will be inserted in DROP
					// and the lines in Eval will be moved to Hist;
					// dropIpTmstmp is set to the same in T040_Hist and T030_Drop for treaceability.
					std::string    dropIpTmstmp(createTimestamp());

					insertIntoDropTable(boost::ref(dropIP), "LogScanner::evalThread()", &dropIpTmstmp);

					std::string moveEvalRows2Hist("UPDATE T010_Eval "
												  "SET Timestamp=" + dropIpTmstmp + " " +
												  "WHERE Octect_1=" + dropIP[0] +
												  "  AND Octect_2=" + dropIP[1] +
												  "  AND Octect_3=" + dropIP[2] +
												  "  AND Octect_4=" + dropIP[3] + ";\n" +
												  "INSERT INTO T040_Hist " +
												  "    SELECT * FROM T010_Eval "
												  "    WHERE Octect_1=" + dropIP[0] +
												  "      AND Octect_2=" + dropIP[1] +
												  "      AND Octect_3=" + dropIP[2] +
												  "      AND Octect_4=" + dropIP[3] + ";\n"
												  "DELETE FROM T010_Eval " +
												  "WHERE Octect_1=" + dropIP[0] +
												  "  AND Octect_2=" + dropIP[1] +
												  "  AND Octect_3=" + dropIP[2] +
												  "  AND Octect_4=" + dropIP[3] + ";\n");
//LogManager::getInstance()->consoleMsg(("DBG ====> execBatchSQL\t" + moveEvalRows2Hist).c_str());
					execBatchSQL(moveEvalRows2Hist);
				}
//				m_notifyDrop->notifyNew();
			}

			if(allDelEvalIPs.size() > 0)
			{
				std::string delEvalRows;

				for(std::size_t Idx = 0; Idx < allDelEvalIPs.size(); Idx++)
				{
					// Delete record of this IP (lastSeen - m_MAX_SECONDS_TO_KEEP seconds).
					if(allLastTmStmp[Idx] - m_MAX_SECONDS_TO_KEEP > allFrstTmStmp[Idx])
					{
						delEvalRows += ("DELETE FROM T010_Eval "
										"WHERE LogTmstmp < " + std::to_string(allLastTmStmp[Idx] - m_MAX_SECONDS_TO_KEEP) +
										" AND Octect_1="     + allDelEvalIPs[Idx][0] +
										" AND Octect_2="     + allDelEvalIPs[Idx][1] +
										" AND Octect_3="     + allDelEvalIPs[Idx][2] +
										" AND Octect_4="     + allDelEvalIPs[Idx][3] + ";\n");
					}
				}

				if(delEvalRows.size() > 0)
				{
					execBatchSQL(delEvalRows);
				}
			}

			allDroppedIPs.clear();
			allDelEvalIPs.clear();
			allFrstTmStmp.clear();
			allLastTmStmp.clear();
		}
	}

	LogManager::getInstance()->consoleMsg(">> EXITING evalThread => Bye bye cruel World");
}

void LogScanner::dropThread()
{
	LogManager::getInstance()->consoleMsg(">> STARTING dropThread => Hello World");

	while(KEEP_LOOPING.load())
	{
		m_notifyDrop.wait4NotifyNew();

		while(KEEP_LOOPING.load() && m_notifyDrop.recordsPending())
		{
			std::unique_ptr<sql::Connection> conn(connectToDB());

			// First we mark the rows that we are going to work with;
			std::string sql2go("UPDATE T030_Drop SET Status = 'P' WHERE Status = 'N';");

			try
			{
				std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(sql2go.data()));
				int rowCount = stmnt->executeUpdate();

				if(rowCount > 0)
				{
					// Select set of new addresses to write iptables rules.
					sql2go = "SELECT DISTINCT a.Octect_1, a.Octect_2, a.Octect_3, a.Octect_4 "
							 "FROM T030_Drop a "
							 "	LEFT JOIN V030_Drop_010 b ON (a.Octect_1 = b.Octect_1 AND a.Octect_2 = b.Octect_2 AND a.Octect_3 = b.Octect_3 AND a.Octect_4 = b.Octect_4) "
							 "WHERE a.Status = 'P' AND b.Octect_1 IS NULL "
							 "ORDER BY a.Octect_1, a.Octect_2, a.Octect_3, a.Octect_4;";


					std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(sql2go.data()));
					std::unique_ptr<sql::ResultSet>         resulSelect(stmnt->executeQuery());

					int newRows = 0;
					if(resulSelect != nullptr && resulSelect->rowsCount() > 0)
					{
						// Write iptables rules!
						std::ofstream  rules;
						std::string    rulesFile("run/rules/" + createTimestamp() + ".txt");

						rules.open(rulesFile.data(), std::ios_base::trunc);
						while(resulSelect->next())
						{
							std::string rule("-A INPUT -s ");
							rule += (resulSelect->getString("Octect_1") + "." +
									 resulSelect->getString("Octect_2") + "." +
									 resulSelect->getString("Octect_3") + "." +
									 resulSelect->getString("Octect_4"));
							rule += " -j DROP";
							rules << rule << std::endl;

							newRows++;
						}
						rules.close();

						// The file below marks the last consistent rule's file written.
						// A shell script will do the rest...
						rulesFile += ".flag";
						rules.open(rulesFile.data(), std::ios_base::trunc);
						rules << createTimestamp() << std::endl;
						rules.close();

						// Load new rules:
						if(newRows > 0)
						{
							std::system("bin/install-new-rules.sh >> run/logs/install-new-rules.log 2>&1");
						}

						// Mark rows as 'D' (Done).
						sql2go = "UPDATE T030_Drop SET Status = 'D' WHERE Status = 'P';";

						std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(sql2go.data()));
						stmnt->executeUpdate();
					}
				}
			}
			catch(sql::SQLException &e)//FIXTHIS!!!  The whole try thing...
			{
				std::string errmsg('\t' + std::string(e.what()) + '\t' + sql2go);
				LogManager::getInstance()->logEvent("dropThread_CATCH", errmsg.c_str());
//				LogManager::getInstance()->consoleMsg(("dropThread_CATCH *DEAD*\t" + errmsg).c_str());
			}

			conn->close();
		}
	}

	LogManager::getInstance()->consoleMsg(">> EXITING dropThread => Bye bye cruel World");
}

void LogScanner::execSqlInsert(std::string tableName, NotifyEvent *notifyEvent)
{
	// This thread reads the files created by LogScanner::insertInto.*Table
	// and excutes the SQL statements.
	const std::filesystem::path FsPath{(m_SQL_IPC_PATH + std::string("/") + tableName).c_str()};

	std::string strRexp("[[:digit:]]{1,}\\.txt");

	boost::match_results<std::string::const_iterator> result;
	boost::regex                                      regexp(strRexp);

	LogManager::getInstance()->consoleMsg(("=> STARTING execSqlInsert for files in " + FsPath.string()).c_str());

	while(KEEP_LOOPING.load())
	{
		// This wait4NotifyNew is set from LogScanner::insertInto.*Table
		notifyEvent->wait4NotifyNew();

		if(KEEP_LOOPING.load())
		{
			std::unique_ptr<sql::Connection> conn(connectToDB());

			while(KEEP_LOOPING.load() && notifyEvent->recordsPending())
			{
				// Read the /run/sql/{tableName} directory.
//				for (auto const& dir_entry : std::filesystem::directory_iterator{FsPath})
				for(std::filesystem::directory_iterator dir_entry{FsPath}, end; dir_entry != end && KEEP_LOOPING.load(); ++dir_entry)
				{
					if(std::filesystem::is_regular_file(dir_entry->path()))
					{
						// Only use the files that match the regex.
						if(boost::regex_search(dir_entry->path().string(), result, regexp))
						{
							std::string   sqlFile(FsPath.string() + "/" + dir_entry->path().filename().string());
							std::ifstream SQLtexFile(sqlFile);

							if(SQLtexFile.is_open())
							{
								std::string sql2go;
								std::string argc;
								std::string value;

								try
								{
									// Get SQL statement.
									std::getline(SQLtexFile, sql2go);
									std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(sql2go.data()));

									// Get number of arguments.
									std::getline(SQLtexFile, argc);

									// Get value of arguments.
									int Idx = 0;
									while(Idx < atoi(argc.c_str()))
									{
										std::getline(SQLtexFile, value);
										Idx++;

										switch(*(value.c_str()))
										{
											case 'S':
												if(value.length() > 1)
												{
													stmnt->setString(Idx, (value.c_str() + 1));
												}
												else
												{
													stmnt->setNull(Idx, sql::DataType::VARCHAR);
												}
												break;
										}
									}

									stmnt->execute();

									remove(sqlFile.c_str());

									if(notifyEvent->forwardNotify() != nullptr)
									{
										notifyEvent->forwardNotify()->notifyNew();
									}
								}
								catch(sql::SQLException &e)
								{
									rename(sqlFile.c_str(), ("run/sql_err/" + dir_entry->path().filename().string()).c_str()); 	//FIXTHIS!!!   Use variables..
									std::string errMsg("\tWhat:" + std::string(e.what()) + "\tsql2go:" + sql2go);
									LogManager::getInstance()->logEvent(("execSqlInsert_" + tableName + "_CATCH").c_str(), errMsg.c_str());
								}

								SQLtexFile.close();
							}
							else
							{
								LogManager::getInstance()->consoleMsg(("*** ERROR *** Unable to read file " + sqlFile).c_str());
							}
						}
					}
				}
			}

			conn->close();
		}
	}

	LogManager::getInstance()->consoleMsg(("=> EXITING execSqlInsert for files in " + FsPath.string()).c_str());
}

void LogScanner::logReadingThread(ConfContainer *logfileData)
{
	// This thread requests a line from the container and hands it to any waiting processingThread.
	LogManager::getInstance()->consoleMsg((">>> STARTING logReadingThread for " + logfileData->getLogFilePath().string()).c_str());

	while(KEEP_LOOPING.load())
	{
		while(KEEP_LOOPING.load() && !logfileData->RESTART_THREAD.load())
		{
			// Open the log file for reading and set the input stream pointer for reading after EOF.
			// initReadLogThread ma sit foe a whilew, so check KEEP_LOOPING and RESTART_THREAD too...
			if(logfileData->initReadLogThread())
			{
				LogManager::getInstance()->consoleMsg(("File (logReadingThread) good\t=>\t" + logfileData->getLogFilePath().string()).c_str());

				// The ConveyorBelt moves lines from this thread to LogScanner::processingThread
				std::vector<std::shared_ptr<boost::thread>> allThreads;
				ConveyorBelt                                conveyorBelt(m_BASE_SEMAPHORE_NAME + "logReadingThread" + logfileData->getThreadName(), m_totThreads);

				// Monitor the file for rename (logrotate will rename the file...).
				allThreads.push_back(std::make_shared<boost::thread>(&LogScanner::monitor4RenameThread, this, logfileData));

				// Backlog?
				if(logfileData->fileHasBackLog())
				{
					std::shared_ptr<boost::thread> thread(std::make_shared<boost::thread>(boost::thread(&LogScanner::catchUpLogThread, this, logfileData)));
					allThreads.push_back(thread);
				}

				// Start m_totThreads "processing threads".
				// We store the pointers in a vector to "join" them on exit.
				for(int procThreadID = 0; procThreadID < m_totThreads; procThreadID++)
				{
					std::shared_ptr<boost::thread> thread(std::make_shared<boost::thread>(&LogScanner::processingThread, this, logfileData, "logReadingThread", &conveyorBelt, procThreadID, &ConfContainer::writePosOfNextLogline2Read));
					allThreads.push_back(thread);
				}

				// Loop to read the log file.
				// The "processing thread" will pick up the lines.
				std::string line;
				while(KEEP_LOOPING.load() && !logfileData->RESTART_THREAD.load())
				{
					// Read incoming line...
					while(KEEP_LOOPING.load() && !logfileData->RESTART_THREAD.load() && logfileData->getNextLogLine(line))
					{
						// We only care for lines that have IP address.
						if(hasIPaddress(line))
						{
LogManager::getInstance()->logEvent(("DBG_TIME_1_logReadingThread_GOT_from" + logfileData->getThreadName()).c_str(), line.c_str());  //FIXTHIS!!!
							// Send a line to conveyorBelt.acceptFromReadThread(line);
							conveyorBelt.deliverToProcThreads(line);
						}
						else
						{
							LogManager::getInstance()->logEvent(("DBG_logReadingThread" + logfileData->getThreadName() + "_NO_IP").c_str(), line.c_str());  //FIXTHIS!!!
						}
					}
int JUNK_HOW_DO_I_SIGNAL_inotifyLogfile; //FIXTHIS!!!
					if(KEEP_LOOPING.load() && !logfileData->RESTART_THREAD.load() && logfileData->eofLiveLog())
					{
LogManager::getInstance()->logEvent(("DBG_TIME_EOF_logReadingThread_GOT_from" + logfileData->getThreadName()).c_str(), line.c_str());  //FIXTHIS!!!
						// Now wait until a new line is added to the file, but...
						// There is a race condition here.
						// A line can be added to the file before the inotify watch gets "installed".
						// AFAIK that condition is impossible to manage unless we can control the process that adds the lines.
						// And we can't control the process that adds the lines...
						// Ah well...
						// The line will sit there until another line is added...
//						logfileData->inotifyLogfile(logfileData->getLogFilePath());
						monitorNewLineThread(logfileData);
					}
				}
LogManager::getInstance()->consoleMsg(("DBG_OUT OF LOOP logReadingThread" + logfileData->getThreadName() + " <=>\tKEEP_LOOPING = " + (KEEP_LOOPING.load() ? "T" : "F") + "\t<=> RESTART_THREAD = " + (logfileData->RESTART_THREAD.load() ? "T" : "F") + "\t" + line).c_str());  //FIXTHIS!!!
				logfileData->stopReadLogThread();
				conveyorBelt.shutDown();

				for(auto thread : allThreads)
				{
					thread->join();
				}
			}
		}

		if(KEEP_LOOPING.load() && logfileData->RESTART_THREAD.load())
		{
			LogManager::getInstance()->consoleMsg((">>> RESTARTING logReadingThread for " + logfileData->getLogFilePath().string()).c_str());
			logfileData->RESTART_THREAD.store(false);
		}
	}

	LogManager::getInstance()->consoleMsg((">>> EXITING logReadingThread for " + logfileData->getLogFilePath().string()).c_str());
}

void LogScanner::catchUpLogThread(ConfContainer *logfileData)
{
	LogManager::getInstance()->consoleMsg((">>> STARTING catchUpLogThread for " + logfileData->getLogFilePath().string()).c_str());

	// The ConveyorBelt moves lines from this thread to LogScanner::processingThread
	ConveyorBelt conveyorBelt(m_BASE_SEMAPHORE_NAME + "catchUpLogThread" + logfileData->getThreadName(), m_totThreads * m_MULTIPLIER);

	std::vector<std::shared_ptr<boost::thread>> allThreads;

	// This thread will die after we have read all old lines, so we start (m_totThreads * m_MULTIPLIER) "processing threads" to speed up the process.
	for(int procThreadID = 0; procThreadID < (m_totThreads * m_MULTIPLIER); procThreadID++)
	{
		std::shared_ptr<boost::thread> thread(std::make_shared<boost::thread>(&LogScanner::processingThread, this, logfileData, "catchUpLogThread", &conveyorBelt, procThreadID, &ConfContainer::writePosOfNextCatchup2Read));
		allThreads.push_back(thread);
	}

	if(logfileData->initCatchUpThread())
	{
		// Loop to read the log file.
		// Some "processing thread" will pick it up.
//LogManager::getInstance()->consoleMsg(("DBG_GOING TO LOOP " + logfileData->getThreadName()).c_str());  //FIXTHIS!!!
		std::string line;
		while(KEEP_LOOPING.load() && !logfileData->RESTART_THREAD.load() && logfileData->getCatchupLine(line))
		{
			// We only care for lines that have IP address.
			if(hasIPaddress(line))
			{
LogManager::getInstance()->logEvent(("DBG_TIME_1_catchUpLogThread_GOT_from" + logfileData->getThreadName()).c_str(), line.c_str());  //FIXTHIS!!!
				// Send a line to conveyorBelt.acceptFromReadThread(line);
				conveyorBelt.deliverToProcThreads(line);
//LogManager::getInstance()->consoleMsg(("DBG_RETURNED_from_deliverToProcThreads_in_catchUpLogThread_GOT_from => " + logfileData->getThreadName() + " <=>\t" + line).c_str());  //FIXTHIS!!!
			}
			else
			{
				LogManager::getInstance()->logEvent(("DBG_catchUpLogThread" + logfileData->getThreadName() + "_NO_IP").c_str(), line.c_str());  //FIXTHIS!!!
			}
		}
//LogManager::getInstance()->consoleMsg(("DBG_OUT OF LOOP catchUpLogThread" + logfileData->getThreadName() + " <=>\tKEEP_LOOPING = " + (KEEP_LOOPING.load() ? "T" : "F") + "\t<=> RESTART_THREAD = " + (logfileData->RESTART_THREAD.load() ? "T" : "F") + "\t" + line).c_str());  //FIXTHIS!!!
		logfileData->stopCatchUpThread();
		conveyorBelt.shutDown();
	}
//LogManager::getInstance()->consoleMsg(("DBG_catchUpLogThread_DONE_for" + logfileData->getThreadName() + "\tNOT EXITING YET!!!").c_str());  //FIXTHIS!!!
	for(auto thisThread : allThreads)
	{
		thisThread->join();
	}

	LogManager::getInstance()->consoleMsg((">>> EXITING catchUpLogThread for " + logfileData->getLogFilePath().string()).c_str());
}

void LogScanner::processingThread(ConfContainer *logfileData, const char *caller, ConveyorBelt *conveyorBelt, int procThreadID, void (ConfContainer::*ptr_writePosOfNextline2Read)())
{
	// The "processing thread" hands the line to all "regexp eval threads" and waits for the result of the regexp evaluation.
	std::vector<std::shared_ptr<boost::thread>> allThreads;
	boost::barrier                               barrier(logfileData->getRegexpCount() + 1);
	std::string                                  line;
	std::string                                 *linePtr = &line;
	Workarea                                     threadWkArea(logfileData, procThreadID);

	LogManager::getInstance()->consoleMsg((">>>> STARTING processingThread (" + std::string(caller) + ") " + threadWkArea.getProcThreadID() + " for " + logfileData->getLogFilePath().string()).c_str());

	// Each "processing thread" creates an "evaluation thread" for every regexp to be evaluated.
	for(int rgxpThreadID = 0; rgxpThreadID < logfileData->getRegexpCount(); rgxpThreadID++)
	{
		// Each regexpExecThread thread stops at the "barrier" after start up.
		// *THIS* thread stops on "acceptFromReadThread" until a line is accepted.
		// ALl threads see the accepted line simultaneously, so when *THIS* thread reaches the barrier,
		// the barrier count gets full and all the threads continue executing.
		// Same barrier is placed again at the bottom to asure that all threads are finished before wrapping up.
		std::shared_ptr<boost::thread> thread(std::make_shared<boost::thread>(boost::thread(&LogScanner::regexpExecThread, this, logfileData, caller, &threadWkArea, linePtr, boost::ref(barrier), rgxpThreadID)));
		allThreads.push_back(thread);
	}

	// Loop to assign line to all regexps.
	// This loop runs from the logReadingThread and the catchUpLogThread.

	while(line != "EXIT")
	{
		// Get a line from conveyorBelt->deliverToProcThreads(line);
		// This line blocks the thread until a line is received.
		// By the meantime all LogScanner::regexpExecThread are blocked in the "barrier".
		// When a line is received the *THIS* theread advances to the barrier and releases the lock for everyone.
//LogManager::getInstance()->consoleMsg(("DBG_GOING TO RECEIVE line FROM " + logfileData->getThreadName()).c_str());  //FIXTHIS!!!
		conveyorBelt->acceptFromReadThread(line);
LogManager::getInstance()->logEvent(("DBG_TIME_2_acceptFromReadThread_GOT_from" + logfileData->getThreadName() + "_" + threadWkArea.getProcThreadID()).c_str(), line.c_str());  //FIXTHIS!!!
//LogManager::getInstance()->consoleMsg(("DBG_RECEIVED line FROM " + logfileData->getThreadName() + " <=>	" + line).c_str());  //FIXTHIS!!!
		threadWkArea.clearMatchCount();

		// This barrier force all LogScanner::regexpExecThread threads to wait for conveyorBelt->acceptFromReadThread(line);
		// to receive a line.
//LogManager::getInstance()->consoleMsg(("DBG_B4_barrier.wait(1)_IN_processingThread " + threadWkArea.getThreadName() + "\t(" + std::string(caller) + ")\t<=>	" + line).c_str());  //FIXTHIS!!!
		barrier.wait();
//LogManager::getInstance()->consoleMsg(("DBG_AF_barrier.wait(1)_IN_processingThread " + threadWkArea.getThreadName() + "\t(" + std::string(caller) + ")\t<=>	" + line).c_str());  //FIXTHIS!!!
		if(line != "EXIT")
		{
//LogManager::getInstance()->consoleMsg(("DBG_PROCESING line FROM " + logfileData->getThreadName() + " <=>	" + line).c_str());  //FIXTHIS!!!
//LogManager::getInstance()->logEvent(("DBG_TIME_3_acceptFromReadThread_GOT_from" + logfileData->getThreadName() + "_" + threadWkArea.getProcThreadID()).c_str(), line.c_str());  //FIXTHIS!!!
			// This barrier waits for all the LogScanner::processingThread threads to be done.
//LogManager::getInstance()->consoleMsg(("DBG_B4_barrier.wait(2)_IN_processingThread " + threadWkArea.getThreadName() + "\t(" + std::string(caller) + ")\t<=>	" + line).c_str());  //FIXTHIS!!!
			barrier.wait();
//LogManager::getInstance()->consoleMsg(("DBG_AF_barrier.wait(2)_IN_processingThread " + threadWkArea.getThreadName() + "\t(" + std::string(caller) + ")\t<=>	" + line).c_str());  //FIXTHIS!!!
//LogManager::getInstance()->logEvent(("DBG_TIME_4_acceptFromReadThread_GOT_from" + logfileData->getThreadName() + "_" + threadWkArea.getProcThreadID()).c_str(), line.c_str());  //FIXTHIS!!!
int JUNK_FIX_THIS_COMMENT; //FIXTHIS!!!    Fix this comment...
			// Lines caught by several regexp are inserted in Audit table.
			// Lines not caught at all are inserted in text file Audit-line.
			// Lines caught by regexps with "Severity_val = 0" are inserted in Hist and in Drop
			// Lines caught by regexps with "Severity_val > 1" are inserted in Eval.
//LogManager::getInstance()->logEvent(("DBG_TIME_5_acceptFromReadThread_GOT_from" + logfileData->getThreadName() + "_" + threadWkArea.getProcThreadID()).c_str(), line.c_str());  //FIXTHIS!!!
			// T100_LinesSeen tells me if the line was matched or not.
			// T020_Audit     tells me f the line was multi-matched or whitelisted.
			insertIntoSeenTable(logfileData, line, threadWkArea.getMatchCount() > 0);

			// Either  writePosOfNextLogline2Read() or writePosOfNextCatchup2Read()
			(logfileData->*ptr_writePosOfNextline2Read)();
//LogManager::getInstance()->logEvent(("DBG_TIME_6_acceptFromReadThread_GOT_from" + logfileData->getThreadName() + "_" + threadWkArea.getProcThreadID()).c_str(), line.c_str());  //FIXTHIS!!!
		}
	}
//LogManager::getInstance()->consoleMsg(("DBG_OUT OF THE LOOP processingThread " + logfileData->getThreadName() + " <=>	" + line).c_str());  //FIXTHIS!!!
	conveyorBelt->notifyExit();
	for(auto thread : allThreads)
	{
		thread->join();
	}

	LogManager::getInstance()->consoleMsg((">>>> EXITING processingThread (" + std::string(caller) + ") " + threadWkArea.getProcThreadID() + " for " + logfileData->getLogFilePath().string()).c_str());
}

void LogScanner::regexpExecThread(ConfContainer *logfileData, const char *caller, Workarea *threadWkArea, std::string *linePtr, boost::barrier &barrier, int rgxpThreadID)
{
	std::string rgxpThreadIDstr("0" + std::to_string(rgxpThreadID));
	rgxpThreadIDstr = rgxpThreadIDstr.substr(rgxpThreadIDstr.length() - 2, 2);

	LogManager::getInstance()->consoleMsg((">>>>> STARTING regexpExecThread (" + std::string(caller) + ") " + threadWkArea->getThreadName() + "_" + threadWkArea->getProcThreadID() + "_" + rgxpThreadIDstr).c_str());

	boost::regex expr(threadWkArea->getRegexpText(rgxpThreadID));

	// Each thread has a "result" slot in a bool array in the Workarea object,
	// which is use to determine how many matches (or if any) this line had.
	// We don't want multiple matches.
	bool *regexpPtrResult = threadWkArea->getRegexpPtrResult(rgxpThreadID);

	while(*linePtr != "EXIT")
	{
		// see line: // conveyorBelt->acceptFromReadThread(line); (top of the "while" loop)
		// in:       // LogScanner::processingThread
		// for an explanation on how this works.
		barrier.wait();

		if(*linePtr != "EXIT")
		{
			// Execute the regexp.
			boost::match_results<std::string::const_iterator>  result;
			*regexpPtrResult = boost::regex_search(*linePtr, result, expr);

			if(*regexpPtrResult)
			{
				std::array<std::string, 4> IPaArr;
				storeIPaddrInArray(result[1], boost::ref(IPaArr));

				threadWkArea->saveIPaddreess(boost::ref(IPaArr), checkForWhitelist(result[1]));

				if(threadWkArea->isWhitelisted())
				{
					insertIntoAudtTable(logfileData, threadWkArea, linePtr, rgxpThreadID);
				}
				else
				{
					if(logfileData->getRegexpSvrt(rgxpThreadID) == '0')
					{
						insertIntoHistTable(logfileData, threadWkArea, linePtr, rgxpThreadID);
						insertIntoDropTable(boost::ref(IPaArr), "LogScanner::regexpExecThread()", linePtr);
					}
					else
					{
						insertIntoEvalTable(logfileData, threadWkArea, linePtr, rgxpThreadID);
					}
				}
			}

			barrier.wait();

			// By now everyone is done and ready to save lines that were multiMatch(ed)
			if(threadWkArea->getMatchCount() > 1 && *regexpPtrResult)
			{
				insertIntoAudtTable(logfileData, threadWkArea, linePtr, rgxpThreadID);
			}
		}
//LogManager::getInstance()->consoleMsg(("DBG_OUT_OF_if_IN_regexpExecThread " + threadWkArea->getThreadName() + "_" + threadWkArea->getProcThreadID() + "\t(" + std::string(caller) + ")\t<=>	" + *linePtr).c_str());  //FIXTHIS!!!
	}

	LogManager::getInstance()->consoleMsg((">>>>> EXITING regexpExecThread (" + std::string(caller) + ") " + threadWkArea->getThreadName() + "_" + threadWkArea->getProcThreadID() + "_" + rgxpThreadIDstr).c_str());
}

bool LogScanner::chkEvalRegexps(std::array<std::string, 4> &IPaddr)
{
	// This function uses the regexps described in:
	// logscanner/config/evalregexps.conf
	// to inspect all the lines for a particular IP address.
	std::unique_ptr<sql::Connection> conn(connectToDB());
	bool                             dropIt = false;
//if(IPaddr[0]  = '193' && IPaddr[1]  = '46' && IPaddr[2]  = '255' && IPaddr[3]  = '184') LogManager::getInstance()->logEvent("DBG_chkEvalRegexps_193.46.255.184", (opCode + " => " + IPaddr[0] + "." + IPaddr[1] + "." + IPaddr[2] + "." + IPaddr[3] + " => " + std::to_string((int)matchCount.size())).c_str());  //FIXTHIS!!!
	std::string sql2go("SELECT Logline "
					   "FROM T010_Eval "
					   "WHERE Octect_1=" + IPaddr[0] + " "
					   "AND   Octect_2=" + IPaddr[1] + " "
					   "AND   Octect_3=" + IPaddr[2] + " "
					   "AND   Octect_4=" + IPaddr[3] + ";");
// SELECT Logline FROM T010_Eval WHERE Octect_1=193 AND   Octect_2=46 AND   Octect_3=255 AND   Octect_4=184;
	try
	{
		std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(sql2go.data()));
		std::unique_ptr<sql::ResultSet>         resulSelect(stmnt->executeQuery());

		if(resulSelect != nullptr && resulSelect->rowsCount() > m_MAX_HITCOUNT_TO_CHK)
		{
			for(auto evalRegex : m_evalRegexps)
			{
				std::string   opCode(evalRegex.first.first);
				int           factor(evalRegex.second);
				boost::regex  expr(evalRegex.first.second);

				// This loop matchCounts every equal result that is matched.
				std::map<std::string, int> matchCount;
				while(resulSelect->next())
				{
					boost::smatch result;
					std::string   Logline(resulSelect->getString("Logline"));

					if(boost::regex_search(Logline, result, expr))
					{
						auto iter = matchCount.find(result[1].str());

						if(iter == matchCount.end())
						{
							matchCount.insert({result[1].str(), 1});
						}
						else
						{
							matchCount[result[1]] = iter->second + 1;
						}
					}
				}
				resulSelect->beforeFirst();
//FIXTHIS!!!   The whole opcode thing needs to be overhauled to accept more opcodes...
				if(opCode == "SEEN_MANY_STRINGS")
				{
					if((int)matchCount.size() > factor)
					{
//LogManager::getInstance()->logEvent("DBG_chkEvalRegexps_DROP", (opCode + " => " + IPaddr[0] + "." + IPaddr[1] + "." + IPaddr[2] + "." + IPaddr[3] + " => " + std::to_string((int)matchCount.size())).c_str());  //FIXTHIS!!!
						dropIt = true;
					}
				}
				else if(opCode == "TOO_MANY_ATTEMPTS")
				{
					for(auto iter : matchCount)
					{
						if(iter.second > factor)
						{
//LogManager::getInstance()->logEvent("DBG_chkEvalRegexps_DROP", (opCode + " => " + IPaddr[0] + "." + IPaddr[1] + "." + IPaddr[2] + "." + IPaddr[3] + " => " + std::to_string((int)matchCount.size())).c_str());  //FIXTHIS!!!
							dropIt = true;
						}
					}
				}
//LogManager::getInstance()->logEvent("DBG_chkEvalRegexps_SQLSET", "beforeFirst() => " + opCode + " = > " + junk2dbg);  //FIXTHIS!!!
				matchCount.clear();
			}
		}
	}
	catch(sql::SQLException &e)//FIXTHIS!!!  The whole try thing...
	{
		std::string errmsg('\t' + std::string(e.what()) + '\t' + sql2go);
		LogManager::getInstance()->logEvent("chkEvalRegexps_CATCH", errmsg.c_str());
//		LogManager::getInstance()->consoleMsg(("chkEvalRegexps_CATCH *DEAD*\t" + errmsg).c_str());
	}

	conn->close();

	return dropIt;
}

// function to convert a date from a logfile into seconds from epoch.
std::string LogScanner::lineTimestamp2Seconds(ConfContainer *logfileData, const std::string &line)
{
//	std::string regexp(logfileData->getDateExtract(ConfContainer::REGEXP));
	std::string bldMap(logfileData->getDateExtract(ConfContainer::BLDMAP));
//	std::string format(logfileData->getDateExtract(ConfContainer::FORMAT));
int JUNK_MAKE_A_TEST_4_lineTimestamp2Seconds; //FIXTHIS!!!
	boost::match_results<std::string::const_iterator> result;
	std::string::const_iterator                       begin = line.begin(),
	                                                  end   = line.end();
	boost::regex dateRx(logfileData->getDateExtract(ConfContainer::REGEXP));//(regexp);
	std::string  datetimeString;
	char         str4atoi[2] = { 0x00 };

//std::cout << "line => " << line.substr(0, 125) << std::endl;
	if(boost::regex_search(begin, end, result, dateRx, boost::match_default))
	{
		const char *cursor = bldMap.data();
		while((long unsigned int)(cursor - bldMap.data()) < bldMap.size())
		{
			if(isdigit(*cursor))
			{
				*str4atoi = *cursor;
				datetimeString += result[atoi(str4atoi)];
			}
			else
			{
				datetimeString += std::string(1, *cursor);
			}

			cursor++;
		}
	}
//	std::cout << "datetimeString => " << datetimeString << std::endl;
	tm tmStruct = {};
	std::istringstream ss(datetimeString);
	ss >> std::get_time(&tmStruct, (logfileData->getDateExtract(ConfContainer::FORMAT)).c_str());

	return  std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::from_time_t(mktime(&tmStruct))));
}

std::string LogScanner::createTimestamp()
{
	// It's highly unlikely but we can get a duplicated timestamp.
	char  timestamp[LogManager::TIMESTAMP_SIZE + 1];
	LogManager::getInstance()->nanosecTimestamp(timestamp);
//	std::string timestamp(std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count()));
////	std::string timestamp(std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count()));

	return timestamp;
}

bool LogScanner::hasIPaddress(std::string &line)
{
	// Here we check if at least it looks like there is an IP address,
	// the IP extraction happens at the logfile configuration.
	static char IP_REGEXP[] = "[^[:digit:]]*([[:digit:]]{1,3}\\.[[:digit:]]{1,3}\\.[[:digit:]]{1,3}\\.[[:digit:]]{1,3}[^[:digit:]])";
	boost::match_results<std::string::const_iterator> result;
//	boost::regex                                      addr(m_IP_REGEXP);
	boost::regex                                      addr(IP_REGEXP);
//LogManager::getInstance()->consoleMsg((std::string("DBG_hasIPaddress => ") + (boost::regex_search(line, result, addr) ? "T => " + result[1] : "F") + " <=> " + line).c_str());  //FIXTHIS!!!
	return boost::regex_search(line, result, addr);
}

bool LogScanner::checkForWhitelist(const std::string &IPaddr)
{
	std::map<std::string, std::string>::iterator ipInfo = m_whitelist.begin();
	bool                                         isWhitelisted = false;
/*
	std::array<std::string, 4> ipAddr;
	threadWkArea->getIPaddrOctects(boost::ref(ipAddr));

	std::string dot(".");
	std::string IPaddr(ipAddr[0] + dot + ipAddr[1] + dot + ipAddr[2] + dot + ipAddr[3]);
*/
	while(!isWhitelisted && ipInfo != m_whitelist.end())
	{
		// Is the IP we found in the white list?
		isWhitelisted = (IPaddr.substr(0, ipInfo->first.length()) == ipInfo->first);
		ipInfo++;
	}

	return isWhitelisted;
}

void LogScanner::monitorNewLineThread(ConfContainer *logfileData)
{
	logfileData->monitorNewLineThread(logfileData->getLogFilePath().string());
}

void LogScanner::monitor4RenameThread(ConfContainer *logfileData)
{
	logfileData->monitor4RenameThread(logfileData->getLogFilePath().string());
}

void LogScanner::insertIntoSeenTable(ConfContainer *logfileData, std::string &line, bool regexMatch)
{
	static std::string tableName("T100_LinesSeen");

	// Extract NotifyEvent pointer from map.
	static std::map<std::string, std::shared_ptr<NotifyEvent>>::iterator notifyEvent(m_NotifyEvent_list.find(tableName));

	// SQL file name..
	static std::string   baseName(std::string(m_SQL_IPC_PATH) + "/" + tableName + "/" + tableName + "_");

	std::ofstream  sqlInfo;
	std::string    sqlInfoFile(baseName + createTimestamp() + ".");

	// Build SQL insert.
	// Same line can come from more than 1 log, so ignore duplicates.
	std::string sql2go("INSERT IGNORE INTO " + tableName + " VALUES (" +
	    m_QUOTE + createTimestamp()                                    + m_QUOTE + ", ?, ?, " +
	              (regexMatch ? "true" : "false") + ");");

	sqlInfo.open((sqlInfoFile + "tmp").data(), std::ios_base::trunc);

	// Save SQL insert
	sqlInfo << sql2go << std::endl;

	// Argument count for the insert
	sqlInfo << "2" << std::endl;

	// Type and value of the arguments
	sqlInfo << "S" << logfileData->getLogFilePath().string() << std::endl;
	sqlInfo << "S" << line                                   << std::endl;

	sqlInfo.close();

	rename((sqlInfoFile + "tmp").data(), (sqlInfoFile + "txt").data());

	// Notify LogScanner::execSqlInsert thread for this table.
//LogManager::getInstance()->consoleMsg(("DBG_NOTIFYING insert into table (insertIntoSeenTable)!!!\t" + tableName + '\t' + sql2go).c_str());  //FIXTHIS!!!
	notifyEvent->second->notifyNew();
}

void LogScanner::insertIntoAudtTable(ConfContainer *logfileData, Workarea *threadWkArea, std::string *linePtr, int rgxpThreadID)
{
	static std::string tableName("T020_Audit");

	// Extract NotifyEvent pointer from map.
	static std::map<std::string, std::shared_ptr<NotifyEvent>>::iterator notifyEvent(m_NotifyEvent_list.find(tableName));

	// SQL file name..
	static std::string   baseName(std::string(m_SQL_IPC_PATH) + "/" + tableName + "/" + tableName + "_");

	std::ofstream  sqlInfo;
	std::string    sqlInfoFile(baseName + createTimestamp() + ".");

	std::array<std::string, 4> ipAddr;
	threadWkArea->getIPaddrOctects(boost::ref(ipAddr));

	// Build SQL insert.
	std::string sql2go("INSERT INTO " + tableName + " VALUES (" +
	    m_QUOTE + lineTimestamp2Seconds(logfileData, *linePtr)  + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[0]                                     + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[1]                                     + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[2]                                     + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[3]                                     + m_QUOTE + ", ?, ?, ?, " +
	    m_QUOTE + logfileData->getRegexpSvrt(rgxpThreadID)      + m_QUOTE + ", " +
	              (threadWkArea->getMatchCount() > 1 ? "true" : "false") + ", " +
	              (threadWkArea->isWhitelisted()     ? "true" : "false") + ");");

	sqlInfo.open((sqlInfoFile + "tmp").data(), std::ios_base::trunc);

	// Save SQL insert
	sqlInfo << sql2go << std::endl;

	// Argument count for the insert
	sqlInfo << "3" << std::endl;

	// Type and value of the arguments
	sqlInfo << "S" << logfileData->getLogFilePath().string()   << std::endl;
	sqlInfo << "S" << *linePtr                                 << std::endl;
	sqlInfo << "S" << logfileData->getRegexpText(rgxpThreadID) << std::endl;

	sqlInfo.close();

	rename((sqlInfoFile + "tmp").data(), (sqlInfoFile + "txt").data());

	// Notify LogScanner::execSqlInsert thread for this table.
//LogManager::getInstance()->consoleMsg(("DBG_NOTIFYING insert into table (insertIntoAudtTable)!!!\t" + tableName + '\t' + sql2go).c_str());  //FIXTHIS!!!
	notifyEvent->second->notifyNew();
}

void LogScanner::insertIntoEvalTable(ConfContainer *logfileData, Workarea *threadWkArea, std::string *linePtr, int rgxpThreadID)
{
	static std::string tableName("T010_Eval");

	// Extract NotifyEvent pointer from map.
	static std::map<std::string, std::shared_ptr<NotifyEvent>>::iterator notifyEvent(m_NotifyEvent_list.find(tableName));

	// SQL file name..
	static std::string   baseName(std::string(m_SQL_IPC_PATH) + "/" + tableName + "/" + tableName + "_");

	std::ofstream  sqlInfo;
	std::string    sqlInfoFile(baseName + createTimestamp() + ".");

	std::array<std::string, 4> ipAddr;
	threadWkArea->getIPaddrOctects(boost::ref(ipAddr));

	// Build SQL insert.
	std::string sql2go("INSERT INTO " + tableName + " VALUES (" +
	    m_QUOTE + lineTimestamp2Seconds(logfileData, *linePtr)  + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[0]                                     + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[1]                                     + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[2]                                     + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[3]                                     + m_QUOTE + ", ?, ?, ?, " +
	    m_QUOTE + logfileData->getRegexpSvrt(rgxpThreadID)      + m_QUOTE + ", " +
	    m_QUOTE + createTimestamp()                             + m_QUOTE + ");");

	sqlInfo.open((sqlInfoFile + "tmp").data(), std::ios_base::trunc);

	// Save SQL insert
	sqlInfo << sql2go << std::endl;

	// Argument count for the insert
	sqlInfo << "3" << std::endl;

	// Type and value of the arguments
	sqlInfo << "S" << logfileData->getLogFilePath().string()   << std::endl;
	sqlInfo << "S" << *linePtr                                 << std::endl;
	sqlInfo << "S" << logfileData->getRegexpText(rgxpThreadID) << std::endl;

	sqlInfo.close();

	rename((sqlInfoFile + "tmp").data(), (sqlInfoFile + "txt").data());

	// Notify LogScanner::execSqlInsert thread for this table.
//LogManager::getInstance()->consoleMsg(("DBG_NOTIFYING insert into table (insertIntoEvalTable)!!!\t" + tableName + '\t' + sql2go).c_str());  //FIXTHIS!!!
	notifyEvent->second->notifyNew();
}

void LogScanner::insertIntoHistTable(ConfContainer *logfileData, Workarea *threadWkArea, std::string *linePtr, int rgxpThreadID)
{
	static std::string tableName("T040_Hist");

	// Extract NotifyEvent pointer from map.
	static std::map<std::string, std::shared_ptr<NotifyEvent>>::iterator notifyEvent(m_NotifyEvent_list.find(tableName));

	// SQL file name..
	static std::string   baseName(std::string(m_SQL_IPC_PATH) + "/" + tableName + "/" + tableName + "_");

	std::ofstream  sqlInfo;
	std::string    sqlInfoFile(baseName + createTimestamp() + ".");

	std::array<std::string, 4> ipAddr;
	threadWkArea->getIPaddrOctects(boost::ref(ipAddr));

	// Build SQL insert.
	std::string sql2go("INSERT INTO " + tableName + " VALUES (" +
	    m_QUOTE + lineTimestamp2Seconds(logfileData, *linePtr)  + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[0]                                     + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[1]                                     + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[2]                                     + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[3]                                     + m_QUOTE + ", ?, ?, ?, " +
	    m_QUOTE + logfileData->getRegexpSvrt(rgxpThreadID)      + m_QUOTE + ", " +
	    m_QUOTE + createTimestamp()                             + m_QUOTE + ");");

	sqlInfo.open((sqlInfoFile + "tmp").data(), std::ios_base::trunc);

	// Save SQL insert
	sqlInfo << sql2go << std::endl;

	// Argument count for the insert
	sqlInfo << "3" << std::endl;

	// Type and value of the arguments
	sqlInfo << "S" << logfileData->getLogFilePath().string()   << std::endl;
	sqlInfo << "S" << *linePtr                                 << std::endl;
	sqlInfo << "S" << logfileData->getRegexpText(rgxpThreadID) << std::endl;

	sqlInfo.close();

	rename((sqlInfoFile + "tmp").data(), (sqlInfoFile + "txt").data());

	// Notify LogScanner::execSqlInsert thread for this table.
//LogManager::getInstance()->consoleMsg(("DBG_NOTIFYING insert into table (insertIntoHistTable)!!!\t" + tableName + '\t' + sql2go).c_str());  //FIXTHIS!!!
	notifyEvent->second->notifyNew();
}

void LogScanner::insertIntoDropTable(std::array<std::string, 4> &ipAddr, const char *ProcName, std::string *lineOrTmstmp, char Status)
{
	static std::string tableName("T030_Drop");

	// Extract NotifyEvent pointer from map.
	static std::map<std::string, std::shared_ptr<NotifyEvent>>::iterator notifyEvent(m_NotifyEvent_list.find(tableName));

	// SQL file name..
	static std::string   baseName(std::string(m_SQL_IPC_PATH) + "/" + tableName + "/" + tableName + "_");

	std::ofstream  sqlInfo;
	std::string    sqlInfoFile(baseName + createTimestamp() + ".");

	// Build SQL insert.
//	std::string sql2go(writeSqlInsertForDrop(boost::ref(ipAddr), "LogScanner::insertIntoDropTable", 'N'));
	std::string sql2go( "INSERT IGNORE INTO T030_Drop VALUES ("      +
	                 m_QUOTE + createTimestamp() + m_QUOTE + ", "    +
	                 m_QUOTE + ipAddr[0]         + m_QUOTE + ", "    +
	                 m_QUOTE + ipAddr[1]         + m_QUOTE + ", "    +
	                 m_QUOTE + ipAddr[2]         + m_QUOTE + ", "    +
	                 m_QUOTE + ipAddr[3]         + m_QUOTE + ", "    +
	                 m_QUOTE + ProcName          + m_QUOTE + ", ?, " +
	                 m_QUOTE + Status            + m_QUOTE + ");");

	sqlInfo.open((sqlInfoFile + "tmp").data(), std::ios_base::trunc);

	// Save SQL insert
	sqlInfo << sql2go << std::endl;

	// Argument count for the insert
	sqlInfo << "1" << std::endl;

	// Type and value of the arguments
	sqlInfo << "S";
	if(lineOrTmstmp)
	{
		sqlInfo << *lineOrTmstmp;
	}
	sqlInfo << std::endl;

	sqlInfo.close();

	rename((sqlInfoFile + "tmp").data(), (sqlInfoFile + "txt").data());

	// Notify LogScanner::execSqlInsert thread for this table.
//LogManager::getInstance()->consoleMsg(("DBG_NOTIFYING insert into table (nsertIntoDropTable)!!!\t" + tableName + '\t' + sql2go).c_str());  //FIXTHIS!!!
	notifyEvent->second->notifyNew();
}

void LogScanner::execBatchSQL(std::string &sql2go)
{
	std::unique_ptr<sql::Connection> conn(connectToDB());

	try
	{
		std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(sql2go.data()));
		stmnt->execute();
	}
//	catch(sql::SQLSyntaxErrorException &e)
//	{
//		std::string errMsg('\t' + std::string(e.what()) + '\t' + sql2go);
//		LogManager::getInstance()->logEvent("execBatchSQL_CATCH", errMsg.c_str());
//		LogManager::getInstance()->consoleMsg((std::string("*DEAD*\t") +  errMsg).c_str());
//	}
	catch(sql::SQLException &e)
	{
		std::string errMsg('\t' + std::string(e.what()) + '\t' + sql2go);
		LogManager::getInstance()->logEvent("execBatchSQL_CATCH", errMsg.c_str());
//		LogManager::getInstance()->consoleMsg((std::string("execBatchSQL_CATCH *DEAD*\t") +  errMsg).c_str());
	}

	conn->close();
}

sql::Connection *LogScanner::connectToDB()
{
	sql::Connection *conn = nullptr;
	int              fail = 0;

	do
	{
		conn = m_driver->connect(m_url, m_properties);

		if(!conn->isValid())
		{
			if(fail >= m_MAX_CONN_FAILURES_TO_QUIT)
			{
				LogManager::getInstance()->consoleMsg("*** ERROR *** Unable to to reconnect to DB, sleeping (Zzzzzzzzzzzzzz...) for a minute...");
				sleep(60);

				fail = 0;
			}
			else
			{
				fail++;
			}
		}
	}
	while(!conn->isValid());

	return conn;
}

void LogScanner::storeIPaddrInArray(const std::string &IPaddr, std::array<std::string, 4> &IPaArr)
{
	const char *cursor = IPaddr.c_str();
	int         Idx    = 0;

	while(Idx < 4)
	{
		char  octect[4] = {};
		char *digit     = &(octect[0]);
		int   fence     = 0; // This is here to avoid an overrun...

		while(*cursor != '.' && *cursor && fence < 3)
		{
			*digit = *cursor;
			digit++;
			cursor++;
			fence++;
		}
		cursor++;
		*digit = '\0';

		IPaArr[Idx] = octect;
		Idx++;
	}
int JUNK_TEST_WHICH_ONE_OF_THESE_IS_FASTER; //FIXTHIS!!!
//	std::stringstream ss(IPaddr);

//	for(int Idx = 0; Idx < 4; Idx++)
//	{
//		getline(ss, IPaArr[Idx], '.');
//	}
}

int LogScanner::fullTrimmedLine(std::string &line, char **fistChar, char **lastChar)
{
	int lineLen = line.length();
	int result  = lineLen;

	if(lineLen > 0)
	{
		char *lineStrt = line.data();
		char *leftchar = lineStrt;
		char *ritechar = lineStrt + lineLen - 1;

		while(leftchar < ritechar && isspace(*leftchar))
		{
			leftchar++;
		}

		while(ritechar >= lineStrt && isspace(*ritechar))
		{
			ritechar--;
		}

		if(fistChar != nullptr)
		{
			*fistChar = leftchar;
		}

		if(lastChar != nullptr)
		{
			*lastChar = ritechar;
		}

		result = ritechar - leftchar + 1;
	}
	else
	{
		if(fistChar != nullptr)
		{
			*fistChar = line.data();
		}

		if(lastChar != nullptr)
		{
			*lastChar = line.data();
		}
	}

	return result;
}
/*END*/
