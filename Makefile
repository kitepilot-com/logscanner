################################################################################
# File: LogScanner/Makefile                                                    #
# Created:     Jan-07-25                                                       #
# Last update: Jan-07-25                                                       #
################################################################################

logscanner: main.o LogManager.o Workarea.o ConfContainer.o LumpContainer.o ReadFileCtrl.o ConveyorBelt.o NotifyIPC.o LogScanner.o
	g++ -g -Wall -o logscanner main.o LogManager.o Workarea.o ConfContainer.o LumpContainer.o ReadFileCtrl.o ConveyorBelt.o NotifyIPC.o LogScanner.o -lboost_thread -lboost_chrono -lboost_regex -lmariadbcpp

LogManager.o: LogManager.h LogManager.cpp
	g++ -g -Wall -c -o LogManager.o LogManager.cpp

Workarea.o: LogManager.h Workarea.h Workarea.cpp
	g++ -g -Wall -c -o Workarea.o Workarea.cpp

ConfContainer.o: LogManager.h ConfContainer.h ConfContainer.cpp
	g++ -g -Wall -c -o ConfContainer.o ConfContainer.cpp

LumpContainer.o: LogManager.h ConfContainer.h LumpContainer.h LumpContainer.cpp
	g++ -g -Wall -c -o LumpContainer.o LumpContainer.cpp

ReadFileCtrl.o: LogManager.h ReadFileCtrl.h ReadFileCtrl.cpp
	g++ -g -Wall -c -o ReadFileCtrl.o ReadFileCtrl.cpp

ConveyorBelt.o: LogManager.h ConveyorBelt.h ConveyorBelt.cpp
	g++ -g -Wall -c -o ConveyorBelt.o ConveyorBelt.cpp

NotifyIPC.o: LogManager.h NotifyIPC.h NotifyIPC.cpp
	g++ -g -Wall -c -o NotifyIPC.o NotifyIPC.cpp

LogScanner.o: LogManager.h Workarea.h ConfContainer.h LumpContainer.h ReadFileCtrl.h ConveyorBelt.h NotifyIPC.h LogScanner.h LogScanner.cpp
	g++ -g -Wall -c -o LogScanner.o LogScanner.cpp

main.o: LogScanner.h main.cpp
	g++ -g -Wall -c -o main.o main.cpp

clean: 
	rm -vf *.o
	rm -vf logscanner 
#*END*#
