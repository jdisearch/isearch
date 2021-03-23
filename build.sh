#!/bin/sh
# install - install search_local

: '
由于检索存储层依赖rocksdb，所以编译时需要满足以下前置配置：
1）centos版本需要升级到centos7及以上版本
2）gcc版本需要支持c++11特性，因此需要安装4.8以上的版本
3）Cmake版本需要大于等于3.6.2
4）安装gflags：google开源的一套命令行参数解析工具，支持从环境变量和配置文件读取参数
   安装命令：
   git clone https://github.com/gflags/gflags.git
   cd gflags
   git checkout -b 2.2 v2.2.2
   cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_SHARED_LIBS=ON -DGFLAGS_NAMESPACE=google -G "Unix Makefiles" .
   make && sudo make install
   sudo ldconfig
   sudo ln -s /usr/local/lib/libgflags.so.2.2 /lib64
   安装后，需要将gflags的包含路径添加到你的CPATH环境变量中
5）安装rocksdb依赖库：zlib，bzip2，lz4，snappy，zstandard
   sudo yum install -y snappy snappy-devel zlib zlib-devel bzip2 bzip2-devel lz4-devel libasan openssl-devel
'

localdir=`pwd`
srcdir="$localdir/src"

common="comm"
stat="stat"
index_write="index_write"
index_read="index_read"
index_storage="index_storage"
search_local="search_local"
rocksdb_lib="3rdlib/rocksdb/lib"

src_common="$srcdir/$common"
src_stat="$srcdir/$common/$stat"
src_index_write="$srcdir/$search_local/$index_write"
src_index_read="$srcdir/$search_local/$index_read"
src_index_storage="$srcdir/$search_local/$index_storage"
src_rocksdb_lib="$srcdir/$rocksdb_lib"

cd $src_rocksdb_lib
rm librocksdb.so librocksdb.so.6
ln -s librocksdb.so.6.6.0 librocksdb.so.6
ln -s librocksdb.so.6 librocksdb.so

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
