/*******************************************************************************
 * File: logscanner/main.cpp                                                   *
 * Created:     Jan-07-25                                                      *
 * Last update: Jan-07-25                                                      *
 * Program start.                                                              *
 *******************************************************************************/

#include "LogScanner.h"
#include "INotifyObj.h"
#include "LogManager.h"
#include <signal.h>
#include <execinfo.h> // Required for backtrace functions

#include <iostream>

#define STACK_DEPTH_LIMIT 50

void signalHandler(int signum, siginfo_t *info, void *context)
{
	// (gdb) handle SIGINT pass nostop print
	// (gdb) signal SIGINT
	LogManager::getInstance()->consoleMsg(("Signal received => " + std::to_string((int)signum)).data());

	if(signum == SIGINT|| signum == SIGTERM)
	{
		LogScanner::KEEP_LOOPING.store(false);
		remove(INotifyObj::getIinotifyExitFlag().data());
		LogManager::getInstance()->consoleMsg(("Removed => " + INotifyObj::getIinotifyExitFlag()).data());
	}
	else if(signum == SIGSEGV)
	{
		void* callstack[STACK_DEPTH_LIMIT];
		int frames;

		// Use async-signal-safe functions where possible. 
		// write() is safe, printf() is not.
		const char msg1[] = "Caught SIGSEGV (Segmentation Fault).\n";
		write(STDERR_FILENO, msg1, sizeof(msg1) - 1);

		const char msg2[] = "Stack trace:\n";
		write(STDERR_FILENO, msg2, sizeof(msg2) - 1);

		// Get the stack frames (addresses)
		frames = backtrace(callstack, STACK_DEPTH_LIMIT);

		// Print the stack frames to stderr using a signal-safe function
		backtrace_symbols_fd(callstack, frames, STDERR_FILENO);

		// Print information about the faulting address
		char buf[100];
		int len = snprintf(buf, sizeof(buf), "Faulting address: 0x%lx\n", (long)info->si_addr);
		write(STDERR_FILENO, buf, len);

		// Exit the program safely. _exit() is async-signal-safe.
		_exit(EXIT_FAILURE);
	}
	else if(signum == SIGUSR1)
	{
// REMEMBER => int signals[] = { SIGSEGV, SIGINT, SIGTERM, SIGUSR1};
int JUNK_USE_SOCKET_AND_DELETE_THIS; //FIXTHIS!!!
	}
}

int setSignalHandlers()
{
	int signals[] = { SIGSEGV, SIGINT, SIGTERM, SIGUSR1};
	int size = (int)(sizeof(signals) / sizeof(int));
	int Idx = 0;
	int res = 0;

	while(Idx < size && res == 0)
	{
		struct sigaction SIG_handler;

		SIG_handler.sa_sigaction = signalHandler;
		SIG_handler.sa_flags = SA_SIGINFO; // Use sa_sigaction field, pass siginfo_t
		sigemptyset(&SIG_handler.sa_mask);

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
