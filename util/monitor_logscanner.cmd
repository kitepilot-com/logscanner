#      clear;while true;do                            echo -en "\033[11H";                                           ls -l $(find logscanner/run/|grep -E 'GOT|DBG|CATCH');sleep 1;done
CNTR=0;clear;while true;do if [[ $CNTR -lt 10 ]];then echo -en "\033[11H";CNTR=$(( $CNTR + 1 ));else clear;CNTR=0;fi;ls -l $(find logscanner/run/|grep -E 'GOT|DBG|CATCH');sleep 1;done
