FROM centos:centos7.2.1511

ARG invertpath=/usr/local/isearch/index_storage/inverted_index
RUN mkdir -p $invertpath && chmod +x resource/dtcd.sh
RUN mkdir $invertpath/conf $invertpath/bin $invertpath/rocksdb_data
COPY resource/index_storage/inverted_index/conf/cache.conf $invertpath/conf
COPY resource/index_storage/inverted_index/conf/table.conf $invertpath/conf
COPY resource/dtcd.sh $invertpath/bin
COPY dtcd $invertpath/bin
COPY rocksdb_helper $invertpath/bin

ARG intellipath=/usr/local/isearch/index_storage/intelligent_index
RUN mkdir -p $intellipath
RUN mkdir $intellipath/conf $intellipath/bin $intellipath/rocksdb_data
COPY resource/index_storage/intelligent_index/conf/cache.conf $intellipath/conf
COPY resource/index_storage/intelligent_index/conf/table.conf $intellipath/conf
COPY resource/dtcd.sh $intellipath/bin
COPY dtcd $intellipath/bin
COPY rocksdb_helper $intellipath/bin

ARG originalpath=/usr/local/isearch/index_storage/original_data
RUN mkdir -p $originalpath
RUN mkdir $originalpath/conf $originalpath/bin
COPY resource/index_storage/original_data/conf/cache.conf $originalpath/conf
COPY resource/index_storage/original_data/conf/table.conf $originalpath/conf
COPY resource/dtcd.sh $originalpath/bin
COPY dtcd $originalpath/bin

COPY libdtc-gcc-4.8-r4646582.so /lib64/libdtc.so.1
COPY librocksdb.so.6.6.0 /lib64/librocksdb.so.6
COPY libgflags.so.2.2 /lib64/libgflags.so.2.2
ARG writepath=/usr/local/isearch/index_write
RUN mkdir -p $writepath && mkdir $writepath/conf $writepath/bin $writepath/log $writepath/stat
COPY resource/index_write/conf $writepath/conf
COPY resource $writepath/conf
COPY index_write $writepath/bin

ARG readpath=/usr/local/isearch/index_read
RUN mkdir -p $readpath && mkdir $readpath/conf $readpath/bin $readpath/log $readpath/stat $readpath/data
COPY resource $readpath/conf
COPY resource/index_read/conf $readpath/conf
COPY resource/index_read/data $readpath/data
COPY index_read $readpath/bin

ARG agentpath=/usr/local/isearch/search_agent
RUN mkdir -p $agentpath
RUN mkdir $agentpath/conf $agentpath/log $agentpath/log/bin
COPY search_agent $agentpath/bin/search_agent
COPY resource/search_agent/conf/sa.conf $agentpath/conf/sa.conf
COPY resource/tools /usr/local/isearch/tools
COPY start.sh /root/start.sh

CMD ["/bin/bash", "/root/start.sh"]