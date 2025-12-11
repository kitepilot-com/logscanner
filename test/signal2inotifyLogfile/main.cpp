/*******************************************************************************
 * File: logscanner/main.cpp                                                   *
 * Created:     Jan-07-25                                                      *
 * Last update: Jan-07-25                                                      *
 * Program start.                                                              *
 *******************************************************************************/

#include <iostream>

#include "LogScanner.h"

int main(int argc, char *argv[])
{
	int result = -1;

	if(argc == 1)
	{
		try
		{
//			LogManager::init();
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
		std::cerr << "I take no prisioners,\nRTFM..." << std::endl;
	}

	return result;
}
/*END*/
