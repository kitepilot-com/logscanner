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

mv logscanner/run/ /tmp/DELETE_THIS_JUNK 2>/dev/null
rm -Rf /tmp/DELETE_THIS_JUNK &

rsync -a --delete /home/kitepilot/tmp/apache2/*.log /tmp/var/log/apache2/
cat /home/kitepilot/tmp/mail.log > /tmp/var/log/mail.log
> /tmp/var/log/apache2/access-LUMP.log
> /tmp/var/log/apache2/error-LUMP.log

SIZE2CHK=$(stat /var/log/mail.log|grep Size: |awk '{print $2}')
(cd logscanner/ && ./logscanner > exec-log/logscanner-output.log 2>&1) &
sleep 1

SEGM_FILE=$(find logscanner/run/ctrl/|grep "mail.log/fileOrSegmentEOF.ctrl");
CTCH_FILE=$(find logscanner/run/ctrl/|grep "mail.log/nextCatchup2Read.ctrl");
PS_COUNT=$(ps -ef | grep ' \./logscanner' | grep -v grep | wc -l)
if [[ ${PS_COUNT} -eq 1 ]];then # {
	PS_INDEX=$(ps -ef | grep ' \./logscanner' | grep -v grep | awk '{print $2}')
	echo "logscanner executing with ps ${PS_INDEX}"

	if [[ -f ${SEGM_FILE} ]];then # {
		echo "=> Heartbeat 'cat /tmp/junk-heartbeat.txt (and/or 'cat ${CTCH_FILE} ${SEGM_FILE}')'"
		while [[ -f ${SEGM_FILE} ]] && [[ $(ps -ef | grep ' \./logscanner' | grep -v grep | wc -l) -gt 0 ]];do # {
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
					if [[ -f ${SEGM_FILE} ]];then # {
						RESULT="FAIL - Control file ${SEGM_FILE} exists."
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
		RESULT="FAIL - MISSING ${SEGM_FILE}"
	fi # }
else # } {
	killall -9 ./logscanner 2>/dev/null
	RESULT="FAIL - 'ps' doesn't return 1 => $(ps -ef | grep ' \./logscanner' | grep -v grep)"
fi # }
echo "${RESULT} => ${TEST_ID}"

echo "ALL DONE"
#*END*#
