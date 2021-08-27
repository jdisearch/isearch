#!/bin/sh
# install - install search_local

localdir=`pwd`
srcdir="$localdir/src"

common="comm"
stat="stat"
index_write="index_write"
index_read="index_read"
index_storage="index_storage"
search_local="search_local"

src_common="$srcdir/$common"
src_stat="$srcdir/$common/$stat"
src_index_write="$srcdir/$search_local/$index_write"
src_index_read="$srcdir/$search_local/$index_read"
src_index_storage="$srcdir/$search_local/$index_storage"

cd $src_common
cmake .
make
cd $localdir

cd $src_stat
cmake .
make
cd $localdir

cd $src_index_write
cmake .
make
cd $localdir

cd $src_index_read
cmake .
make
cd $localdir

cd $src_index_storage
make