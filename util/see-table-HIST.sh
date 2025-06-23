#!/bin/bash
# logscanner/util/see-table-HIST.sh

echo "# CREATE USER 'logscanner_admin'@'localhost' IDENTIFIED BY 'YOUR-PASSWORD-HERE';"
echo "select * from T040_Hist order by Timestamp;"|mysql -u logscanner_admin -p logscanner
#END#
