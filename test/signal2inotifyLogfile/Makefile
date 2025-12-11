################################################################################
# File: LogScanner/Makefile                                                    #
# Created:     Jan-07-25                                                       #
# Last update: Jan-07-25                                                       #
################################################################################

logscanner: main.o LogManager.o LogScanner.o
	g++ -g -Wall -o logscanner main.o LogManager.o LogScanner.o -lboost_thread -lboost_chrono -lboost_regex -lmariadbcpp

LogManager.o: LogManager.h LogManager.cpp
	g++ -g -Wall -c -o LogManager.o LogManager.cpp

LogScanner.o: LogManager.h LogScanner.h LogScanner.cpp
	g++ -g -Wall -c -o LogScanner.o LogScanner.cpp

main.o: LogScanner.h main.cpp
	g++ -g -Wall -c -o main.o main.cpp

clean: 
	rm -vf *.o
	rm -vf logscanner 
#*END*#
