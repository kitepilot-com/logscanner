#!/bin/bash
# logscanner/util/create-tables.sh

echo "# CREATE USER 'logscanner_admin'@'localhost' IDENTIFIED BY 'YOUR-PASSWORD-HERE';"
mysql -u logscanner_admin -p logscanner < logscanner/sql/tables.sql
#END#
