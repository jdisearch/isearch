#!/bin/sh

cd $(dirname $0)
proname="index_write"

if [ -f $proname ] ; then
	chmod 755 $proname
	./$proname
else
	echo "no program"
fi

