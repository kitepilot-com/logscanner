// logscanner/test/testRegexp/testRegexp.cpp

#include "testRegexp.h"

#include <string>
#include <boost/regex.hpp>
#include <fstream>

// (cd logscanner/test/regexp/ && ./testregexp lines.txt rgexp.txt)
void testRegexp(const char *lglines, const char *regexps)
{
	// Loop over the lines and test them with the regexp.
 	std::ifstream  fsLglines(lglines);

 	if(fsLglines.is_open())
 	{
		std::string line;

		while(getline(fsLglines, line))
		{
			std::string test;

			std::ifstream  fsRegexps(regexps);
			if(fsRegexps.is_open())
			{
				std::cout << "Line =>"<< line << "<=" << std::endl << std::endl;

				while(getline(fsRegexps, test))
				{
					boost::regex   expr(test);
					boost::smatch  result;

					std::cout << "Regx =>"<< expr.str() << "<=" << std::endl;

					if (boost::regex_search(line, result, expr))
					{
						std::cout << "Match\tResult: '" << result[1] << "'" << std::endl;
					}
					else
					{
						std::cout << "NO MATCH..." << std::endl;
					}

					std::cout << std::endl;
				}

				fsRegexps.close();
				std::cout << "====  X  =====" << std::endl;
			}
		}
		
		fsLglines.close();
	}
}
/*END*/
