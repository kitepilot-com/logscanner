// logscanner/test/date2seconds/date2seconds.cpp
// (cd logscanner/test/date2seconds/ && g++ -o date2seconds date2seconds.cpp -lboost_regex)
// logscanner/test/date2seconds/date2seconds
// Verify with: logscanner/util/secs2date.sh 1684787092;logscanner/util/secs2date.sh 1745209615

// From: https://www.geeksforgeeks.org/date-and-time-parsing-in-cpp/
// C++ Program to implement Date and Time Parsing using chrono

//	/var/log/apache2/access-rescate.com.log	20.171.207.110 - - [20/Apr/2025:20:26:55 -0400] "GET /YV1315%202.jpg HTTP/1.1" 404 66
//	      2025-04-20T20:24:25.398047-04:00 mainmail saslauthd[4429]:                 : auth failure: [user=noreply@kitepilot.net] [serv
//	echo "2025-04-20T20:24:25.398047-04:00 mainmail saslauthd[4429]:                 : auth failure: [user=noreply@kitepilot.net] [serv"|sed -r -e 's/^([[:digit:]]{4}-[[:digit:]]{2}-[[:digit:]]{2})T([[:digit:]]{2}:[[:digit:]]{2}:[[:digit:]]{2})\.[[:digit:]]{1,}-[[:digit:]].*/\1 \2/g'
//	echo '/var/log/apache2/access-rescate.com.log 20.171.207.110 - - [20/Apr/2025:20:26:55 -0400] "GET /YV1315%202.jpg HTTP/1.1" 404 66'|sed -r -e 's=.*\[([[:digit:]]{2})/([[:alpha:]]{3})/([[:digit:]]{4}):([[:digit:]]{2}:[[:digit:]]{2}:[[:digit:]]{2})[[:space:]].*=\3-\2-\1 \4=g'


#include <vector>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <boost/regex.hpp>
//using namespace std;

// function to parse a date or time string.
std::chrono::system_clock::time_point GFG(const std::string &datetimeString, const std::string &format)
{
	tm tmStruct = {};
	std::istringstream ss(datetimeString);
	ss >> std::get_time(&tmStruct, format.c_str());
	return std::chrono::system_clock::from_time_t(mktime(&tmStruct));
}

// Function to format a time_t value into a date or time string.
//	std::string DateTime(const std::chrono::system_clock::time_point& timePoint, std::string& format)
//	{
//		time_t time = std::chrono::system_clock::to_time_t(timePoint);
//		tm timeinfo;
//		localtime_r(&time, &timeinfo);
//	//std::cout << "timeinfo.tm_sec   = " << timeinfo.tm_sec   << std::endl;
//	//std::cout << "timeinfo.tm_min   = " << timeinfo.tm_min   << std::endl;
//	//std::cout << "timeinfo.tm_hour  = " << timeinfo.tm_hour  << std::endl;
//	//std::cout << "timeinfo.tm_mday  = " << timeinfo.tm_mday  << std::endl;
//	//std::cout << "timeinfo.tm_mon   = " << timeinfo.tm_mon   << std::endl;
//	//std::cout << "timeinfo.tm_year  = " << timeinfo.tm_year  << std::endl;
//	//std::cout << "timeinfo.tm_wday  = " << timeinfo.tm_wday  << std::endl;
//	//std::cout << "timeinfo.tm_yday  = " << timeinfo.tm_yday  << std::endl;
//	//std::cout << "timeinfo.tm_isdst = " << timeinfo.tm_isdst << std::endl;
//		char buffer[70];
//		strftime(buffer, sizeof(buffer), format.c_str(), &timeinfo);
//		return buffer;
//	}

std::string lineTimestamp2Seconds(const std::string &line, const std::string &regxFmt, std::string &bldMap, const std::string &format)
{
	boost::match_results<std::string::const_iterator> result;
	std::string::const_iterator                       start = line.begin(),
	                                                  end   = line.end();
	boost::regex                                      dateRx(regxFmt);
	std::string                                       datetimeString;
	char                                              str4atoi[2];
	memset(str4atoi, '\0', sizeof(str4atoi));
//std::cout << "line => " << line.substr(0, 125) << std::endl;
	if(regex_search(start, end, result, dateRx, boost::match_default))
	{
		const char *cursor = bldMap.data();
		while((cursor - bldMap.data()) < bldMap.size())
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
	std::cout << "datetimeString => " << datetimeString << std::endl;
//std::chrono::system_clock::time_point GFG(const std::string &datetimeString, const std::string &format)
//{
	tm tmStruct = {};
	std::istringstream ss(datetimeString);
	ss >> std::get_time(&tmStruct, format.c_str());
//	return std::chrono::system_clock::from_time_t(mktime(&tmStruct));
//}
//	std::cout << "return datetimeString => " << datetimeString << std::endl;
//	return "";//std::to_string(time2go);
	return  std::to_string(std::chrono::system_clock::to_time_t(std::chrono::system_clock::from_time_t(mktime(&tmStruct))));
}

//void showResult(std::chrono::system_clock::time_point parsedTime/*, std::string &formattedTime, std::string &datetimeString*/)
//{
//	std::cout << "Parsed Time---> "
//		<< std::chrono::system_clock::to_time_t(parsedTime)
//		<< std::endl;
//	std::cout << "Formatted Time---> " << formattedTime << std::endl;
//	std::cout << "INPUT Time---> " << datetimeString << std::endl << std::endl;
//}

int main()
{
{
	std::string line = "2025-04-20T23:08:13.846546-04:00 mainmail postfix/smtpd[1183601]: disconnect from 2be65cda.tidalcoinage.internet-measurement.com[167.99.234.119] commands=0/0";
	std::string regxFmt = "^([[:digit:]]{4}-[[:digit:]]{2}-[[:digit:]]{2})T([[:digit:]]{2}:[[:digit:]]{2}:[[:digit:]]{2})\\.[[:digit:]]{1,}-[[:digit:]].*";
	std::string format = "%Y-%m-%d %H:%M:%S";
	std::string bldMap("1 2");
//	std::vector<char> bldMap{1, ' ', 2};

//	std::string datetimeString = lineTimestamp2Seconds(line, regxFmt, bldMap);
	std::string datetimeString = lineTimestamp2Seconds(line, regxFmt, bldMap, format);

//	std::string datetimeString = "2025-04-20 20:24:25";
//	std::cout << datetimeString << std::endl;
//	std::chrono::system_clock::time_point parsedTime = GFG(datetimeString, format);
//	std::string formattedTime = DateTime(parsedTime, format);

	std::cout << "Parsed Time---> " << datetimeString << std::endl;
//	showResult(parsedTime/*, formattedTime, datetimeString*/);
}
std::cout << "==================== X ====================" << std::endl;
{
	std::string line = "/var/log/apache2/access-kitepilot.info.log	43.159.144.16 - - [20/Apr/2025:23:07:56 -0400] \"GET /?C=N;O=A HTTP/1.1\" 200 778 \"-\" \"Mozilla/5.0 (iPhone; CPU iPhone OS 13_2_3 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/13.0.3 Mobile/15E148 Safari/604.1\"";
	std::string regxFmt = ".*\\[([[:digit:]]{2})/([[:alpha:]]{3})/([[:digit:]]{4}):([[:digit:]]{2}:[[:digit:]]{2}:[[:digit:]]{2})[[:space:]].*";
	std::string format = "%Y-%b-%d %H:%M:%S";
	std::string bldMap("3-2-1 4");
//	std::vector<char> bldMap{3, '-', 2, '-', 1, ' ', 4};

	std::string datetimeString = lineTimestamp2Seconds(line, regxFmt, bldMap, format);

//	std::string datetimeString = "2025-Apr-20 20:26:55";
//	std::cout << datetimeString << std::endl;
//	std::chrono::system_clock::time_point parsedTime = GFG(datetimeString, format);
//	std::string formattedTime = DateTime(parsedTime, format);

	std::cout << "Parsed Time---> " << datetimeString << std::endl;
//	showResult(parsedTime/*, formattedTime, datetimeString*/);
}
	return 0;
}
/*END*/
