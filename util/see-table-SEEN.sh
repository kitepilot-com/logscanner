#!/bin/bash
# logscanner/util/see-table-SEEN.sh

echo "# CREATE USER 'logscanner_admin'@'localhost' IDENTIFIED BY 'ADMIN-PASSWORD-HERE';" >&2
echo "select * from T100_LinesSeen;"|mysql -u logscanner_admin -p logscanner
#END#
