#!/bin/sh

: '
cd /usr/local
git clone https://gitee.com/jd-platform-opensource/isearch.git
#git clone https://coding.jd.com/content-store/intelligentsearch.git
mv isearch jdisearch
cd jdisearch
git checkout -b dev_zhulin origin/dev_zhulin
sh build.sh
'

srcdir="/usr/local/jdisearch"
dstdir="/usr/local/isearch"

mkdir -p $dstdir/index_storage/inverted_index
cd $dstdir/index_storage/inverted_index
mkdir rocksdb_data bin conf
cp $srcdir/resource/index_storage/inverted_index/conf/cache.conf conf
cp $srcdir/resource/index_storage/inverted_index/conf/table.conf conf
cp $srcdir/resource/dtcd.sh bin
cp $srcdir/src/search_local/index_storage/cache/dtcd bin
cp $srcdir/src/search_local/index_storage/rocksdb_helper/rocksdb_helper bin
cd bin
chmod +x dtcd.sh
./dtcd.sh start

mkdir -p $dstdir/index_storage/intelligent_index
cd $dstdir/index_storage/intelligent_index
mkdir rocksdb_data bin conf
cp $srcdir/resource/index_storage/intelligent_index/conf/cache.conf conf
cp $srcdir/resource/index_storage/intelligent_index/conf/table.conf conf
cp $srcdir/resource/dtcd.sh bin
cp $srcdir/src/search_local/index_storage/cache/dtcd bin
cp $srcdir/src/search_local/index_storage/rocksdb_helper/rocksdb_helper bin
cd bin
chmod +x dtcd.sh
./dtcd.sh start

mkdir -p $dstdir/index_storage/original_data
cd $dstdir/index_storage/original_data
mkdir bin conf
cp $srcdir/resource/index_storage/original_data/conf/cache.conf conf
cp $srcdir/resource/index_storage/original_data/conf/table.conf conf
cp $srcdir/resource/dtcd.sh bin
cp $srcdir/src/search_local/index_storage/cache/dtcd bin
cd bin
chmod +x dtcd.sh
./dtcd.sh start

ln -s $dstdir/src/search_local/index_storage/api/c_api_cc/libdtc-gcc-4.8-r4646582.so /lib64/libdtc.so.1
cd $dstdir
mkdir index_write index_read search_agent
cd index_write
mkdir log bin stat conf
cp $srcdir/resource/index_write/conf/{index_gen.json,index_write.conf,localCluster.json} conf
cp $srcdir/resource/{app_field_define.json,character_map.txt,msr_training.utf8,phonetic_base.txt,phonetic_map.txt,stop_words.dict,words_base.txt} conf
cp $srcdir/src/search_local/index_write/bin/index_write bin
cd bin
./index_write

cd $dstdir/index_read
mkdir log bin stat conf data
cp $srcdir/resource/{app_field_define.txt,app_field_define.json,character_map.txt,en_intelligent_match.txt,en_stem.txt,en_words_base.txt} conf
cp $srcdir/resource/{intelligent_match.txt,msr_training.utf8,phonetic_base.txt,phonetic_map.txt,stop_words.dict,suggest_base.txt,words_base.txt} conf
cp $srcdir/resource/index_read/conf/{index_read.conf,cache.conf,table.conf} conf
cp $srcdir/resource/index_read/data/{analyze_data,relate_data,sensitive_data,synonym_data} data
cp $srcdir/src/search_local/index_read/bin/index_read bin
cd bin
./index_read

cd $dstdir/search_agent
mkdir log bin conf
cp $srcdir/resource/search_agent/conf/sa.conf conf
cp $srcdir/src/search_agent/bin/search_agent bin/
cd bin
./search_agent -d -c ../conf/sa.conf -v 3

yum install -y jq
cd /usr/local/isearch
mkdir tools
cp $srcdir/resource/tools/* tools
cd tools
sh load_data.sh
sh query.sh

