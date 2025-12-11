################################################################################
# File: logscanner/doc/README.txt                                              #
# Created:     Jan-07-25                                                       #
# Last update: Jun-23-25                                                       #
################################################################################


OVERVIEW
========
The purpose of this program is to read logfiles and identify IP addresses threatening a server (like bogus mail/ssh/http connections).
Those addresses are then DROP(ped) via iptables.
The program is just a glorified "tail -f" implementation with an added feature.
The "glorified" part is that it can "tail -f" multiple logfiles in parallel.
The added feature is that each line is tested against 1 (or more) custom-designed regular expressions to identify threats.
Configuration files are used to enumerate the input logfiles and describe the regular expressions to test the lines.
The programs is written in standard C++


CONFIGURATION FILES
===================
Configuration files can describe one logfile or a group of logfiles (LUMPing them all together).
Each configuration file is named after the log file to be inspected.
For example:
./config/var/log/mail.log.conf translates to /var/log/mail.log.

Or a LUMP configuration:
./config/var/log/apache2/access-LUMP.log.conf"
The "LUMP" flag will do the quivalent of a "ls /var/log/apache2/access-*.log" and then "tail -f" all those files into "/var/log/apache2/access-LUMP.log".
Then "/var/log/apache2/access-LUMP.log" is read by the program as a single logfile.

There are 4 type of lines in the configuration file:

One (and only) of the next 3 lines:
.- R => Regexp to extract timestamp of logline.
.- M => A map to convert timestamp in logfile to thr format below.
.- F => A format to hand to std::get_time to get seconds from epoch.
These lines are used to extract the timestamp of the line from the logfile and translate it into seconds from the epoch.  The resulting timestamp is used in the "eval thread"

Multiple lines as:
.- R => Regexp to select bad boys.

Each regexp line contains:
    1.- A "severity-val" flag (0/1).
    2.- A blank separator.
    3.- One regular expression to match offending log lines.

For a more detailed explanation about configuration files (until I decide to expand this) see the ".conf" files in the "config" directory.

Empty lines and lines starting with '#' are ignored.


EVALUATION CRITERIA
===================
The severity index of the regexp is used as follows:
Severity == 0 implies write "iptables rule to DROP it NOW".
Severity == 1 implies "if seen many times in so long", DROP it.

For details on "Severity == 1" cases search m_MAX_REQUEST_X_SEC in logscanner/LogScanner.cpp.

Lines caught by several regexp or not caught at all are inserted in Audit.
Lines caught by regexps with "Severity_val == 0" are inserted in Hist and in Drop.
Lines caught by regexps with "Severity_val == 1" are inserted in Eval.

The Eval thread will always be a "work in progress".

There is a test-regexp utility to validate regular expressions to be used in config files.


ARCHITECTURE
============
The main program spawns 3 main threads at startup:
The Input Thread, the Evaluation Thread and the Drop Thread.

There is also a comm thread that opens a listening socket (abstract socket) with the fundamental purpose of impeding that two instances of the program be run simultaneously.  It’s called the "comm server" because it may be used in the future to interact with the program.

a.- The Input Thread:
---------------------
This thread starts as many "reading threads" as there are configuration files.
Each "reading thread" is a "glorified tail -f" implementation for a single log file.  It "tail(s) -f" a single logfile and spawns a pool of standby "processing threads" (like Apache does).
Each "processing thread" creates a "regexp eval thread" for every regexp to be evaluated.

Each line read by the Input Thread is handed to any available standby "processing thread".
The "processing thread" hands the line to all "regexp eval threads" and waits for the result of the regexp evaluation.

b.- The Evaluation Thread:
--------------------------
This thread determines which addresses to block and inserts them in the DROP table:

c.- The Drop Thread:
--------------------
The drop is done in 4 steps:
1.- Status is replaced with 'P' for rows with Status == 'N' in the "DROP table".
2.- iptables rules to DROP the address are written to a file for all rows with Status 'P a file for'
3.- Status is set to 'D' for all rows with Status == 'P'.
4.- Finally newly created file is iotables-restore(ed).


CRON JOBS:
==========
Every day a cron job will remove (and copy to a history table) rows older than 24 hours from the DROP table and remove iptables rules for blocked addresses.
The "Audit table" will grow until it is inspected manually


SQL
===
See logscanner/sql/*.sql
#*END*#


TO-DO
=====
Unblock VPN (VPN port NOT bloqued by iptables)
Monitor white list file to unblock new addresses.
