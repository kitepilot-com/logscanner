/*******************************************************************************
 * File: logscanner/INotifyObj.cpp                                             *
 * Created:     Dec-17-25                                                      *
 * Last update: Dec-17-25                                                      *
 * Program start.                                                              *
 *******************************************************************************/
/////////////////////////////////////////////////////////////////////////
// From: inotify(7) — Linux manual page                                //
// Inotify reports only events that a user-space program triggers      //
// through the filesystem API.  As a result, it does not catch remote  //
// events that occur on network filesystems.  (Applications must fall  //
// back to polling the filesystem to catch such events.)               //
/////////////////////////////////////////////////////////////////////////

// This means that this version of INotifyObj can only work with local files.

#include <sys/inotify.h>

#include "INotifyObj.h"
#include "LogManager.h"
#include "LogScanner.h"

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
	LogManager::getInstance()->consoleMsg("==> STARTING INotifyObj::intfThread");

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
		LogManager::getInstance()->consoleMsg(("===> FATAL in intfThread =>" + std::string(strerror(errno))).c_str());
		throw;
	}

	// Is the path to the flag in the list of directoris to monitor?
	if(inotifyTargetPaths.find(std::filesystem::path(m_inotifyExitFlag).parent_path().string()) == inotifyTargetPaths.end())
	{
		std::string flagPath(std::filesystem::path(m_inotifyExitFlag).parent_path().string());
		LogManager::getInstance()->consoleMsg(("INotifyObj::intfThread adding watch for EXIT_FLAG " + flagPath).c_str());

		inotifyTargetPaths.insert(flagPath);
	}

	std::map<int, std::set<std::string>::iterator> wd_map;
	for(std::set<std::string>::iterator dir2monitor = inotifyTargetPaths.begin(); dir2monitor != inotifyTargetPaths.end(); dir2monitor++)
	{
		LogManager::getInstance()->consoleMsg(("INotifyObj::intfThread adding watch for  " + *dir2monitor).c_str());

		int wd = inotify_add_watch(fd, dir2monitor->data(), IN_MOVED_FROM | IN_DELETE | IN_MODIFY);
		wd_map[wd] = dir2monitor;
	}

	while(LogScanner::KEEP_LOOPING.load())
	{
		int readSize = read(fd, buffer, BUF_LEN );

		if (readSize == -1)
		{
			if (errno == EINTR)
			{
				LogManager::getInstance()->consoleMsg("===> intfThread: read interrupted by signal, exiting...");
			}
			else
			{
				LogManager::getInstance()->consoleMsg(("===> Read error in intfThread =>" + std::string(strerror(errno))).c_str());
			}
		}
		else
		{
			// Process all notifications in buffer.
			int Idx = 0;
			while(Idx < readSize)
			{
				struct inotify_event *event = ( struct inotify_event * ) &buffer[Idx];

				// I don't see how this could fail but the man page checks it...
				if(event->len)
				{
					// If it is not a directory.
					if(!(event->mask & IN_ISDIR))
					{
						// Do I care about it?
						if(event->mask & IN_MOVED_FROM || event->mask & IN_DELETE || event->mask & IN_MODIFY)
						{
							std::string wdPath(*(wd_map.find(event->wd)->second));

							// Am I monitoring that file for that mod?
							std::string key2find(wdPath + "/" + event->name);

							if(event->mask & IN_MODIFY)
							{
								key2find = (STR_IN_MODIFY + key2find);
							}
							else
							{
								key2find = (STR_IN_DELETE + key2find);
							}

							m_mutex.lock();
							std::multimap<std::string, std::pair<INotifyObj::Outcome *, boost::condition_variable *>>::iterator iterInotifyTargetFiles;
							while((iterInotifyTargetFiles = m_inotifyTargets.find(key2find)) != m_inotifyTargets.end())
							{
								deleteMeAndNotifyOwner(iterInotifyTargetFiles, EVENT_TRUE);
							}
							m_mutex.unlock();
						}
					}
				}

				Idx += EVENT_SIZE + event->len;
			}
		}
	}

	// We are done, free up everybody...
	for(std::multimap<int, std::set<std::string>::iterator>::iterator wdIter = wd_map.begin(); wdIter != wd_map.end(); wdIter++)
	{
		LogManager::getInstance()->consoleMsg(("INotifyObj::intfThread delete watch for  " + *(wdIter->second)).c_str());
		( void ) inotify_rm_watch( fd, wdIter->first );
	}
	( void ) close( fd );

	m_mutex.lock();
	while(!m_inotifyTargets.empty())
	{
		deleteMeAndNotifyOwner(m_inotifyTargets.begin(), EVENT_FALSE);
	}
	m_mutex.unlock();

	LogManager::getInstance()->consoleMsg("==> EXITING INotifyObj::intfThread");
}

INotifyObj::Outcome INotifyObj::monitorNewLineThread(std::string inotifyTarget)
{
	INotifyObj iNotifyObj;

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

void INotifyObj::releaseMonitorNewLineThread(std::string inotifyTarget)
{
	const static std::string STR_IN_MODIFY(std::to_string(_IN_MODIFY));
	releaseAllMonitorsFor(STR_IN_MODIFY + inotifyTarget);
}

void INotifyObj::releaseMonitor4RenameThread(std::string inotifyTarget)
{
	const static std::string STR_IN_DELETE(std::to_string(_IN_DELETE));
	releaseAllMonitorsFor(STR_IN_DELETE + inotifyTarget);
}

void INotifyObj::releaseAllMonitorsFor(std::string key2find)
{
	std::multimap<std::string, std::pair<INotifyObj::Outcome *, boost::condition_variable *>>::iterator iterInotifyTargetFiles;

	m_mutex.lock();
	while((iterInotifyTargetFiles = m_inotifyTargets.find(key2find)) != m_inotifyTargets.end())
	{
		deleteMeAndNotifyOwner(iterInotifyTargetFiles, EVENT_FALSE);
	}
	m_mutex.unlock();
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

	std::pair<INotifyObj::Outcome *, boost::condition_variable *> inotifyInfo{ result, condVar };
	m_inotifyTargets.insert(std::make_pair(monType + inotifyTarget, inotifyInfo));

	m_mutex.unlock();
}

void INotifyObj::deleteMeAndNotifyOwner(std::map<std::string, std::pair<INotifyObj::Outcome *, boost::condition_variable *>>::iterator iterInotifyTargetFiles, INotifyObj::Outcome outcome)
{
	const static std::string EXIT_FLAG = std::to_string(_IN_DELETE) + m_inotifyExitFlag;

	std::string                key2find = iterInotifyTargetFiles->first;
	INotifyObj::Outcome       *result   = iterInotifyTargetFiles->second.first;
	boost::condition_variable *condVar  = iterInotifyTargetFiles->second.second;

	// Remove name from list of monitored files...
	m_inotifyTargets.erase(iterInotifyTargetFiles);

	if(key2find != EXIT_FLAG)
	{
		// Notify owner.
		*result = outcome;
		condVar->notify_one();
	}
}
/*END*/
