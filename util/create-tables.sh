#!/bin/bash
# logscanner/util/create-tables.sh

echo "# CREATE USER 'logscanner_admin'@'localhost' IDENTIFIED BY 'ADMIN-PASSWORD-HERE';" >&2
mysql -u logscanner_admin -p logscanner < logscanner/sql/tables.sql
#END#
