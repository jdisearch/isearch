#!/bin/sh
# clean - clean search_local

localdir=`pwd`
srcdir="$localdir/src"

common="comm"
stat="stat"
index_write="index_write"
index_read="index_read"
index_storage="index_storage"
search_local="search_local"
search_agent="search_agent"

src_common="$srcdir/$common"
src_stat="$srcdir/$common/$stat"
src_index_write="$srcdir/$search_local/$index_write"
src_index_read="$srcdir/$search_local/$index_read"
src_search_agent="$srcdir/$search_agent"
src_index_storage="$srcdir/$search_local/$index_storage"

cd $src_common
rm CMakeCache.txt Makefile cmake_install.cmake libcommon.a
rm -rf CMakeFiles/

cd $src_stat
rm CMakeCache.txt Makefile cmake_install.cmake libstat.a stattool
rm -rf CMakeFiles/

cd $src_index_write
rm CMakeCache.txt Makefile cmake_install.cmake
rm -rf CMakeFiles/
rm -rf bin/

cd $src_index_read
rm CMakeCache.txt Makefile cmake_install.cmake
rm -rf CMakeFiles/
rm -rf bin/

cd $src_search_agent
rm CMakeCache.txt Makefile cmake_install.cmake
rm -rf CMakeFiles/
rm -rf bin/

cd $src_index_storage
make clean
