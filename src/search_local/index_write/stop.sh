#!/bin/sh

root=$(cd $(dirname $0); pwd)

pid_file=$root/index_write.pid

if [ -f $pid_file ] ; then
	pid=`cat $pid_file`
	kill $pid
	/bin/rm -f $pid_file
else
	echo "No pid file."
fi

