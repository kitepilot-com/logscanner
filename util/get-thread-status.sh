#!/bin/bash
# logscanner/util/get-thread-status.sh

extract_lines_of_thread_ID() {
	local ID=$1
	local DELM=$2
	local STARTLINE=""
	local EXIT_FLAG=0
	local RSTRTFLAG=0
	local RESULT=""

	STARTLINE="$(grep -E ">[[:space:]]STARTING.*[^[:digit:]]${ID}[^[:digit:]]" logscanner/exec-log/logscanner-output.log)"
	EXIT_FLAG="$(grep -E ">[[:space:]]EXITING.*[^[:digit:]]${ID}[^[:digit:]]" logscanner/exec-log/logscanner-output.log|wc -l)"
	RSTRTFLAG="$(grep -E ">[[:space:]]RESTARTING.*[^[:digit:]]${ID}[^[:digit:]]" logscanner/exec-log/logscanner-output.log|wc -l)"

	if [[ ${RSTRTFLAG} -eq 0 ]];then # {
		if [[ ${EXIT_FLAG} -eq 0 ]];then # {
			RESULT="RUNNING"
		else # } {
			RESULT="FINISHED"
		fi # }
	else # } {
		RESULT="RESTARTED"
	fi # }

	echo -e "$(echo "${STARTLINE} "|sed -r -e 's/^[[:digit:]]{1,}[[:space:]]{1,}[^[:alnum:]]{1,}[[:space:]]{1,}STARTING[[:space:]]{1,}//g')\t${RESULT}" > /tmp/junk-logscanner-get-threads-result-${ID}.txt

	echo "grep -E '[^[:digit:]]${ID}[^[:digit:]]' logscanner/exec-log/logscanner-output.log|less -S" > /tmp/junk-logscanner-show-one-thread-${ID}.cmd
	echo -n "${DELM}[^[:digit:]]${ID}[^[:digit:]]"                                                   > /tmp/junk-logscanner-show-all-threads-${ID}.txt
	DELM="i|"

	echo -n '*'
}

LC_ALL=C
DELM=""

rm -f /tmp/junk-logscanner*

echo "Finding and sorting threads tags..."
grep '>> STARTING' logscanner/exec-log/logscanner-output.log|sed -r -e 's/^[[:digit:]]{1,}[[:space:]]{1,}([>]{1,}> STARTING).*/\1/g'|sort -u  >  /tmp/junk-logscanner-STARTING-TAGS.txt
grep '=> STARTING' logscanner/exec-log/logscanner-output.log|sed -r -e 's/^[[:digit:]]{1,}[[:space:]]{1,}([=]{1,}> STARTING).*/\1/g'|sort -ru >> /tmp/junk-logscanner-STARTING-TAGS.txt

echo "Finding threads that started..."
grep -E -f /tmp/junk-logscanner-STARTING-TAGS.txt logscanner/exec-log/logscanner-output.log|
	sed -r -e 's/^[[:digit:]]{1,}[[:space:]]{1,}//g'|
		sed -r -e 's/.*\(([[:digit:]]{1,})\).*/\1/g'|
			sort -n > /tmp/junk-logscanner-thread-ID-list.log

echo "Finding threads by ID..."
for ID in $(cat /tmp/junk-logscanner-thread-ID-list.log);do # {
	extract_lines_of_thread_ID ${ID} ${DELM} &
	DELM="|"
done # }
echo

echo -e "\nWaiting..."
wait
echo -e "\nDONE waiting..."

sort /tmp/junk-logscanner-show-one-thread-*.cmd > /tmp/junk-logscanner-show-one-thread.cmd &

echo "grep -E '$(sort /tmp/junk-logscanner-show-all-threads-*.txt|tr '\n' ' '|sed -e 's/[[:space:]]*//g')' logscanner/exec-log/logscanner-output.log" > /tmp/junk-logscanner-show-all-threads.cmd &

cat /tmp/junk-logscanner-STARTING-TAGS.txt|while read MAIN_TAG;do # {
	echo -e "\nExtracting TAG '${MAIN_TAG}' from logscanner/exec-log/logscanner-output.log."

	grep -E "[[:space:]]${MAIN_TAG}" logscanner/exec-log/logscanner-output.log|sed -r -e "s/^[[:digit:]]{1,}[[:space:]]{1,}${MAIN_TAG}//g"|sort|while read LOG_LINE;do # {
		grep -h "${LOG_LINE}" /tmp/junk-logscanner-get-threads-result-*.txt|sort >> /tmp/junk-logscanner-show-threads-status.log
		echo -n '.'
	done # }
done # }
echo

echo -e "\nWaiting again..."
wait

rm -f /tmp/junk-logscanner-show-one-thread-*.cmd f /tmp/junk-logscanner-show-all-threads-*.txt f /tmp/junk-logscanner-get-threads-result-*.txt

if [[ -f logscanner/exec-log/logscanner-output.log ]];then # {
	grep 'logscanner is exiting gracefully' logscanner/exec-log/logscanner-output.log

	if [[ $(grep 'logscanner is exiting gracefully' logscanner/exec-log/logscanner-output.log|wc -l) -ne 1 ]];then # {
		echo "***********************   Is 'logscanner is exiting gracefully' there?   ***********************"
	fi # }
else # } {
	echo "Whre is logscanner/exec-log/logscanner-output.log  ?   :("
fi # }

echo "Results at:"
ls -ld /tmp/junk-logscanner*
#*END*#
