#!/bin/bash
# logscanner/util/see-table-VIEW.sh

echo "# CREATE USER 'logscanner_admin'@'localhost' IDENTIFIED BY 'ADMIN-PASSWORD-HERE';" >&2
echo "select * from V030_Drop_010 order by Octect_1, Octect_2, Octect_3, Octect_4;"|mysql -u logscanner_admin -p logscanner
#END#
