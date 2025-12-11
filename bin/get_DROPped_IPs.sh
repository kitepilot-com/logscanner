#!/bin/bash
# logscanner/bin/get_DROPped_IPs.sh

EXEC_PATH=$(pwd)

PATH="" # Everything full path...

if [[ "${EXEC_PATH}" == "/" || $(/usr/bin/grep '/rm[[:space:]]' "$0"|/usr/bin/grep -v OPTIONS|/usr/bin/wc -l) -gt 0 ]];then # {
	echo "I DO NOT execute at filesystem's root (/) OR execute .../rm" >&2
	exit
fi # }

WORK_AREA=${EXEC_PATH}/run
TEMP_PATH=${WORK_AREA}/tmp

/usr/bin/mkdir -pv ${TEMP_PATH}
/usr/bin/touch ${TEMP_PATH}/dropped-IPs.txt

/usr/sbin/iptables-save |
	/usr/bin/grep -- '-A INPUT -s .* -j DROP' |
		/usr/bin/awk '{print $4}'|
			/usr/bin/cut -f1 -d/ > ${TEMP_PATH}/dropped-IPs.txt
#*END*#
