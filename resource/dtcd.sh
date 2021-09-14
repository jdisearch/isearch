#!/bin/sh

ulimit -c unlimited

session_token=`pwd | awk -F'/' '{print $5}'`
session_port=`pwd | awk -F'/' '{print $6}'`
DTC_BIN="dtcd_${session_token}_${session_port}"
rm -f "$DTC_BIN"
ln -s dtcd "$DTC_BIN" 

DisableDataSource=`cat ../conf/cache.conf | grep DisableDataSource | awk -F"=" '{print $2}'`
db_machine_num=`cat ../conf/table.conf | grep MachineNum | awk -F"=" '{print $2}'`


if [ "$1" = "stop" ] ; then
    killall $DTC_BIN
elif [ "$1" = "restart" ]; then
    killall $DTC_BIN
    sleep 3
    ./$DTC_BIN  >> /dev/null 2>&1
elif [ "$1" = "start" ]; then
    ./$DTC_BIN  >> /dev/null 2>&1
    sleep 1
else
    echo "usage: $0 stop | start |restart"
fi
