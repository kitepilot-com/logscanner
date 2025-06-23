/*******************************************************************************
 * File: LogScanner/LogScanner.cpp                                             *
 * Created:     Jan-07-25                                                      *
 * Last update: May-17-25                                                      *
 * Top level program file.                                                     *
 *******************************************************************************/

#include <time.h>
#include <stdlib.h>
//#include <unistd.h>
#include <sys/inotify.h>
#include <sys/socket.h>
//#include <sys/types.h>
#include <ctime>
#include <vector>
#include <iomanip>
#include <sstream>
#include <string>
#include <fstream>
#include <iostream>
#include <boost/regex.hpp>
#include <boost/thread.hpp>

#include "LogManager.h"
#include "LogScanner.h"
#include "LumpContainer.h"

bool        LogScanner::KEEP_LOOPING = false;
std::string LogScanner::m_QUOTE      = "\"";

LogScanner::LogScanner() :
//	m_QUOTE("\""),
	m_url(m_DB_URL),
	m_properties({{"user", m_DB_USER}, {"password", m_DB_PSWD}}),
	m_driver(sql::mariadb::get_driver_instance()),
	m_notifyEval(false),
	m_notifyDrop(false)
{
	LogManager::init();

	uploadConfiguration();

	// if m_state == LogScanner::INVALID, then we didn't find any problems.
	if(m_state == LogScanner::INVALID && initDB() == true)
	{
		// Start app's comm server.
		m_fdSocketSrv = 0;
		m_fdAccept    = 0;
		if(createServer(m_fdSocketSrv, m_fdAccept, m_SYS_SOCKET, m_SYS_CONNECTS))
		{
			try
			{
				boost::interprocess::named_semaphore::remove(m_NOTIFY_EVAL);
				boost::interprocess::named_semaphore(boost::interprocess::create_only_t(), m_NOTIFY_EVAL, 0);

				boost::interprocess::named_semaphore::remove(m_NOTIFY_DROP);
				boost::interprocess::named_semaphore(boost::interprocess::create_only_t(), m_NOTIFY_DROP, 0);

				m_evalsmphr = new boost::interprocess::named_semaphore(boost::interprocess::open_only_t(), m_NOTIFY_EVAL);
				m_dropSmphr = new boost::interprocess::named_semaphore(boost::interprocess::open_only_t(), m_NOTIFY_DROP);

				m_state = LogScanner::HEALTHY;
				KEEP_LOOPING = true;
			}
			catch(...)
			{
				LogManager::getInstance()->consoleMsg("*** FATAL *** Unable to implement semaphore(s) " + std::string(m_NOTIFY_DROP) + " or " + std::string(m_NOTIFY_EVAL));
				m_state = LogScanner::SMPHR_ERR;
			}

		}
	}
}

LogScanner::~LogScanner()
{
	for(auto logfileData : m_logfileList)
	{
		delete logfileData;
	}

	boost::interprocess::named_semaphore::remove(m_NOTIFY_EVAL);
	boost::interprocess::named_semaphore::remove(m_NOTIFY_DROP);

	delete m_evalsmphr;
	delete m_dropSmphr;
}

int LogScanner::exec()
{
	LogManager::getInstance()->consoleMsg("=====> Entering LogScanner::exec()");

	if(m_state == LogScanner::HEALTHY)
	{
		LogManager::getInstance()->consoleMsg();
		LogManager::getInstance()->consoleMsg("**********************************");
		LogManager::getInstance()->consoleMsg("****    logscanner STARTED    ****");
		LogManager::getInstance()->consoleMsg("**********************************");
		LogManager::getInstance()->consoleMsg();

		LogManager::getInstance()->consoleMsg("LogScanner => Hello World...");

		boost::thread *comm = new boost::thread(&LogScanner::commThread, this);

		// ARCHITECTURE
		// ============
		// The main program spawns 3 main threads at startup:
		// The Input Thread, the Evaluation Thread and the Drop Thread.
//std::cout << "EEEEXIIIT!!!!" << std::endl;exit(0);
//std::cout << "Zzzzzzzzzzzzzzzzzzzz..." << std::endl;sleep(3600);
		boost::thread *inpt = new boost::thread(&LogScanner::inptThread, this);
		boost::thread *eval = new boost::thread(&LogScanner::evalThread, this);
		boost::thread *drop = new boost::thread(&LogScanner::dropThread, this);

		comm->join();
		drop->join();
		eval->join();
		inpt->join();

		delete drop;
		delete eval;
		delete inpt;
		delete comm;

		LogManager::getInstance()->consoleMsg("==> All threads finished...");
		LogManager::getInstance()->consoleMsg("==> Bye bye cruel World");
	}
	else
	{
		LogManager::getInstance()->consoleMsg("*** FATAL *** Bad configuration or unable to start comm server...");
		LogManager::getInstance()->consoleMsg("Search lor for '*** FATAL ***' for more info...");
	}

	if(m_state != LogScanner::HEALTHY)
	{
		LogManager::getInstance()->consoleMsg();
		LogManager::getInstance()->consoleMsg("********************************************************");
		LogManager::getInstance()->consoleMsg("****    Ensure that no other instance is running    ****");
		LogManager::getInstance()->consoleMsg("****    Execute as 'root'                           ****");
		LogManager::getInstance()->consoleMsg("********************************************************");
	}

	return m_state;
}

void LogScanner::uploadConfiguration(std::filesystem::path confPath)
{
	// This function loads (readDirectoryTree)
	// and parses (loadAndParseConfFiles) configuration files
	// From: logscanner/LogScanner.h
	//          void uploadConfiguration(std::filesystem::path confPath = m_BASE_PATH);
	//          static constexpr char m_BASE_PATH[] = "config";
	LogManager::getInstance()->consoleMsg("*********************************************");
	LogManager::getInstance()->consoleMsg("****   Initiating configuration upload   ****");
	LogManager::getInstance()->consoleMsg("*********************************************");
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
				LogManager::getInstance()->consoleMsg("Found " + std::to_string(m_confCount) + " configration files");

				// std::vector<ConfContainer *>   m_logfileList;
				// This loop validates and outputs to stdout the final configuration.
				LogManager::getInstance()->consoleMsg();
				LogManager::getInstance()->consoleMsg("**********************************************************");
				LogManager::getInstance()->consoleMsg("vvvv => BEGIN CONFIGURATION VALIDATION AND SUMMARY <= vvvv");
				LogManager::getInstance()->consoleMsg("**********************************************************");
				for(auto logfileData : m_logfileList)
				{
					LogManager::getInstance()->consoleMsg();
					LogManager::getInstance()->consoleMsg("==> Begin " + logfileData->getLogFilePath().string());

					if(logfileData->getObjStatus() == ConfContainer::HEALTHY)
					{
						std::ifstream    readTest(logfileData->getLogFilePath());

						if(exists(logfileData->getLogFilePath()) && is_regular_file(logfileData->getLogFilePath()) && readTest.is_open())
						{
//							LogManager::getInstance()->consoleMsg(logfileData->getLogFilePath());
							readTest.close();

							LogManager::getInstance()->consoleMsg("Date REGEXP\t=>" + logfileData->getDateExtract(ConfContainer::REGEXP) + "<=");
							LogManager::getInstance()->consoleMsg("Date BLDMAP\t=>" + logfileData->getDateExtract(ConfContainer::BLDMAP) + "<=");
							LogManager::getInstance()->consoleMsg("Date FORMAT\t=>" + logfileData->getDateExtract(ConfContainer::FORMAT) + "<=");
							LogManager::getInstance()->consoleMsg();

							for(int Idx = 0; Idx < logfileData->getRegexpCount(); Idx++)
							{
								LogManager::getInstance()->consoleMsg("Rxg ->\t" + logfileData->getRegexpSvrt(Idx) + "\t -\t=>" + logfileData->getRegexpText(Idx) + "<=");
							}

							std::string  lumpComponent(logfileData->getLumpComponent());
							if(lumpComponent != "")
							{
								LogManager::getInstance()->consoleMsg();
								LogManager::getInstance()->consoleMsg(">>\tBEGIN list of LUMP components:");

								do
								{
									LogManager::getInstance()->consoleMsg("\t>>\t" + lumpComponent);
									lumpComponent = logfileData->getLumpComponent();
								}
								while(lumpComponent != "");

								LogManager::getInstance()->consoleMsg(">>\tEND list of LUMP components.");
							}
						}
						else
						{
							LogManager::getInstance()->consoleMsg("*** FATAL *** Unable to open " + logfileData->getLogFilePath().string() + "for reading.");
							m_state = LogScanner::OPEN_ERR;
						}
					}
					else
					{
						LogManager::getInstance()->consoleMsg("*** FATAL *** Unable to init " + logfileData->getLogFilePath().string());
						m_state = LogScanner::INIT_ERR;
					}
					LogManager::getInstance()->consoleMsg("==> End " + logfileData->getLogFilePath().string());
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
						LogManager::getInstance()->consoleMsg("=>\t{" + ipInfo.first + "}\t-\t{" + ipInfo.second + "}");
					}
				}

				LogManager::getInstance()->consoleMsg("********************************************************");
				LogManager::getInstance()->consoleMsg("^^^^ => END CONFIGURATION VALIDATION AND SUMMARY <= ^^^^");
				LogManager::getInstance()->consoleMsg("********************************************************");
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
			LogManager::getInstance()->consoleMsg("*** FATAL *** File: '" + confPath.string() + "' does not exist or is not a directory.");
			m_state = LogScanner::EXIST_ERR;
		}
	}
	catch (const std::filesystem::filesystem_error &ex)
	{
		LogManager::getInstance()->consoleMsg("*** FATAL *** Something crashed while reading " + confPath.string() + " in uploadConfiguration." + ex.what());
		m_state = LogScanner::CRASH_ERR;
		throw;
	}
}

bool LogScanner::initDB()
{
	bool allGood = true;

	LogManager::getInstance()->consoleMsg();
	LogManager::getInstance()->consoleMsg("=====> ENTERING void LogScanner::initDB()");

	// Test Connection
	std::unique_ptr<sql::Connection> conn(m_driver->connect(m_url, m_properties));

	if(conn->isValid())
	{
		// Program starts and reads from iptables-save all dropped addresses.
		std::filesystem::create_directories("exec-log");
		std::filesystem::create_directories("run/logs");
		std::filesystem::create_directories("run/rules");
		std::filesystem::create_directories("run/tmp");

		std::string sysCmd("bin/get_DROPped_IPs.sh");
		std::string sysLog("run/logs/get_DROPped_IPs.log");
		std::string sysRes("run/tmp/dropped-IPs.txt");
		std::string sysExe(sysCmd + " > " + sysLog + " 2>&1");

		LogManager::getInstance()->consoleMsg("EXEC =>\tExecuting " + sysCmd + " in directory " + std::filesystem::current_path().string());
		LogManager::getInstance()->consoleMsg("EXEC =>\tSee log at " + sysLog + " and result in " + sysRes);
		LogManager::getInstance()->consoleMsg("EXEC =>\t" + sysExe);
//		std::system(sysCmd.data());
		std::system(sysExe.data());

		std::ifstream    droppedIPs(sysRes.data());
		std::string      IPaddr;

		if(droppedIPs.is_open())
		{
			LogManager::getInstance()->consoleMsg("Reading currently blocked IPs from iptables...");

			std::string  sql2go;

			int IPcounter = 0;
			while(std::getline(droppedIPs, IPaddr))
			{
				std::array<std::string, 4> IPaArr;
				storeIPaddrInArray(IPaddr, boost::ref(IPaArr));

				std::string sql2go(writeInsertDropFor(boost::ref(IPaArr), 'D'));

				try
				{
					// Execute query
					std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(sql2go.data()));
					stmnt->executeUpdate();

					IPcounter++;

				}
				catch(sql::SQLException &e)
				{
					std::string errmsg('\t' + std::string(e.what()) + '\t' + sql2go);
					LogManager::getInstance()->logEvent("initDB_CATCH", errmsg);
					LogManager::getInstance()->consoleMsg(std::string("*** FATAL *** while INSERT(ing) INTO T030_Drop in initDB()\t" + errmsg));
					m_state = LogScanner::CRASH_ERR;
				}
			}
			LogManager::getInstance()->consoleMsg("Added " + std::to_string(IPcounter) + " currently blocked IPs from iptables to table...");

			droppedIPs.close();
		}

		conn->close();

		LogManager::getInstance()->consoleMsg("=====> EXITING initDB()");
		LogManager::getInstance()->consoleMsg();
	}
	else
	{
		LogManager::getInstance()->consoleMsg(std::string("*** FATAL ***  Can't connect to database"));
		m_state = LogScanner::CONN_ERR;
		allGood = false;
	}

	return allGood;
}

// This function will read the directory m_BASE_PATH to load a sorted tree.
int LogScanner::readDirectoryTree(std::filesystem::path confPath, std::set<std::filesystem::path> &sortedTree, std::set<std::filesystem::path> &emptyDirs)
{
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
				LogManager::getInstance()->consoleMsg("*** WARNING *** Empty directory " + child.path().string());
				emptyDirs.insert(child);
			}
			fileCount++;
//std::cout <<  std::endl; //FIXTHIS!!!
		}
		else
		{
			LogManager::getInstance()->consoleMsg("*** FATAL *** " + child.path().string() + " exists, but is not a regular file or directory");
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
					LogManager::getInstance()->consoleMsg("\t\t\t*** WARNING *** Empty directory " + confPath.string());
					m_state = LogScanner::EMPTY_ERR;
				}
			}
		}
	}
	catch (const std::filesystem::filesystem_error& ex)
	{
		LogManager::getInstance()->consoleMsg(std::string("*** FATAL *** Something crashed while parsing config in loadAndParseConfFiles") + ex.what());
		m_state = LogScanner::CRASH_ERR;
		throw;
	}
}

void LogScanner::parseConfOf(std::filesystem::path confPath)
{
	static const int basePathSize = strlen(m_BASE_PATH);
	static const int fileTypeSize = strlen(m_FILE_TYPE);

	bool allGood = true;

	try
	{
		LogManager::getInstance()->consoleMsg("Parsing " + confPath.string());

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
//				if(line.length() > 0)
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
									LogManager::getInstance()->consoleMsg("\t-> Got rexexp => '" + line.substr(0, 1) + "' - " +  regexp+ "<=");

									if(regexpList.find(regexp) == regexpList.end())
									{
										regexpList.insert({regexp, *cursor});
										regexpCount++;
									}
									else
									{
										LogManager::getInstance()->consoleMsg("*** FATAL *** DUPLICATE rexexp in file " + confPath.string() + " line " + std::to_string(lineCtr));
										m_state = LogScanner::DUPRGXP_ERR;
										allGood = false;
									}
								}
								else
								{
									// Got information to extract date from logfile...
									std::string extrctInfo(cursor + 2);
									LogManager::getInstance()->consoleMsg("\t-> Got date extract info => '" + line.substr(0, 1) + "' - " + extrctInfo + "<=");

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
										LogManager::getInstance()->consoleMsg("*** FATAL *** DUPLICATE date extract info in file " + confPath.string() + " line " + std::to_string(lineCtr));
										m_state = LogScanner::DUPDTINFO_ERR;
										allGood = false;
									}
								}
							}
							else
							{
								LogManager::getInstance()->consoleMsg("*** FATAL *** Line #" + std::to_string(lineCtr) + " unrecognized char 1 or not TAB in char 2 in file " + confPath.string());
								m_state = LogScanner::STRUCT_ERR;
								allGood = false;
							}
						}
						else
						{
							LogManager::getInstance()->consoleMsg("*** FATAL *** Line #" + std::to_string(lineCtr) + " too short in file " + confPath.string());
							m_state = LogScanner::TOOSHORT_ERR;
							allGood = false;
						}
					}
				}

				lineCtr++;
			}

			if(allGood)
			{
				if(extrctCount == 3)
				{
					if(regexpCount > 0)
					{
						ConfContainer *logfileData = nullptr;

						// Line below converts the name in the "config" directory to the name of the logfile to read.
						std::string logfilePath(confPath.string().substr(basePathSize, confPath.string().length() - basePathSize - fileTypeSize));
						LogManager::getInstance()->consoleMsg("\t\tStoring " + logfilePath + " and " + std::to_string(regexpList.size()) + " regexp(s).");

						// We need to discern whether we have or not a LUMP configuration file:
						if(logfilePath.find(LumpContainer::m_LUMP) == std::string::npos)
						{
							// This is a plain vanilla logfile.
							logfileData = new ConfContainer(logfilePath, dateExtrct, regexpList);
						}
						else
						{
							// This is a LUMP logfile.
							logfileData = new LumpContainer(logfilePath, dateExtrct, regexpList);
						}

						if(logfileData->getObjStatus() == ConfContainer::HEALTHY)
						{
							m_logfileList.push_back(logfileData);
							m_confCount++;
						}
					}
					else
					{
						LogManager::getInstance()->consoleMsg("*** FATAL *** No rexeps found in " + confPath.string());
						m_state = LogScanner::NORXGP_ERR;
					}
				}
				else
				{
					LogManager::getInstance()->consoleMsg("*** FATAL *** " + std::to_string(3 - extrctCount) + " 'date extract {R(exgexp), M(ap), or F(ormat)}' items missing in " + confPath.string());
					m_state = LogScanner::DATEEXT_ERR;
				}
			}

			regexpList.clear();

			LogManager::getInstance()->consoleMsg("DONE parsing " + confPath.string());
			LogManager::getInstance()->consoleMsg();
		}
		else
		{
			LogManager::getInstance()->consoleMsg("*** FATAL *** Unable to read " + confPath.string());
			m_state = LogScanner::FILE_ERR;
		}
	}
	catch (const std::filesystem::filesystem_error& ex)
	{
		LogManager::getInstance()->consoleMsg("*** FATAL *** Unable to parse " + confPath.string() + " => " + ex.what());
		m_state = LogScanner::CRASH_ERR;
	}
}

void LogScanner::uploadWhitelistedIPs()
{
	std::filesystem::path confPath("data/whitelist.txt");

	if(exists(confPath) && is_regular_file(confPath))
	{
		LogManager::getInstance()->consoleMsg();
		LogManager::getInstance()->consoleMsg(">> Reading Whitelist file " + confPath.string());

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
							LogManager::getInstance()->consoleMsg("*** FATAL *** Duplicated entry " + IPaddr + "in whitelist file " + confPath.string());
							m_state = LogScanner::WHITE_ERR;
						}
					}
				}
			}
		}
		else
		{
			LogManager::getInstance()->consoleMsg("*** FATAL *** Unable to read whitelist " + confPath.string());
			m_state = LogScanner::WHITE_ERR;
		}
	}
	else
	{
		LogManager::getInstance()->consoleMsg("Whitelist file " + confPath.string() + " not found...");
		LogManager::getInstance()->consoleMsg();
	}
}

void LogScanner::commThread()
{
	LogManager::getInstance()->consoleMsg(">> commThread => Hello World");

	struct sockaddr remote;
//FIXTHIS!!!   All this...
	while (KEEP_LOOPING)
	{
		unsigned int sockLen = 0;

		memset(&remote, 0x00, sizeof(struct sockaddr));

		if((m_fdAccept = accept(m_fdSocketSrv, (struct sockaddr*)&remote, &sockLen)) > 0)
		{
			LogManager::getInstance()->consoleMsg("comm server accepting connections in " + std::string(m_SYS_SOCKET));

			// We will get an empty string, 2 should be sufficient...
			char recvBuff[2] = { 0x00 };
			int  dataRecv    = recv(m_fdAccept, recvBuff, sizeof(recvBuff), 0);

			if(dataRecv > 0)
			{
int JUNK; LogManager::getInstance()->consoleMsg("End of the ride message received..."); //FIXTHIS!!!
KEEP_LOOPING = false;       //FIXTHIS!!!
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

	LogManager::getInstance()->consoleMsg(">> commThread => Bye bye cruel World");
}

void LogScanner::inptThread()
{
	// a.- The Input Thread:
	// ---------------------
	// This main thread starts as many "reading threads" as there are configuration files.
	LogManager::getInstance()->consoleMsg(">> inptThread => Hello World");

	std::vector<boost::thread *> allThreads;

	for(auto logfileData : m_logfileList)
	{
		// Start a "Reading Thread" for every logfile.
		boost::thread *thread = new boost::thread(&LogScanner::readingThread, this, logfileData);
		allThreads.push_back(thread);
	}

	for(boost::thread *thread : allThreads)
	{
		thread->join();
	}

	for(boost::thread *thread : allThreads)
	{
		delete thread;
	}

	LogManager::getInstance()->consoleMsg(">> inptThread => Bye bye cruel World");
}

void LogScanner::evalThread()
{
	LogManager::getInstance()->consoleMsg(">> evalThread => Hello World");
// SELECT DISTINCT MIN(LogTmstmp) AS frstSeen, MAX(LogTmstmp) AS lastSeen, CONCAT(Octect_1, '.', Octect_2, '.', Octect_3, '.', Octect_4) AS IPaddr, COUNT(*) AS hitCount FROM T010_Eval GROUP BY IPaddr ORDER BY hitCount DESC;
// INSERT INTO T040_Hist VALUES (Octect_1=Arr[1], ..., Timestmp=EvalTimestamp), (Octect_1=Arr[1], ..., Timestmp=EvalTimestamp), (Octect_1=Arr[1], ..., Timestmp=EvalTimestamp);
	while(KEEP_LOOPING)
	{

		wait4NotifyEval();

		if(KEEP_LOOPING)
		{
			// We have rows to inspect...
			std::unique_ptr<sql::Connection> conn(connectToDB());

			std::vector<std::array<std::string, 4> > allDroppedIPs;
			std::vector<std::array<std::string, 4> > allDelEvalIPs;
			std::vector<unsigned int>                allLastSeenTs;

			// Below we get a list of IP addresses and how many requests per second they have had in so many seconds.
			// We get rows like this here:
			// frstSeen    lastSeen    IPaddr          hitCount
			// 1747761912   1747761951  143.198.197.57  357
			// 1747777831   1747778118  179.43.149.114  63
			// 1747730320   1747794090  145.239.10.137  9
			// Any IP address hiting more than 2 requests per second: ((float)hitCount / (float)(lastSeen - frstSeen)) > 2.0f
			// is blocked and the lines moved to T040_Hist.
			// Records older than an hour (counting from the first IP found below 2 requests per second) are deleted.
// SELECT DISTINCT MIN(LogTmstmp) AS frstSeen, MAX(LogTmstmp) AS lastSeen, COUNT(*) AS hitCount, CONCAT(Octect_1, '.', Octect_2, '.', Octect_3, '.', Octect_4) AS IPaddr FROM T010_Eval GROUP BY IPaddr ORDER BY hitCount DESC;
			std::string sql2go("SELECT DISTINCT MIN(LogTmstmp)                                                AS frstSeen, "
			                                   "MAX(LogTmstmp)                                                AS lastSeen, "
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
					while(resulSelect->next())
					{
						unsigned long long int frstSeen = atoi(resulSelect->getString("frstSeen"));
						unsigned long long int lastSeen = atoi(resulSelect->getString("lastSeen"));
						unsigned           int hitCount = atoi(resulSelect->getString("hitCount"));

						std::array<std::string, 4> IPaddr;
						std::stringstream ss(resulSelect->getString("IPaddr").c_str());

						for(int Idx = 0; Idx < 4; Idx++)
						{
							getline(ss, IPaddr[Idx], '.');
						}

						// We drop IP when we see more than m_MAX_REQUEST_X_SEC in m_MAX_SECONDS_TO_KEEP
						if(lastSeen > frstSeen && (((float)hitCount / (float)(lastSeen - frstSeen)) > m_MAX_REQUEST_X_SEC) && (lastSeen - frstSeen > m_MAX_SECONDS_TO_KEEP))
						{
							// This IP will be inserted in DROP and the lines in Eval will be moved to Hist;
							allDroppedIPs.push_back(IPaddr);
						}
						else
						{
							// Any record of this IP (frstSeen + 60 seconds) will be deleted from Eval.
							allDelEvalIPs.push_back(IPaddr);
							allLastSeenTs.push_back(lastSeen);
						}
					}
				}
			}
			catch(sql::SQLException &e)
			{
				std::string errmsg('\t' + std::string(e.what()) + '\t' + sql2go);
				LogManager::getInstance()->logEvent("evalThread_1_CATCH", errmsg);
				LogManager::getInstance()->consoleMsg("*DEAD*\t" + errmsg);
			}

			conn->close();

			if(allDroppedIPs.size() > 0)
			{
				for(auto dropIP : allDroppedIPs)
				{
					std::string moveEvalRows2Hist;

					// This IP will be inserted in DROP
					// and the lines in Eval will be moved to Hist;
					insertIntoDropTable(boost::ref(dropIP));
					moveEvalRows2Hist += ("INSERT INTO T040_Hist SELECT * FROM T010_Eval WHERE Octect_1=" + dropIP[0] +
					                                                                     " AND Octect_2=" + dropIP[1] +
					                                                                     " AND Octect_3=" + dropIP[2] +
					                                                                     " AND Octect_4=" + dropIP[3] + ";\n");

					moveEvalRows2Hist += ("DELETE FROM T010_Eval WHERE Octect_1=" + dropIP[0] +
					                                             " AND Octect_2=" + dropIP[1] +
					                                             " AND Octect_3=" + dropIP[2] +
					                                             " AND Octect_4=" + dropIP[3] + ";\n");


					execBatchSQL(moveEvalRows2Hist);
				}

				notifyDrop();
			}

			if(allDelEvalIPs.size() > 0)
			{
				std::string delEvalRows;

				for(std::size_t Idx = 0; Idx < allDelEvalIPs.size(); Idx++)
				{
					// Delete anything older than (m_MAX_SECONDS_TO_KEEP * 10)
					delEvalRows += ("DELETE FROM T010_Eval WHERE LogTmstmp < " + std::to_string(allLastSeenTs[Idx] - (m_MAX_SECONDS_TO_KEEP * 10)) +
					                                       " AND Octect_1=" + allDelEvalIPs[Idx][0] +
					                                       " AND Octect_2=" + allDelEvalIPs[Idx][1] +
					                                       " AND Octect_3=" + allDelEvalIPs[Idx][2] +
					                                       " AND Octect_4=" + allDelEvalIPs[Idx][3] + ";\n");

				}

				execBatchSQL(delEvalRows);
			}
		}
	}

	LogManager::getInstance()->consoleMsg(">> evalThread => Bye bye cruel World");
}

void LogScanner::dropThread()
{
	LogManager::getInstance()->consoleMsg(">> dropThread => Hello World");

	while(KEEP_LOOPING)
	{
		wait4NotifyDrop();

		if(KEEP_LOOPING)
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
				LogManager::getInstance()->logEvent("dropThread_CATCH", errmsg);
				LogManager::getInstance()->consoleMsg("*DEAD*\t" + errmsg);
			}

			conn->close();
		}
	}

	LogManager::getInstance()->consoleMsg(">> dropThread => Bye bye cruel World");
}

void LogScanner::readingThread(ConfContainer *logfileData)
{
	// Each line read by the Input Thread is handed via queue to any available standby "processing thread".
	// The "processing thread" hands the line to all "regexp eval threads" and waits for the result of the regexp evaluation.
	LogManager::getInstance()->consoleMsg(">>>>  STARTING readingThread for " + logfileData->getLogFilePath().string());

	std::vector<boost::thread *>   allThreads;

	// Start reading threads for all "LUMP" components.
	std::string  lumpComponent(logfileData->getLumpComponent());
	if(lumpComponent != "")
	{
		do
		{
			boost::thread *thread = new boost::thread(&LogScanner::lumpReadingThread, this, logfileData->getLogFilePath(), lumpComponent);
			allThreads.push_back(thread);

			lumpComponent = logfileData->getLumpComponent();
		}
		while(lumpComponent != "");
	}

	// Start m_totThreads "processing threads".
	for(int procThreadID = 0; procThreadID < m_totThreads; procThreadID++)
	{
		boost::thread *thread = new boost::thread(&LogScanner::processingThread, this, logfileData, procThreadID);
		allThreads.push_back(thread);
	}

	// createServer opens an abstract socket for listening.
	// The server waits for connections from threads that request a line.
	// The "request" is just an empty string.
	// The server then answers with a line read from the logfile.
	int fdSocketSrv = 0;
	int fdAccept    = 0;

	// Open log file to read.
	std::ifstream ifs(logfileData->getLogFilePath(), std::ios::in/* | std::ios_base::binary*/);

	if(ifs.is_open() && createServer(fdSocketSrv, fdAccept, logfileData->getThreadName(), allThreads.size()))
	{
		LogManager::getInstance()->consoleMsg("File good\t=>\t" + logfileData->getLogFilePath().string());

		std::string line;

		// Loop to read the log file.
		// Some "processing thread" will pick it up.
		while(KEEP_LOOPING)
		{
			// Read incoming line...
			while(KEEP_LOOPING && std::getline(ifs, line))
			{
				// We only care for lines that have IP address.
				if(hasIPaddress(line))
				{
					if(notEverSeen(line))
					{
LogManager::getInstance()->logEvent(("readingThread_GOT_from" + std::string(logfileData->getThreadName())).data(), line);  //FIXTHIS!!!
						logfileData->deliverToProcThreads(line);
						insertIntoSeenTable(logfileData, line);
					}
				}
//				else
//				{
//					LogManager::getInstance()->logEvent(("DBG_readingThread" + std::string(logfileData->getThreadName()) + "_NO_IP").data(), line);  //FIXTHIS!!!
//				}
			}

			if(KEEP_LOOPING && ifs.eof())
			{
				ifs.clear();
				// Now wait until a new line is added to the file, but...
				// There is a race condition here.
				// A line can be added to the file before the inotify gets "installed".
				// AFAIK that condition is impossible to manage unless we can control the process that adds the lines.
				// And we can't control the process that adds the lines...
				// Ah well...
				// The line will sit there until another line is added...
				inotifyLogfile(logfileData->getLogFilePath());
			}
		}
	}
	else
	{
		LogManager::getInstance()->consoleMsg("*** FATAL *** unable to open logfile " + logfileData->getLogFilePath().string() + " or start comm sever for " +  + "...");
	}

	for(boost::thread *thisThread : allThreads)
	{
		thisThread->join();
	}

	for(boost::thread *thisThread : allThreads)
	{
		delete thisThread;
	}

	LogManager::getInstance()->consoleMsg(">>>>  EXITING readingThread for " + logfileData->getLogFilePath().string());
}

void LogScanner::processingThread(ConfContainer *logfileData, int procThreadID)
{
	// The "processing thread" hands the line to all "regexp eval threads" and waits for the result of the regexp evaluation.
	Workarea                       threadWkArea(logfileData, procThreadID);
	std::string                    line;
	std::string                   *linePtr = &line;
	std::vector<boost::thread *>   allThreadsIDs;
	boost::barrier                 barrier(logfileData->getRegexpCount() + 1);

	LogManager::getInstance()->consoleMsg(">>>>  STARTING processingThread " + threadWkArea.getProcThreadID() + " for " + logfileData->getLogFilePath().string());

	// Each "processing thread" creates an "evaluation thread" for every regexp to be evaluated.
	for(int rgxpThreadID = 0; rgxpThreadID < logfileData->getRegexpCount(); rgxpThreadID++)
	{
		boost::thread *thread = new boost::thread(&LogScanner::regexpExecThread, this, linePtr, &threadWkArea, boost::ref(barrier), rgxpThreadID);
		allThreadsIDs.push_back(thread);
	}

	// Loop to assign line to all regexps.
	while(KEEP_LOOPING)
	{
		// Get a line.
		logfileData->acceptFromReadThread(line);
LogManager::getInstance()->logEvent(("acceptFromReadThread_GOT_from" + std::string(logfileData->getThreadName()) + "_" + threadWkArea.getProcThreadID()).data(), line);  //FIXTHIS!!!
		if(KEEP_LOOPING)
		{
			barrier.wait();

			// We need to get the 1st occurrence of IP address in the line.
			// to save it with threadWkArea.saveIPaddreess(...).
			boost::match_results<std::string::const_iterator> result;
			std::string::const_iterator                       begin = line.begin(),
															  end   = line.end();
			boost::regex                                      addr(m_IP_REGEXP);

			regex_search(begin, end, result, addr, boost::match_default);
			threadWkArea.saveIPaddreess(result[1]);

			// The boost::barriers holds all the treads and they will have
			// a valid pointer in outcome[#], a single check will do...

			barrier.wait();

			// Lines caught by several regexp are inserted in Audit table.
			// Lines not caught at all are inserted in text file Audit-line.
			// Lines caught by regexps with "Severity_val == 0" are inserted in Hist and in Drop
			// Lines caught by regexps with "Severity_val == 1" are undenifined as of yet.
			// Lines caught by regexps with "Severity_val > 1" are inserted in Eval.
			if(threadWkArea.getMatchCount() > 0)
			{
//				threadWkArea.setTimestamp(createTimestamp());
				threadWkArea.setWhitelisted(checkForWhitelist(&threadWkArea));

				for(int rgxpThreadID = 0; rgxpThreadID < logfileData->getRegexpCount(); rgxpThreadID++)
				{
					if(*(threadWkArea.getRegexpPtrResult(rgxpThreadID)) == true)
					{
						if(!threadWkArea.isWhitelisted())
						{
							if(logfileData->getRegexpSvrt(rgxpThreadID) == "0")
							{
								std::array<std::string, 4> ipAddr;
								threadWkArea.rcvrIPaddreess(boost::ref(ipAddr));

								insertIntoDropTable(boost::ref(ipAddr));
							}
							else
							{
								insertIntoHistTable(logfileData, &threadWkArea, line, rgxpThreadID);
								insertIntoEvalTable(logfileData, &threadWkArea, line, rgxpThreadID);
							}
						}
//if(threadWkArea.isWhitelisted())   LogManager::getInstance()->logEvent(("DBG_processingThread_WHITELISTED" + threadWkArea.getThreadName() + "_" + threadWkArea.getProcThreadID()).data(), line);  //FIXTHIS!!!
if(threadWkArea.getMatchCount() > 1) LogManager::getInstance()->logEvent(("DBG_processingThread_MULTIMATCH"  + threadWkArea.getThreadName() + "_" + threadWkArea.getProcThreadID()).data(), line);  //FIXTHIS!!!
						if(threadWkArea.isWhitelisted() || threadWkArea.getMatchCount() > 1)
						{
							insertIntoAditTable(logfileData, &threadWkArea, line, rgxpThreadID);
						}
					}
				}
			}
			else
			{
				// Lines not caught at all are inserted in text file Audit-line.
				LogManager::getInstance()->logNoMatch(logfileData->getThreadName(), line);
			}

			threadWkArea.clearMatchCount();
		}
	}

for(boost::thread *thisThread : allThreadsIDs)
{
	thisThread->join();
}

for(boost::thread *thisThread : allThreadsIDs)
{
	delete thisThread;
	thisThread = nullptr;
}

	LogManager::getInstance()->consoleMsg(">>>>  EXITING processingThread " + threadWkArea.getProcThreadID() + " for " + logfileData->getLogFilePath().string());
}

void LogScanner::regexpExecThread(std::string *linePtr, Workarea *threadWkArea, boost::barrier &barrier, int rgxpThreadID)
{
	std::string rgxpThreadIDstr("0" + std::to_string(rgxpThreadID));
	rgxpThreadIDstr = rgxpThreadIDstr.substr(rgxpThreadIDstr.length() - 2, 2);

	LogManager::getInstance()->consoleMsg(">>>>  STARTING regexpExecThread " + threadWkArea->getThreadName() + "_" + threadWkArea->getProcThreadID() + "_" + rgxpThreadIDstr);

	boost::match_results<std::string::const_iterator>   result;
	boost::regex                                        expr(threadWkArea->getRegexpText(rgxpThreadID));
	bool                                               *regexpPtrResult = threadWkArea->getRegexpPtrResult(rgxpThreadID);

	while(KEEP_LOOPING)
	{
		barrier.wait();

		// Execute the regexp.
		boost::match_results<std::string::const_iterator>  result;
		boost::regex                                       expr(threadWkArea->getRegexpText(rgxpThreadID));

		*regexpPtrResult = boost::regex_search(*linePtr, result, expr);

		if(*regexpPtrResult)
		{
			threadWkArea->addMatchCount();
		}

		barrier.wait();
	}

	LogManager::getInstance()->consoleMsg(">>>>  EXITING regexpExecThread " + threadWkArea->getThreadName() + "_" + threadWkArea->getProcThreadID() + "_" + rgxpThreadIDstr);
}

void LogScanner::lumpReadingThread(std::filesystem::path logFilePath, std::string lumpComponent)
{
	LogManager::getInstance()->consoleMsg(">>>>  STARTING lumpReadingThread for " + logFilePath.string() + " => " + lumpComponent);

	boost::mutex       writeMutex;

	std::ifstream ifs(lumpComponent, std::ios::in/* | std::ios_base::binary*/);

	if(ifs.is_open())
	{
		LogManager::getInstance()->consoleMsg("Lump component " + lumpComponent + " for " + logFilePath.string() + " is good.");

		std::string line;
		while(KEEP_LOOPING)
		{
			// Read incoming line...
			while(KEEP_LOOPING && std::getline(ifs, line))
			{
				// We only care for lines that have IP address.
				// hand this line to any processingThread,
				// The line is pushed into logfileInputQueue to be read by a processingThread thread.
//std::ostringstream ss; ss << boost::this_thread::get_id(); std::string idstr = ss.str(); //FIXTHIS!!!
//LogManager::getInstance()->logEvent(("lumpReadingThread_writeLineToLumpFile_CHK_IP_" + idstr).data(), line);  //FIXTHIS!!!
				if(hasIPaddress(" " + line))
				{
//LogManager::getInstance()->logEvent(("lumpReadingThread_writeLineToLumpFile_" + idstr + "_GOT").data(), line);  //FIXTHIS!!!
					writeLineToLumpFile(&writeMutex, lumpComponent + '	' + line,  logFilePath);
				}
			}

			if(KEEP_LOOPING && ifs.eof())
			{
				ifs.clear();
				inotifyLogfile(lumpComponent);
			}
		}
	}

	LogManager::getInstance()->consoleMsg(">>>>  EXITING lumpReadingThread for " + logFilePath.string() + " => " + lumpComponent);
}

void LogScanner::writeLineToLumpFile(boost::mutex *writeMutex, std::string line, std::filesystem::path &logFilePath)
{
	boost::mutex::scoped_lock lock(*writeMutex);

	std::ofstream  myfile;
	myfile.open(logFilePath.string().data(), std::ios_base::app);
//LogManager::getInstance()->logEvent("writeLineToLumpFile_GOT", line);  //FIXTHIS!!!
	myfile << line << std::endl;
	myfile.close();
}

bool LogScanner::createServer(int &fdSocketSrv, int &fdAccept, const char* socketPath, int maxConnections)
{
	struct sockaddr_un local;
	int                dataSize = 0;
	bool               allGood  = false;
//FIXTHIS!!!
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
				LogManager::getInstance()->consoleMsg("Socket " + std::string(socketPath) + " is listening."); //FIXTHIS!!!  Do I want this here?
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

	return allGood;
}

// function to convert a date from a logfile into seconds from epoch.
std::string LogScanner::lineTimestamp2Seconds(ConfContainer *logfileData, const std::string &line)
{
	std::string regexp(logfileData->getDateExtract(ConfContainer::REGEXP));
	std::string bldMap(logfileData->getDateExtract(ConfContainer::BLDMAP));
	std::string format(logfileData->getDateExtract(ConfContainer::FORMAT));

	boost::match_results<std::string::const_iterator> result;
	std::string::const_iterator                       begin = line.begin(),
	                                                  end   = line.end();
	boost::regex                                      dateRx(regexp);
	std::string                                       datetimeString;
	char                                              str4atoi[2] = { 0x00 };

//std::cout << "line => " << line.substr(0, 125) << std::endl;
	if(regex_search(begin, end, result, dateRx, boost::match_default))
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
	ss >> std::get_time(&tmStruct, format.c_str());

	return  std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::from_time_t(mktime(&tmStruct))));
}

std::string LogScanner::createTimestamp()
{
	// Avoid a duplicated timestamp.
	boost::mutex::scoped_lock lock(m_mutex4Timestamp);
//FIXTHIS!!!   This function seems to be duplicated, see LogManager..?.
	static const int TIMESTAMP_SIZE = 19;
	std::string timestamp(std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count()));
//	std::string timestamp(std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count()));

	while(timestamp.length() < TIMESTAMP_SIZE)
	{
		timestamp += "0";
	}

	return timestamp;
}

bool LogScanner::hasIPaddress(std::string line)
{
	// We need this to get 1st IP address in line,
	// otherwise we the last...
	boost::match_results<std::string::const_iterator> result;
	std::string::const_iterator                       begin = line.begin(),
	                                                  end   = line.end();
	boost::regex                                      addr(m_IP_REGEXP);

	return regex_search(begin, end, result, addr, boost::match_default);
}

bool LogScanner::notEverSeen(std::string line)
{
	std::unique_ptr<sql::Connection> conn(connectToDB());
	bool                             seen = true;

	std::string sql2go = "SELECT Logline FROM T100_LinesSeen WHERE Logline = \"" + escapeString(line) + "\";";

	try
	{
		std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(sql2go.data()));
		std::unique_ptr<sql::ResultSet>         resulSelect(stmnt->executeQuery());

		if(resulSelect != nullptr && resulSelect->rowsCount() > 0)
		{
			seen = false;
		}
	}
	catch(sql::SQLException &e)//FIXTHIS!!!  The whole try thing...
	{
		std::string errmsg('\t' + std::string(e.what()) + '\t' + sql2go);
		LogManager::getInstance()->logEvent("notEverSeen_CATCH", errmsg);
		LogManager::getInstance()->consoleMsg("*DEAD*\t" + errmsg);
	}

	conn->close();

	return seen;
}

bool LogScanner::checkForWhitelist(Workarea *threadWkArea)
{
	std::map<std::string, std::string>::iterator ipInfo = m_whitelist.begin();
	bool                                         isWhitelisted = false;

	std::array<std::string, 4> ipAddr;
	threadWkArea->rcvrIPaddreess(boost::ref(ipAddr));

	std::string dot(".");
	std::string IPaddr(ipAddr[0] + dot + ipAddr[1] + dot + ipAddr[2] + dot + ipAddr[3]);

	while(!isWhitelisted && ipInfo != m_whitelist.end())
	{
		// Is the IP we found in the white list?
		isWhitelisted = (IPaddr.substr(0, ipInfo->first.length()) == ipInfo->first);
		ipInfo++;
	}

	return isWhitelisted;
}

void LogScanner::insertIntoSeenTable(ConfContainer *logfileData, std::string &line)
{
	// mysql is dead-locking here, so...
	boost::mutex::scoped_lock lock(m_mutex4sql);

	// Same line can come from 2 logs, so ignore duplicates.
	std::string sql2go("INSERT IGNORE INTO T100_LinesSeen VALUES ("                     +
	    m_QUOTE + createTimestamp()                                    + m_QUOTE + ", " + 
	    m_QUOTE + escapeString(logfileData->getLogFilePath().string()) + m_QUOTE + ", " + 
	    m_QUOTE + escapeString(line)                                   + m_QUOTE + ");");

	std::unique_ptr<sql::Connection> conn(connectToDB());

	try
	{
		std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(sql2go.data()));
		stmnt->execute();
	}
	catch(sql::SQLException &e)
	{
		std::string errMsg("\nWhat:" + std::string(e.what()) + '\t' + logfileData->getLogFilePath().string() + "\nsql2go:" + sql2go + '\t' + line);
		LogManager::getInstance()->logEvent("insertIntoSeenTable_CATCH", errMsg);
		LogManager::getInstance()->consoleMsg(std::string("*DEAD*\t") + errMsg);
	}

	conn->close();
}

void LogScanner::insertIntoAditTable(ConfContainer *logfileData, Workarea *threadWkArea, std::string &line, int rgxpThreadID)
{
	// mysql is dead-locking here, so...
	boost::mutex::scoped_lock lock(m_mutex4sql);

	std::array<std::string, 4> ipAddr;
	threadWkArea->rcvrIPaddreess(boost::ref(ipAddr));

	std::string sql2go("INSERT INTO T020_Audit VALUES ("                                  +
	    m_QUOTE + lineTimestamp2Seconds(logfileData, line)               + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[0]                                              + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[1]                                              + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[2]                                              + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[3]                                              + m_QUOTE + ", " +
	    m_QUOTE + escapeString(logfileData->getLogFilePath().string())   + m_QUOTE + ", " +
	    m_QUOTE + escapeString(line)                                     + m_QUOTE + ", " +
	    m_QUOTE + escapeString(logfileData->getRegexpText(rgxpThreadID)) + m_QUOTE + ", " +
	    m_QUOTE + logfileData->getRegexpSvrt(rgxpThreadID)               + m_QUOTE + ", " +
	              (threadWkArea->getMatchCount() > 1 ? "true" : "false")           + ", " +
	              (threadWkArea->isWhitelisted() ? "true" : "false")               + ");");

	std::unique_ptr<sql::Connection> conn(connectToDB());

	try
	{
		std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(sql2go.data()));
		stmnt->execute();
	}
	catch(sql::SQLException &e)
	{
		std::string errMsg('\t' + std::string(e.what()) + '\t' + sql2go + '\t' + line);
		LogManager::getInstance()->logEvent("insertIntoAditTable_CATCH", errMsg);
		LogManager::getInstance()->consoleMsg(std::string("*DEAD*\t") +  errMsg);
	}

	conn->close();
}

void LogScanner::insertIntoEvalTable(ConfContainer *logfileData, Workarea *threadWkArea, std::string &line, int rgxpThreadID)
{
	// mysql is dead-locking here, so...
	boost::mutex::scoped_lock lock(m_mutex4sql);

	std::array<std::string, 4> ipAddr;
	threadWkArea->rcvrIPaddreess(boost::ref(ipAddr));

	std::string sql2go("INSERT INTO T010_Eval VALUES ("                                   +
	    m_QUOTE + lineTimestamp2Seconds(logfileData, line)               + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[0]                                              + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[1]                                              + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[2]                                              + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[3]                                              + m_QUOTE + ", " +
	    m_QUOTE + escapeString(logfileData->getLogFilePath().string())   + m_QUOTE + ", " +
	    m_QUOTE + escapeString(line)                                     + m_QUOTE + ", " +
	    m_QUOTE + escapeString(logfileData->getRegexpText(rgxpThreadID)) + m_QUOTE + ", " +
	    m_QUOTE + logfileData->getRegexpSvrt(rgxpThreadID)               + m_QUOTE + ", " +
	    m_QUOTE + createTimestamp()                                      + m_QUOTE + ");");

	std::unique_ptr<sql::Connection> conn(connectToDB());

	try
	{
		std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(sql2go.data()));
		stmnt->execute();

		notifyEval();
	}
	catch(sql::SQLException &e)
	{
		std::string errMsg('\t' + std::string(e.what()) + '\t' + sql2go + '\t' + line);
		LogManager::getInstance()->logEvent("insertIntoEvalTable_CATCH", errMsg);
		LogManager::getInstance()->consoleMsg(std::string("*DEAD*\t") +  errMsg);
	}

	conn->close();
}

void LogScanner::insertIntoHistTable(ConfContainer *logfileData, Workarea *threadWkArea, std::string &line, int rgxpThreadID)
{
	// mysql is dead-locking here, so...
	boost::mutex::scoped_lock lock(m_mutex4sql);

	std::array<std::string, 4> ipAddr;
	threadWkArea->rcvrIPaddreess(boost::ref(ipAddr));

	std::string sql2go("INSERT INTO T040_Hist VALUES ("                                    +
	    m_QUOTE + lineTimestamp2Seconds(logfileData, line)               + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[0]                                              + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[1]                                              + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[2]                                              + m_QUOTE + ", " +
	    m_QUOTE + ipAddr[3]                                              + m_QUOTE + ", " +
	    m_QUOTE + escapeString(logfileData->getLogFilePath().string())   + m_QUOTE + ", " + 
	    m_QUOTE + escapeString(line)                                     + m_QUOTE + ", " + 
	    m_QUOTE + escapeString(logfileData->getRegexpText(rgxpThreadID)) + m_QUOTE + ", " + 
	    m_QUOTE + logfileData->getRegexpSvrt(rgxpThreadID)               + m_QUOTE + ", " +
	    m_QUOTE + createTimestamp()                                      + m_QUOTE + ");");

	std::unique_ptr<sql::Connection> conn(connectToDB());

	try
	{
		std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(sql2go.data()));
		stmnt->execute();
	}
	catch(sql::SQLException &e)
	{
		std::string errMsg('\t' + std::string(e.what()) + '\t' + sql2go + '\t' + line);
		LogManager::getInstance()->logEvent("insertIntoHistTable_CATCH", errMsg);
		LogManager::getInstance()->consoleMsg(std::string("*DEAD*\t") +  errMsg);
	}

	conn->close();
}

void LogScanner::insertIntoDropTable(std::array<std::string, 4> &ipAddr, char status)
{
	// mysql is dead-locking here, so...
	boost::mutex::scoped_lock lock(m_mutex4sql);

	std::string sql2go(writeInsertDropFor(boost::ref(ipAddr)));

	std::unique_ptr<sql::Connection> conn(connectToDB());

	try
	{
		std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(sql2go.data()));
		stmnt->execute();

		notifyDrop();
	}
	catch(sql::SQLException &e)
	{
		std::string errMsg('\t' + std::string(e.what()) + '\t' + sql2go);
		LogManager::getInstance()->logEvent("insertIntoDropTable_CATCH", errMsg);
		LogManager::getInstance()->consoleMsg(std::string("*DEAD*\t") +  errMsg);
	}

	conn->close();
}

std::string LogScanner::writeInsertDropFor(std::array<std::string, 4> &ipAddr, char Status)
{
	return "INSERT IGNORE INTO T030_Drop VALUES (" +
	    m_QUOTE + createTimestamp() + m_QUOTE + ", " + 
	    m_QUOTE + ipAddr[0]         + m_QUOTE + ", " + 
	    m_QUOTE + ipAddr[1]         + m_QUOTE + ", " + 
	    m_QUOTE + ipAddr[2]         + m_QUOTE + ", " + 
	    m_QUOTE + ipAddr[3]         + m_QUOTE + ", " + 
	    m_QUOTE + Status            + m_QUOTE +  ");";
}

void LogScanner::execBatchSQL(std::string &sql2go)
{
	// mysql is dead-locking here, so...
	boost::mutex::scoped_lock lock(m_mutex4sql);

	std::unique_ptr<sql::Connection> conn(connectToDB());

	try
	{
		std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement(sql2go.data()));
		stmnt->execute();
	}
	catch(sql::SQLException &e)
	{
		std::string errMsg('\t' + std::string(e.what()) + '\t' + sql2go);
		LogManager::getInstance()->logEvent("execBatchSQL_CATCH", errMsg);
		LogManager::getInstance()->consoleMsg(std::string("*DEAD*\t") +  errMsg);
	}

	conn->close();
}

void LogScanner::notifyDrop()
{
	m_dropSmphr->post();
	setNotifyDrop(true);
}

void LogScanner::wait4NotifyDrop()
{
	setNotifyDrop(false);
}

void LogScanner::setNotifyDrop(bool flag)
{
	boost::mutex::scoped_lock lock(m_mutex4NotifyDrop);

	if(flag == true)
	{
		m_notifyDrop = true;
	}
	else
	{
		if(m_notifyDrop == false)
		{
			m_dropSmphr->wait();
		}

		m_notifyDrop = false;
	}
}

void LogScanner::notifyEval()
{
	m_evalsmphr->post();
	setNotifyEval(true);
}

void LogScanner::wait4NotifyEval()
{
	setNotifyEval(false);
}

void LogScanner::setNotifyEval(bool flag)
{
	boost::mutex::scoped_lock lock(m_mutex4NotifyEval);

	if(flag == true)
	{
		m_notifyEval = true;
	}
	else
	{
		if(m_notifyEval == false)
		{
			m_evalsmphr->wait();
		}

		m_notifyEval = false;
	}
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

void LogScanner::storeIPaddrInArray(std::string &IPaddr, std::array<std::string, 4> &IPaArr)
{
	std::stringstream ss(IPaddr);

	for(int Idx = 0; Idx < 4; Idx++)
	{
		getline(ss, IPaArr[Idx], '.');
	}
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

std::string LogScanner::escapeString(std::string srcstr)
{
	// This function escapes '\', '"' and "'".
	// I can't find a stupid standard SQL function to do this.

	// The 1st and 2 lines below are (1st) from this app logs ("escaped out" by this function)
	// and (2nd) from the apache logs itself.
	// The next 2 are the same 2 lines with the clutter removed to highlight the case.
	// From logs:
	// *DEAD* 186.167.166.225 - - [19/May/2025:10:00:58 -0400] \"GET /guapo4.jpg HTTP/1.1\" 200 36952 \"https://myactivity.google.com/\" \"Mozilla/5.0 (Linux; Android 14; CRT-LX3 Build/HONORCRT-L33; wv) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Chrome/135.0.7049.111 Mobile Safari/537.36 OcIdWebView ({\\"os\\":\\"Android\\",\\"osVersion\\":\\"34\\",\\"app\\":\\"com.google.android.gms\\",\\"appVersion\\":\\"251633000\\",\\"style\\":2,\\"callingAppId\\":\\"com.google.android.googlequicksearchbox\\",\\"isDarkTheme\\":false})\"";
	//        186.167.166.225 - - [19/May/2025:10:00:58 -0400] "GET /guapo4.jpg HTTP/1.1" 200 36952 "https://myactivity.google.com/" "Mozilla/5.0 (Linux; Android 14; CRT-LX3 Build/HONORCRT-L33; wv) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Chrome/135.0.7049.111 Mobile Safari/537.36 OcIdWebView ({\"os\":\"Android\",\"osVersion\":\"34\",\"app\":\"com.google.android.gms\",\"appVersion\":\"251633000\",\"style\":2,\"callingAppId\":\"com.google.android.googlequicksearchbox\",\"isDarkTheme\":false})"
	// ({\\"os\\":\\"Android\\",\\"osVersion\\":\\"34\\",\\"app\\":\\"com.google.android.gms\\",\\"appVersion\\":\\"251633000\\",\\"style\\":2,\\"callingAppId\\":\\"com.google.android.googlequicksearchbox\\",\\"isDarkTheme\\":false})\"";
	// ({\"os\":\"Android\",\"osVersion\":\"34\",\"app\":\"com.google.android.gms\",\"appVersion\":\"251633000\",\"style\":2,\"callingAppId\":\"com.google.android.googlequicksearchbox\",\"isDarkTheme\":false})"
	std::string   str2go;
	const char   *character = srcstr.data();

	for(std::size_t Idx = 0; Idx < srcstr.length(); character++, Idx++)
	{
		// Check that we have a quote that is not already escaped (like the comment above).
		if(*character == '"' && character > srcstr.data() && *(character - 1) != '\\')
		{
			str2go += "\\";
		}

		str2go += *character;
	}

	//FIXTHIS!!!
	// This is here becauase stupid mysql lib will turn not '\\' to to '\' on insert.
	// It does it in the client.
	// But then if the string ends in '\' the inswert will blow up.
	// So I check for the last '\' ...   :(
	if(*(character - 1) == '\\')
	{
		str2go += "\\";
	}

	// This line goes into a table, make it fit, no column is bigger than m_MYSQL_MAX_INDEX_LENGTH
	return str2go.substr(0, m_MYSQL_MAX_INDEX_LENGTH);
}

void LogScanner::inotifyLogfile(std::filesystem::path logFilePath)
{
// SEE:
// https://stackoverflow.com/questions/32281277/too-many-open-files-failed-to-initialize-inotify-the-user-limit-on-the-total
// echo 2048 > /proc/sys/user/max_inotify_instances
//std::cout << LogManager::getInstance()->nanosecTimestamp() << '\t' << "::::::> DBG   ENTERING LogScanner::inotifyLogfile(" << logFilePath.string() << ")" << std::endl;
	// From: https://developer.ibm.com/tutorials/l-ubuntu-inotify/
	std::string logPath(logFilePath.parent_path().string());
	std::string logName(logFilePath.filename().string());

	int EVENT_SIZE = ( sizeof (struct inotify_event) );
	int BUF_LEN    = ( 1024 * ( EVENT_SIZE + 16 ) );

	int length, Idx = 0;
	int fd;
	int wd;
	char buffer[BUF_LEN];

	bool fileChanged = false;

	fd = inotify_init();

	if(fd < 0 ) {
		perror( "inotify_init" ); //FIXTHIS!!!
	}

	wd = inotify_add_watch( fd, logPath.data(), IN_MODIFY | IN_CREATE | IN_DELETE );
	length = read( fd, buffer, BUF_LEN );

	if(length < 0 ) {
		perror( "read" ); //FIXTHIS!!!
	}

	while(KEEP_LOOPING && Idx < length && !fileChanged)
	{
		struct inotify_event *event = ( struct inotify_event * ) &buffer[Idx];

		if(event->len )
		{
			if(strcmp(event->name, logName.data()) == 0 )
			{
				if(event->mask & IN_CREATE )
				{
					if(event->mask & IN_ISDIR )
					{// printf( "The directory %s was created.\n", event->name );
					}
					else
					{// printf( "The file %s was created.\n", event->name );
					}
				}
				else if(event->mask & IN_DELETE )
				{
					if(event->mask & IN_ISDIR )
					{// printf( "The directory %s was deleted.\n", event->name );
					}
					else
					{// printf( "The file %s was deleted.\n", event->name );
					}
				}
				else if(event->mask & IN_MODIFY )
				{
					if(event->mask & IN_ISDIR )
					{// printf( "The directory %s was modified.\n", event->name );
					}
					else
					{
						fileChanged = true;
					}
				}
			}
		}

		Idx += EVENT_SIZE + event->len;
	}

	( void ) inotify_rm_watch( fd, wd );
	( void ) close( fd );
}
/*END*/
