################################################################################
# File: LogScanner/Makefile                                                    #
# Created:     Jan-07-25                                                       #
# Last update: Jan-07-25                                                       #
################################################################################

logscanner: main.o LogManager.o Workarea.o ConfContainer.o LumpContainer.o LogScanner.o
	g++ -g -Wall -o logscanner main.o LogManager.o Workarea.o ConfContainer.o LumpContainer.o LogScanner.o -lboost_thread -lboost_chrono -lboost_filesystem -lboost_regex -lmariadbcpp

LogManager.o: LogManager.h LogManager.cpp
	g++ -g -Wall -c -o LogManager.o LogManager.cpp

Workarea.o: LogManager.h Workarea.h Workarea.cpp
	g++ -g -Wall -c -o Workarea.o Workarea.cpp

ConfContainer.o: LogManager.h ConfContainer.h ConfContainer.cpp
	g++ -g -Wall -c -o ConfContainer.o ConfContainer.cpp

LumpContainer.o: LogManager.h ConfContainer.h LumpContainer.h LumpContainer.cpp
	g++ -g -Wall -c -o LumpContainer.o LumpContainer.cpp

LogScanner.o: LogManager.h LogManager.h Workarea.h ConfContainer.h LogScanner.h LogScanner.cpp
	g++ -g -Wall -c -o LogScanner.o LogScanner.cpp

main.o: LogScanner.h main.cpp
	g++ -g -Wall -c -o main.o main.cpp

clean: 
	rm -vf *.o
	rm -vf logscanner 
#*END*#
