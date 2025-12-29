// logscanner/test/inotify/inotifyThread.cpp

// clear;(cd logscanner/test/inotify/ && g++ -Wall -o inotifyThread inotifyThread.cpp  -lboost_thread)
// (cd logscanner/test/inotify/ && ./inotifyThread.sh 2>&1) > /tmp/junk-inotifyThread.log;less -S /tmp/junk-inotifyThread.log

#include <signal.h>
#include <map>
#include <set>
#include <atomic>
#include <vector>
#include <sys/inotify.h>
#include <errno.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <atomic>
#include <boost/thread.hpp>

// :%s/LogManager::getInstance()->consoleMsg/consoleMsg/g
// :%s/consoleMsg/LogManager::getInstance()->consoleMsg/g
#define consoleMsg(MSG) std::cout << MSG << std::endl
////////////////////////////   COPY THIS   vvvvvvvvvvvvvvvvvvvvvvvvvvv    TO "logscanner/INotifyObj.h"
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
	protected:
		typedef enum {
			_IN_MODIFY = 0,
			_IN_DELETE = 1
		} MonType;

		void monitorNewLine(std::string inotifyTarget, INotifyObj::Outcome *result, boost::condition_variable *condVar);
		void monitor4Rename(std::string inotifyTarget, INotifyObj::Outcome *result, boost::condition_variable *condVar);

		void addFile2inotify(const std::string &monType, std::string inotifyTarget, INotifyObj::Outcome *result, boost::condition_variable *condVar = nullptr);

		static void deleteMeAndNotifyOwner(std::multimap<std::string, std::pair<INotifyObj::Outcome *, boost::condition_variable *>>::iterator iterInotifyTargetFiles, INotifyObj::Outcome outcome);
	private:
		static std::multimap<std::string, std::pair<INotifyObj::Outcome *, boost::condition_variable *>>  m_inotifyTargets;
		static std::string                                                                                m_inotifyExitFlag;
		static boost::mutex                                                                               m_mutex;
};

////////////////////////////   COPY THIS   ^^^^^^^^^^^^^^^^^^^^^^^^^^^    TO "logscanner/INotifyObj.h"
class LogScanner
{
	public:
		static std::atomic<bool>  KEEP_LOOPING;
		std::set<std::string>                        m_inotifyTargetPaths;
		void exec();
		void intfThread();
		void inptThread();
		void monitorNewLineThread(std::string inotifyTarget);
		void monitor4RenameThread(std::string inotifyTarget);
};

#include <filesystem>
void LogScanner::monitorNewLineThread(std::string inotifyTarget)
{
	INotifyObj iNotifyObj;
	iNotifyObj.monitorNewLineThread(inotifyTarget);
}

void LogScanner::monitor4RenameThread(std::string inotifyTarget)
{
	INotifyObj iNotifyObj;
	iNotifyObj.monitor4RenameThread(inotifyTarget);
}

void LogScanner::exec()
{
	m_inotifyTargetPaths.insert("/tmp/junk_test");
	m_inotifyTargetPaths.insert("/tmp/junk_test/child");

	// We need to create this here before we ever try to use it because
	// this is a monostate class and it has to be initialized.
	INotifyObj iNotifyObj("/tmp/junk_test/run/ctrl/intfThreadExit.flag");

	// /tmp/junk_test/run/ctrl/intfThreadExit.flag
	std::filesystem::create_directories("/tmp/junk_test/run/ctrl");  // Directory for exitflag HAS to exist...
	for(auto dirPath : m_inotifyTargetPaths)
	{
		std::filesystem::create_directories(dirPath);  // Directory HAS to exist...
	}

	std::cout << "Calling confContainer.intfThread()" << std::endl;
	std::shared_ptr<boost::thread> intf(std::make_shared<boost::thread>(&LogScanner::intfThread, this));
	std::shared_ptr<boost::thread> inpt(std::make_shared<boost::thread>(&LogScanner::inptThread, this));

	intf->join();
	intf->join();

	std::cout << "Out of confContainer.intfThread()" << std::endl;
}

void LogScanner::inptThread()
{
	std::set<std::string> targetList({
		"/tmp/junk_test/child/test_target-0",
		"/tmp/junk_test/child/test_target-1",
		"/tmp/junk_test/child/test_target-2",
		"/tmp/junk_test/test_target-0",
		"/tmp/junk_test/test_target-1",
		"/tmp/junk_test/test_target-2"});

	std::vector<std::shared_ptr<boost::thread>> allThreads;
	int Top = 1;
	for(auto inotifyTarget : targetList)
	{
		// Test multiple monitors for same file.
		for(int cnt = 0; cnt < Top; cnt++)
		{
			allThreads.push_back(std::make_shared<boost::thread>(&LogScanner::monitorNewLineThread, this, inotifyTarget));
			allThreads.push_back(std::make_shared<boost::thread>(&LogScanner::monitor4RenameThread, this, inotifyTarget));
		}
		Top++;
	}
std::cout << "THREADS LAUNCHED in inptThread" << std::endl;
	for(auto thread : allThreads)
	{
		thread->join();
	}
}

////////////////////////////   COPY THIS   vvvvvvvvvvvvvvvvvvvvvvvvvvv    TO "logscanner/LogScanner.cpp"
void LogScanner::intfThread()
{
	INotifyObj iNotifyObj;
	iNotifyObj.intfThread(boost::ref(m_inotifyTargetPaths));
}
////////////////////////////   COPY THIS   ^^^^^^^^^^^^^^^^^^^^^^^^^^^    TO "logscanner/LogScanner.cpp"

////////////////////////////   COPY THIS   vvvvvvvvvvvvvvvvvvvvvvvvvvv    TO "logscanner/INotifyObj.cpp"
	/////////////////////////////////////////////////////////////////////////
	// From: inotify(7) — Linux manual page                                //
	// Inotify reports only events that a user-space program triggers      //
	// through the filesystem API.  As a result, it does not catch remote  //
	// events that occur on network filesystems.  (Applications must fall  //
	// back to polling the filesystem to catch such events.)               //
	/////////////////////////////////////////////////////////////////////////

	// This means that this version of INotifyObj can only work with local files.
std::multimap<std::string, std::pair<INotifyObj::Outcome *, boost::condition_variable *>>  INotifyObj::m_inotifyTargets;
boost::mutex                                                                               INotifyObj::m_mutex;
std::string                                                                                INotifyObj::m_inotifyExitFlag;

INotifyObj::INotifyObj(std::string inotifyExitFlag)
{
	// Because constructor can be called mulriple times...
	if(m_inotifyExitFlag.empty())
	{
		m_inotifyExitFlag = inotifyExitFlag;

		std::ofstream  ofInotifyExitFlag(m_inotifyExitFlag.data(), std::ios_base::trunc);
		ofInotifyExitFlag.close();
		monitor4Rename(m_inotifyExitFlag, nullptr, nullptr);
	}
}

INotifyObj::~INotifyObj()
{
}

std::string INotifyObj::getIinotifyExitFlag()
{
	return m_inotifyExitFlag;
}

void INotifyObj::intfThread(std::set<std::string> &inotifyTargetPaths)
{
	// This function works as a single thread and monitors directories.
	// Files are monitored by adding the full path to m_inotifyTargets via the monitorNewLine and monitor4Rename functions.
	// The same file may have several monitors.
	// MonType accepts 2 values: _IN_MODIFY and _IN_DELETE.
	// _IN_MODIFY covers the case to monitor lines being added.
	// _IN_DELETE covers the case to monitor the file renamed.
	// When a file is renamed we need to release it's _IN_MODIFY monitor.
	// In the case of a LUMP file we need to release all the components too.
	// There is also a special case file that is created at program startup and monitored for _IN_DELETE.
	// When the program needs to exit LogScanner::KEEP_LOOPING is set to false and the file is delete to release the read'
	consoleMsg("==> STARTING INotifyObj::intfThread");

	const static std::string STR_IN_MODIFY(std::to_string(_IN_MODIFY));
	const static std::string STR_IN_DELETE(std::to_string(_IN_DELETE));

	int EVENT_SIZE = ( sizeof (struct inotify_event) );
	int BUF_LEN    = ( 1024 * ( EVENT_SIZE + 16 ) );

	int fd;
	char buffer[BUF_LEN];

	fd = inotify_init();
	if(fd < 0 )
	{
		//  Ugly, we are dead...   :(
		consoleMsg(("===> FATAL in intfThread =>" + std::string(strerror(errno))).c_str());
		throw;
	}

	// Is the path to the flag in the list of directoris to monitor?
	if(inotifyTargetPaths.find(std::filesystem::path(m_inotifyExitFlag).parent_path().string()) == inotifyTargetPaths.end())
	{
		std::string flagPath(std::filesystem::path(m_inotifyExitFlag).parent_path().string());
		consoleMsg(("INotifyObj::intfThread adding watch for EXIT_FLAG " + flagPath).c_str());

		inotifyTargetPaths.insert(flagPath);
	}

	std::map<int, std::set<std::string>::iterator> wd_map;
	for(std::set<std::string>::iterator pathName = inotifyTargetPaths.begin(); pathName != inotifyTargetPaths.end(); pathName++)
	{
		consoleMsg(("INotifyObj::intfThread adding watch for  " + *pathName).c_str());

		int wd = inotify_add_watch(fd, pathName->data(), IN_MOVED_FROM | IN_DELETE | IN_MODIFY);
		wd_map[wd] = pathName;
	}

	while(LogScanner::KEEP_LOOPING.load())
	{
std::cout << "Going to read." << std::endl;
		int readSize = read(fd, buffer, BUF_LEN );
std::cout << "Something happened..." << std::endl;
		if (readSize == -1)
		{
			if (errno == EINTR)
			{
				consoleMsg("===> intfThread: read interrupted by signal, exiting...");
			}
			else
			{
				consoleMsg(("===> Read error in intfThread =>" + std::string(strerror(errno))).c_str());
			}
		}
		else
		{
//std::cout << "Processing all notifications in buffer. => readSize = " << readSize  << std::endl;
			// Process all notifications in buffer.
			int Idx = 0;
			while(Idx < readSize)
			{
				struct inotify_event *event = ( struct inotify_event * ) &buffer[Idx];
//std::cout << "I don't how this could fail but the man page checks it..." << std::endl;
				// I don't how this could fail but the man page checks it...
				if(event->len)
				{
//std::cout << "If it is not a directory." << std::endl;
					// If it is not a directory.
					if(!(event->mask & IN_ISDIR))
					{
//std::cout << "Do I care about it?" << std::endl;
						// Do I care about it?
						if(event->mask & IN_MOVED_FROM || event->mask & IN_DELETE || event->mask & IN_MODIFY)
						{
							std::string wdPath(*(wd_map.find(event->wd)->second));
//std::cout << "Am I monitoring that file for that mod?\twd = " << event->wd << " in map =>" << wdPath << std::endl;
//for(auto inotifyTarget : m_inotifyTargets) std::cout << "file in m_inotifyTargets = '" << inotifyTarget.first << "' <=" << std::endl;
//for(auto inotifyTarget : wd_map) std::cout << "file in wd_map = '" << *(inotifyTarget.second) << "' <=" << std::endl;
							// Am I monitoring that file for that mod?
							std::string key2find(wdPath + "/" + event->name);

							if(event->mask & IN_MODIFY)
							{
std::cout << "STR_IN_MODIFY => (key)'" << (STR_IN_MODIFY  + key2find) << "' <=" << std::endl;
								key2find = (STR_IN_MODIFY + key2find);
//								iterInotifyTargetFiles = m_inotifyTargets.find(STR_IN_MODIFY + key2find);
							}
							else
							{
std::cout << "STR_IN_DELETE => (key)'" << (STR_IN_DELETE  + key2find) << "' <=" << std::endl;
								key2find = (STR_IN_DELETE + key2find);
//								iterInotifyTargetFiles = m_inotifyTargets.find(STR_IN_DELETE + key2find);
							}

							std::multimap<std::string, std::pair<INotifyObj::Outcome *, boost::condition_variable *>>::iterator iterInotifyTargetFiles;
							while((iterInotifyTargetFiles = m_inotifyTargets.find(key2find)) != m_inotifyTargets.end())
							{
								deleteMeAndNotifyOwner(iterInotifyTargetFiles, EVENT_TRUE);
							}
						}
					}
				}

				Idx += EVENT_SIZE + event->len;
			}
		}
	}
std::cout << "INotifyObj::intfThread => OUT of loop <=" << std::endl;
	// We are done, free up everybody...
	while(!m_inotifyTargets.empty())
	{
		deleteMeAndNotifyOwner(m_inotifyTargets.begin(), EVENT_FALSE);
	}

	for(std::multimap<int, std::set<std::string>::iterator>::iterator wdIter = wd_map.begin(); wdIter != wd_map.end(); wdIter++)
	{
		consoleMsg(("INotifyObj::intfThread delete watch for  " + *(wdIter->second)).c_str());
		( void ) inotify_rm_watch( fd, wdIter->first );
	}

	( void ) close( fd );

	consoleMsg("==> EXITING INotifyObj::intfThread");
}

INotifyObj::Outcome INotifyObj::monitorNewLineThread(std::string inotifyTarget)
{
	INotifyObj iNotifyObj;
std::cout << "MONITORING (monitorNewLineThread) =>" << inotifyTarget << std::endl;
	boost::condition_variable condVar;
	INotifyObj::Outcome       result = EVENT_INVALID;

	iNotifyObj.monitorNewLine(inotifyTarget, &result, &condVar);

	boost::mutex                     localMutex;
	boost::unique_lock<boost::mutex> lock(localMutex);
	while(result == EVENT_INVALID)
	{
		condVar.wait(lock);
	}

	return result;
}

INotifyObj::Outcome INotifyObj::monitor4RenameThread(std::string inotifyTarget)
{
	INotifyObj iNotifyObj;
std::cout << "MONITORING (monitor4RenameThread) =>" << inotifyTarget << std::endl;
	boost::condition_variable condVar;
	INotifyObj::Outcome       result = EVENT_INVALID;

	iNotifyObj.monitor4Rename(inotifyTarget, &result, &condVar);

	boost::mutex                     localMutex;
	boost::unique_lock<boost::mutex> lock(localMutex);
	while(result == EVENT_INVALID)
	{
		condVar.wait(lock);
	}

	return result;
}

void INotifyObj::monitorNewLine(std::string inotifyTarget, INotifyObj::Outcome *result, boost::condition_variable *condVar)
{
	const static std::string STR_IN_MODIFY(std::to_string(_IN_MODIFY));
	addFile2inotify(STR_IN_MODIFY, inotifyTarget, result, condVar);
}

void INotifyObj::monitor4Rename(std::string inotifyTarget, INotifyObj::Outcome *result, boost::condition_variable *condVar)
{
	const static std::string STR_IN_DELETE(std::to_string(_IN_DELETE));
	addFile2inotify(STR_IN_DELETE, inotifyTarget, result, condVar);
}

void INotifyObj::addFile2inotify(const std::string &monType, std::string inotifyTarget, INotifyObj::Outcome *result, boost::condition_variable *condVar)
{
	m_mutex.lock();
std::cout << "**** ADDING inotify =>" << monType + inotifyTarget << "<= TO m_inotifyTargets ****" << std::endl;
	std::pair<INotifyObj::Outcome *, boost::condition_variable *> inotifyInfo{ result, condVar };
	m_inotifyTargets.insert(std::make_pair(monType + inotifyTarget, inotifyInfo));

	m_mutex.unlock();
}

void INotifyObj::deleteMeAndNotifyOwner(std::multimap<std::string, std::pair<INotifyObj::Outcome *, boost::condition_variable *>>::iterator iterInotifyTargetFiles, INotifyObj::Outcome outcome)
{
	const static std::string EXIT_FLAG = std::to_string(_IN_DELETE) + m_inotifyExitFlag;

	m_mutex.lock();

	std::string                pathName = iterInotifyTargetFiles->first;
	INotifyObj::Outcome       *result   = iterInotifyTargetFiles->second.first;
	boost::condition_variable *condVar  = iterInotifyTargetFiles->second.second;
std::cout << "IN INotifyObj::deleteMeAndNotifyOwner => '" << (iterInotifyTargetFiles->first) << "' <= condvar = " << reinterpret_cast<uintptr_t>(condVar) << std::endl;
	// Remove name from list of monitored files...
	m_inotifyTargets.erase(iterInotifyTargetFiles);

	if(pathName != EXIT_FLAG)
	{
std::cout << "**** IN deleteMeAndNotifyOwner // Notify owner." << std::endl;
		// Notify owner.
		*result = outcome;
		condVar->notify_one();
	}

	m_mutex.unlock();
}
////////////////////////////   COPY THIS   ^^^^^^^^^^^^^^^^^^^^^^^^^^^    TO "logscanner/INotifyObj.cpp"

void signalHandler(int signum)
{
std::cout << "IN signalHandler => signum = " << signum << "' <=" << std::endl;
	if(signum == SIGINT|| signum == SIGTERM)
	{
		LogScanner::KEEP_LOOPING.store(false);
std::cout << "IN signalHandler => removing = " << INotifyObj::getIinotifyExitFlag() << " <=" << std::endl;
		remove(INotifyObj::getIinotifyExitFlag().data());
	}
	else if(signum == SIGUSR1)
	{
		// Do nothing...
	}
}

int setSignalHandlers()
{
	int signals[] = { SIGINT, SIGTERM, SIGUSR1};
	int size = (int)(sizeof(signals) / sizeof(int));
	int Idx = 0;
	int res = 0;

	while(Idx < size && res == 0)
	{
		struct sigaction SIG_handler;

		SIG_handler.sa_handler = signalHandler;
		sigemptyset(&SIG_handler.sa_mask);
		SIG_handler.sa_flags = 0;

		if(sigaction(signals[Idx], &SIG_handler, NULL) == -1)
		{
			perror("sigaction");
			std::cerr << "size = " << size << std::endl;
			std::cerr << "Idx = " << Idx << std::endl;
			res = signals[Idx];
		}

		Idx++;
	}

	return res;
}

std::atomic<bool>  LogScanner::KEEP_LOOPING(true);
int main(int argc, char* argv[])
{
	if(setSignalHandlers() == 0)
	{
		LogScanner logScanner;

		std::cout << "Calling LogScanner::intfThread()" << std::endl;
		logScanner.exec();
		std::cout << "Out of LogScanner::intfThread()" << std::endl;
	}
	else
	{
		std::cerr << "Unable to set signal handler ." << std::endl;
	}
std::cout << "====> DONE <====" << std::endl;
}
/*END*/
