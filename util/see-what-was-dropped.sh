#!/bin/bash
# logscanner/util/see-what-was-dropped.sh

rm -Rf /tmp/JUNK
mkdir -pv /tmp/JUNK

ssh root@localhost 'echo "select * from T100_LinesSeen;"|mysql logscanner'|
	grep /var/log/mail.log|
		sed -r -e 's/^.*(2025)/\1/g' > /tmp/JUNK/mail.log

ssh root@localhost 'echo "select * from T030_Drop order by Octect_1, Octect_2, Octect_3, Octect_4;"|mysql logscanner'|
	grep 'D$'|
		awk '{printf "%s\\.%s\\.%s\\.%s\n", $2, $3, $4, $5}'|
			sort -V|
				while read IP_ADDR;do # {
	grep -E "[^[:digit:]]{1,}${IP_ADDR}[^[:digit:]]{1,}" /tmp/JUNK/mail.log > /tmp/JUNK/${IP_ADDR}
	echo -e "$(cat /tmp/JUNK/${IP_ADDR}|wc -l)\t${IP_ADDR}"
done # }
#END#
