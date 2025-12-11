#!/bin/bash
# logscanner/bin/install-new-rule.sh

function rm {
	local OPTIONS=$1
	shift
	local PATH2RM=$@

	if [[ $(echo "${PATH2RM}"|/usr/bin/grep '\.\.'|/usr/bin/wc -l) -gt 0 ]];then # {
		echo "NO relative paths allowed!" >&2
		exit
	fi # }

	/usr/bin/rm ${OPTIONS} ${PATH2RM}
}

EXEC_PATH=$(pwd)

if [[ "${EXEC_PATH}" == "/" || $(/usr/bin/grep '/rm[[:space:]]' "$0"|/usr/bin/grep -v OPTIONS|/usr/bin/wc -l) -gt 0 ]];then # {
	echo "I DO NOT execute at filesystem's root (/) OR execute .../rm" >&2
	exit
fi # }

# This is 'root', assume NOTHING!!!
# EVERYTHING has to referred to with full path.
PATH=""

DATA_PATH=${EXEC_PATH}/data
WORK_AREA=${EXEC_PATH}/run
RULE_PATH=${WORK_AREA}/rules
LOGS_PATH=${WORK_AREA}/logs
TEMP_PATH=${WORK_AREA}/tmp
FILE_NAME=rules_$(/usr/bin/date "+%F_%T.%N").iptables
IPTAB_LOG=${LOGS_PATH}/iptables-restore-${FILE_NAME}.log

# Lets make sure it is all new and consistent...
rm -Rf ${TEMP_PATH}
/usr/bin/mkdir ${TEMP_PATH}

# We need to write a script to move all consistent rules to a tmp directory,
# rules are created continuosly.
# See: LogScanner::dropThread()
# Search logscanner/LogScanner.cpp for:
# 'rulesFile += ".flag";' and/or 'std::system'
> ${TEMP_PATH}/get_rules.cmd
/usr/bin/find ${RULE_PATH}/ -name "*.flag"|/usr/bin/sed -e 's/\.flag$//g'|while read RULE_SET;do # {
	echo "/usr/bin/mv ${RULE_SET}* ${TEMP_PATH}" >> ${TEMP_PATH}/get_rules.cmd
done # }

# And get those rules...
source ${TEMP_PATH}/get_rules.cmd

# Create the iptables rules.
/usr/sbin/iptables-save | /usr/bin/grep -- '-A INPUT -s .* -j DROP' > ${TEMP_PATH}/iptables-save.txt
/usr/bin/cp ${DATA_PATH}/server-iptables-TOP.txt     ${TEMP_PATH}/${FILE_NAME}
/usr/bin/sort -u ${TEMP_PATH}/*.txt               >> ${TEMP_PATH}/${FILE_NAME}
/usr/bin/cat ${DATA_PATH}/server-iptables-BOT.txt >> ${TEMP_PATH}/${FILE_NAME}

# And install it...
#/usr/sbin/iptables-restore --noflush ${TEMP_PATH}/${FILE_NAME} > ${LOGS_PATH}/${FILE_NAME}.log 2>&1
/usr/sbin/iptables-restore --counters ${TEMP_PATH}/${FILE_NAME} > ${IPTAB_LOG} 2>&1

# Dump empty logfiles...
if [[ $(/usr/bin/stat ${IPTAB_LOG}|/usr/bin/grep -E '^[[:space:]]{1,}Size:'|/usr/bin/awk '{print $2}') -eq 0 ]];then # {
	rm -v ${IPTAB_LOG}
fi # }

# Save de suckers...
/usr/sbin/iptables-save > ${TEMP_PATH}/LAST-dropIPs.iptables-new
/usr/bin/mv ${TEMP_PATH}/LAST-dropIPs.iptables-new ${DATA_PATH}/LAST-dropIPs.iptables
#*END*#
