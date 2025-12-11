while true;do
(
echo -n "logscanner/run/ctrl/var/log/mail.log/fileOrSegmentEOF.ctrl => ";
if [[ -f logscanner/run/ctrl/var/log/mail.log/fileOrSegmentEOF.ctrl ]];then echo "'$(cat logscanner/run/ctrl/var/log/mail.log/fileOrSegmentEOF.ctrl)'";else echo "GONE";fi;
echo -n "logscanner/run/ctrl/var/log/mail.log/nextCatchup2Read.ctrl => ";
if [[ -f logscanner/run/ctrl/var/log/mail.log/nextCatchup2Read.ctrl ]];then echo "'$(cat logscanner/run/ctrl/var/log/mail.log/nextCatchup2Read.ctrl)'";else echo "GONE";fi;
echo "====  O  ====";
ls -l $(find logscanner/|grep -E 'CATCH|DBG|_NO_IP'|grep mail)
) > /tmp/JUNK_SHOW.txt;clear;cat /tmp/JUNK_SHOW.txt;echo "----  X  ----";sleep .1;done
