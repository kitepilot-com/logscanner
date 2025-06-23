#!/bin/bash
# logscanner/util/see-table-AUDIT.sh

echo "# CREATE USER 'logscanner_admin'@'localhost' IDENTIFIED BY 'YOUR-PASSWORD-HERE';"
echo "select * from T020_Audit order by LogTmstmp;"|mysql -u logscanner_admin -p logscanner
#END#
