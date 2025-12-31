#!/bin/bash
# logscanner/util/run-tests.sh
# See logscanner/doc/test-cases.txt
# for ADDR in $(grep -v '^[[:digit:]]' logscanner/exec-log/logscanner-output.log|grep -E '\(\+'|sed -e 's/.*(+/+/g'|sed -e 's/).*//g');do addr2line -f -C -e logscanner/logscanner ${ADDR};done|less -S

cd /home/kitepilot/

if [[ ! -x logscanner/logscanner ]];then # }
	echo -e "***************   No executable logscanner/logscanner...\nDEAD"
	exit
fi # }
TEST_ID="Case 1: The program has a fresh start on an empty logfile -> Control files SHOULD NOT exist."
echo "${TEST_ID}"
RESULT="PASS"
rsync -a --delete /home/kitepilot/tmp/apache2/ /var/log/apache2/
cat /home/kitepilot/tmp/mail.log > /var/log/mail.log
> /var/log/apache2/access-LUMP.log
> /var/log/apache2/error-LUMP.log
rm -Rf logscanner/run/ctrl/*
SIZE2CHK=$(stat /var/log/mail.log|grep Size: |awk '{print $2}')
(cd logscanner/ && ./logscanner > exec-log/logscanner-output.log 2>&1) &
sleep 1
PS_COUNT=$(ps -ef | grep ' \./logscanner' | grep -v grep | wc -l)
if [[ ${PS_COUNT} -eq 1 ]];then # {
	PS_INDEX=$(ps -ef | grep ' \./logscanner' | grep -v grep | awk '{print $2}')
	echo "logscanner executing with ps ${PS_INDEX}"

	if [[ -f logscanner/run/ctrl/var/log/mail.log/fileOrSegmentEOF.ctrl ]];then # {
		echo "=> Heartbeat 'cat /tmp/junk-heartbeat.txt (and/or 'cat logscanner/run/ctrl/var/log/mail.log/nextCatchup2Read.ctrl logscanner/run/ctrl/var/log/mail.log/fileOrSegmentEOF.ctrl')'"
		while [[ -f logscanner/run/ctrl/var/log/mail.log/fileOrSegmentEOF.ctrl ]] && [[ $(ps -ef | grep ' \./logscanner' | grep -v grep | wc -l) -gt 0 ]];do # {
			date > /tmp/junk-heartbeat.txt
			sleep 1
		done # }

		if [[ $(ps -ef | grep ' \./logscanner' | grep -v grep | wc -l) -gt 0 ]];then # {
			kill ${PS_INDEX} 2>/dev/null
			sleep 1
			if [[ $(grep 'logscanner is exiting gracefully.' logscanner/exec-log/logscanner-output.log|wc -l) -eq 1 ]];then # {
				echo "Looking for pending threads..."
				logscanner/util/get-thread-status.sh >/dev/null 2>&1
				if [[ $(grep -v FINISHED /tmp/junk-logscanner-show-threads-status.log|wc -l) -eq 0 ]];then # {
					if [[ -f logscanner/run/ctrl/var/log/mail.log/fileOrSegmentEOF.ctrl ]];then # {
						RESULT="FAIL - Control file logscanner/run/ctrl/var/log/mail.log/fileOrSegmentEOF.ctrl exists."
					fi # }
				else # } {
					RESULT="FAIL - Some threads didn't finish => /tmp/junk-logscanner-show-threads-status.log."
				fi # }
			else # } {
				RESULT="FAIL - 'logscanner is exiting gracefully.' not found in logscanner/exec-log/logscanner-output.log"
			fi # }
		else # } {
			RESULT="FAIL - Looks like the program aborted..."
		fi # }
	else # } {
		kill ${PS_INDEX} 2>/dev/null
		RESULT="FAIL - MISSING logscanner/run/ctrl/var/log/mail.log/fileOrSegmentEOF.ctrl"
	fi # }
else # } {
	killall -9 ./logscanner 2>/dev/null
	RESULT="FAIL - 'ps' doesn't return 1 => $(ps -ef | grep ' \./logscanner' | grep -v grep)"
fi # }
echo "${RESULT} => ${TEST_ID}"

echo "ALL DONE"
#*END*#
