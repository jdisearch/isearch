#!/bin/bash

#BIN_NAME="TransactionPool"
#BIN_NAME="AtomicTester"
#BIN_NAME="ConcurrentTransPoolTester"
BIN_NAME="MutexCondTester"

PROCESS_ID=`pgrep ${BIN_NAME}`
if [ -n "$PROCESS_ID" ];then
  kill -9 $PROCESS_ID
  echo "$PROCESS_ID exit"
fi

rm -rf ./core.* ./log ./$BIN_NAME
g++ -g ${BIN_NAME}".cpp" -I../ -I../../../../common -L../../../../common \
  -L../TransPool -Bstatic -lcommon -lTransPool -lpthread -o $BIN_NAME 

<<!
#test concurrent transaction executor case!
rm -rf ../TransPool ./core.* ./log ./$BIN_NAME

cd ..
make -f ./Makefile
cd ./Demo

g++ -g ConcurTransPoolTester.cpp -I../ -I../../../../common -L../../../../common \
  -L../TransPool -Bstatic -lcommon -lTransPool -lpthread -o $BIN_NAME 
!

<<!
#test transaction executor case!
rm -rf ../TransPool ./core.* ./log ./TransactionPool

cd ..
make -f ./Makefile
cd ./Demo

g++ -g TransPoolTester.cpp -I../ -I../../../../common -L../../../../common \
  -L../TransPool -Bstatic -lcommon -lTransPool -lpthread -o TransactionPool
!

<<!
# test atomic class
rm -rf ./core.* ./log ./${BIN_NAME}
g++ -g AtomicTester.cpp -I.. -I../../../../common -L../../../../common \
  -L../TransPool -Bstatic -lTransPool -lcommon -lpthread  -o ${BIN_NAME}
!
