#!/bin/bash
# logscanner/util/secs2date.sh
# From: https://www.linuxquestions.org/questions/linux-newbie-8/convertion-of-seconds-to-date-and-time-in-linux-shell-script-825997/
# 1747761912000000000

echo -e "$1\t$(date -d @${1:0:10} +"%F_%T").${1: -9}"
#END#
