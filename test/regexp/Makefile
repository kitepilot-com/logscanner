################################################################################
# File: testRegexp/Makefile                                                    #
# Created:     Jan-07-25                                                       #
# Last update: Jan-07-25                                                       #
################################################################################

testRegexp: main.o testRegexp.o
	g++ -g -o testregexp main.o testRegexp.o -lboost_filesystem -lboost_regex -lmariadbcpp

main.o: main.cpp testRegexp.h
	g++ -g -c -o main.o main.cpp

testRegexp.o: testRegexp.h testRegexp.cpp
	g++ -g -c -o testRegexp.o testRegexp.cpp

clean: 
	rm -vf *.o
	rm -vf testregexp

#*END*#
