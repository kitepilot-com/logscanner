#!/bin/bash
#  echo 2048 > /proc/sys/user/max_inotify_instances
# rm -Rf logscanner/run;mkdir -pv logscanner/exec-log/;ssh root@localhost 'mysql logscanner < /home/kitepilot/logscanner/sql/tables.sql';(cd logscanner/ && ./logscanner > exec-log/logscanner-output.log 2>&1 & sleep 18000);kill $(ps -ef|grep ' \./logscanner$'|grep -v grep|awk '{print $2}')
# while true;do (cd logscanner/ && ./logscanner > exec-log/logscanner-output.log 2>&1 & sleep 3600);kill $(ps -ef|grep ' \./logscanner$'|grep -v grep|awk '{print $2}');date;done
# while true;do (cd logscanner/ && ./logscanner > exec-log/logscanner-output.log 2>&1 & echo "START: $(date)");grep FATAL ./logscanner/exec-log/logscanner-output.log;sleep 3600;if [[ $(ps -ef|grep ' \./logscanner$'|grep -v grep|wc -l) -lt 1 ]];then echo "*DEAD*";else kill $(ps -ef|grep ' \./logscanner$'|grep -v grep|awk '{print $2}');echo "KILLED: $(date)";echo "Sleeping 5";fi;sleep 5;done

echo "less -S logscanner/exec-log/logscanner-output.log"
# valgrind --max-threads=4096
EXEC_CMD=" "
if [[ -z "$1" ]];then # {
	EXEC_CMD="(cd logscanner/ && ./logscanner > exec-log/logscanner-output.log 2>&1) "
else # } {
	EXEC_CMD="(cd logscanner/ && gdb ./logscanner)"
fi # }

echo "==> eval ${EXEC_CMD} <=="

# logscanner/util/create-tables.sh;
echo "# CREATE USER 'logscanner_admin'@'localhost' IDENTIFIED BY 'ADMIN-PASSWORD-HERE';" >&2
mysql -u logscanner_admin --password='ADMIN-PASSWORD-HERE' logscanner < logscanner/sql/tables.sql

mv logscanner/run tmp/JUNK_run;
rm -Rf tmp/JUNK_run &
mkdir -pv logscanner/exec-log;
echo "Starting..."
eval ${EXEC_CMD}
#8END*#
