#!/bin/bash

set -e

#初始化
INIT(){
	cd /usr/local/isearch/search_agent/bin
	./search_agent -d -c ../conf/sa.conf -v 3
}

#守护脚本
PROCESS_DAEMON(){
	while :
	do
		sleep 5
	done
}

/usr/sbin/sshd -D &
INIT
PROCESS_DAEMON