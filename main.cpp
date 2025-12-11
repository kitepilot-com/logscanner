/*******************************************************************************
 * File: logscanner/main.cpp                                                   *
 * Created:     Jan-07-25                                                      *
 * Last update: Jan-07-25                                                      *
 * Program start.                                                              *
 *******************************************************************************/

#include "LogScanner.h"
#include <signal.h>

void signalHandler(int signum)
{
	if(signum == SIGINT)
	{
		LogScanner::KEEP_LOOPING.store(false);
	}
	else if(signum == SIGUSR1)
	{
		// Do nothing...
	}
}

int setSignalHandlers()
{
	int signals[] = {SIGINT, SIGUSR1};
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

int main(int argc, char *argv[])
{
	int JUNK_FIX_INOTIFY_FILE_LIMIT; //FIXTHIS!!!
std::cerr << "echo 2048 > /proc/sys/user/max_inotify_instances" << std::endl;
std::cout << "echo 2048 > /proc/sys/user/max_inotify_instances" << std::endl;
	int result = -1;

	if(argc == 1)
	{
		if(setSignalHandlers() == 0)
		{
			try
			{
				LogScanner logScanner;
				result = logScanner.exec();
			}
			catch(...)
			{
				// If we get here the constructor threw, all other exceptions are caught.
				std::cerr << "Unable to start...\nEnsure that no other instance is running\nExecute as 'root'" << std::endl;
			}
		}
		else
		{
			std::cerr << "Unable to set signal handler ." << std::endl;
		}
	}
	else
	{
		std::cerr << "I take no prisioners,\nRTFM..." << std::endl;
	}

	return result;
}
/*END*/
