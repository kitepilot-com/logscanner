// (cd logscanner/test/timestamp/ && g++ -o /tmp/timestamp-tst timestamp.cpp -lboost_chrono)
// time { /tmp/timestamp-tst > /tmp/junk.txt; }

#include <chrono>
#include <iostream>
#include <cstring>
#include <string>
#include <stdio.h>
#include <time.h>

static const int  TIMESTAMP_SIZE = 19;

static const int LOOPS = 1000000;
/*
std::string LogManager_nanosecTimestamp()
{
//  using namespace std::chrono;
//  return duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count();

    std::string timestamp(std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count()));

    return timestamp;
}

int main()
{
	std::cerr << "C++ !!" << std::endl;

	for(unsigned int Idx = 0; Idx < LOOPS; Idx++)
	{
		std::string timestamp(LogManager_nanosecTimestamp());
		std::cout << "timestamp\tlen => " << strlen(timestamp.c_str()) << "\t" << timestamp << std::endl;
	}

	return 0;
}
*/
void nanosecTimestamp(char *timestamp)
{
	struct            timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);

	//  It is *YOUR* responsibility to send a 'timestamp' big enough for this...
	snprintf(timestamp, TIMESTAMP_SIZE + 1, "%lld", (long long int)(ts.tv_sec) * (long long int)1000000000 + (long long int)(ts.tv_nsec));
}

int main()
{
	std::cerr << "C only" << std::endl;

	char timestamp[TIMESTAMP_SIZE + 1] = {};

	for(unsigned int Idx = 0; Idx < LOOPS; Idx++)
	{
		nanosecTimestamp(timestamp);
		std::cout << "timestamp\tlen => " << strlen(timestamp) << "\t" << timestamp << std::endl;
	}

	return 0;
}
/*END*/
/*
#include <iostream>
#include <stdio.h>
#include <time.h>

int main() {
	struct timespec ts;
	char timestamp[TIMESTAMP_SIZE + 1] = {};

	if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
		perror("clock_gettime");
		return 1;
	}

	printf("Epoch time in seconds: %ld\n", ts.tv_sec);
	printf("Nanoseconds: %ld\n", ts.tv_nsec);
//	snprintf(timestamp, TIMESTAMP_SIZE + 1, "%lld", (long long)ts.tv_sec * 10000000000 + ts.tv_nsec);

//	sprintf(timestamp, "%lld\n", (long long)ts.tv_sec * 10000000000 + ts.tv_nsec);
	sprintf(timestamp, "%lld", (long long)ts.tv_sec);
	sprintf(timestamp, "%lld", (long long)ts.tv_nsec);
	sprintf(timestamp, "%lld", (long long int)(ts.tv_sec) * (long long int)1000000000 + (long long int)(ts.tv_nsec));
	std::cout << "timestamp\tlen => " << strlen(timestamp) << "\t" << timestamp << std::endl;
	timestamp[19] = '\0';
	std::cout << "timestamp\tlen => " << strlen(timestamp) << "\t" << timestamp << std::endl;

	return 0;
}*/
