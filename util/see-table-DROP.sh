#!/bin/bash
# logscanner/util/see-table-DROP.sh

echo "# CREATE USER 'logscanner_admin'@'localhost' IDENTIFIED BY 'YOUR-PASSWORD-HERE';"
echo "select * from T030_Drop order by Octect_1, Octect_2, Octect_3, Octect_4;"|mysql -u logscanner_admin -p logscanner
#END#
