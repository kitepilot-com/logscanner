// logscanner/test/testRegexp/main.cpp

#include "testRegexp.h"

int main(int argc, char *argv[])
{
	if(argc == 3)
	{
		const char *lglines = argv[1];
		const char *regexps = argv[2];

		testRegexp(lglines, regexps);
	}
	else
	{
		std::cout << "Usaage: " << argv[0] << " <file with lines> <file with regexps>" << std::endl;
		std::cout << "(cd logscanner/test/regexp/ && ./testregexp /tmp/file_with_lines.txt /tmp/file_with_regexps.txt)" << std::endl;
	}

	return 0;
}
/*END*/
