#!/bin/bash
#" "logscanner/test/inotify/inotifyThread.sh

BASE_PATH=/tmp/junk_test
PATH_NAME=child
FILE_NAME=test_target
TRASHPATH=${BASE_PATH}/TRASH
FLAG_PATH=${BASE_PATH}/run/ctrl
MAX_DEPTH=3

rm -Rf ${BASE_PATH}
mkdir -pv ${BASE_PATH}/${PATH_NAME}

for TEST_DIR in $(find ${BASE_PATH}/);do # {
	IDX=0

	while [[ ${IDX} -lt ${MAX_DEPTH} ]];do # {
		touch ${TEST_DIR}/${FILE_NAME}-${IDX}
		IDX=$(( ${IDX} + 1 ))
	done # }
done # }
mkdir -pv ${TRASHPATH}
mkdir -pv ${FLAG_PATH}

clear;(cd logscanner/test/inotify/ && ./inotifyThread &)
MY_PID="$(ps -ef|grep '\./inotifyThread'|grep -v grep|awk '{print $2}')"
sleep 1

if [[ -n "${MY_PID}" ]];then # {
	# Next loop adds lines to, rename and create files
	IDX=0
	while [[ ${IDX} -lt ${MAX_DEPTH} ]];do # {
		echo "Adding line to ${TEST_DIR}/${FILE_NAME}-$(( ${IDX} + 1 ))"
		echo "X" >> ${TEST_DIR}/${FILE_NAME}-$(( ${IDX} + 1 )) & mv -v ${TEST_DIR}/${FILE_NAME}-${IDX} ${TRASHPATH} &
		IDX=$(( ${IDX} + 2 ))

		sleep 1
		wait
	done # }

	sleep 1
	echo "Killing program..."
	kill ${MY_PID}

	sleep 1
	echo "DONE and DONE..."
else # } {
	echo "No PID to test..."
fi # }
#*END*#
