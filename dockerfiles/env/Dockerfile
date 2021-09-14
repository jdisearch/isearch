FROM centos:centos7.2.1511
RUN yum -y update
RUN yum install -y wget pcre pcre-devel gcc gcc-c++ make zlib-devel sudo openssh-server vim lrzsz openssl-devel &&\
    yum install -y crypto-policies snappy-devel psmisc git epel-release jq && \
    yum clean all && \
    useradd --create-home --no-log-init --shell /bin/bash isearch && echo "isearch:isearch" | chpasswd && \
    mkdir -p /env/app &&\
    cd /env/app/ &&\
    wget https://cmake.org/files/v3.20/cmake-3.20.0-rc3.tar.gz &&\
    tar xf cmake-3.20.0-rc3.tar.gz &&\
    cd /env/app/cmake-3.20.0-rc3 &&\
    ./bootstrap &&\
    gmake &&\
    gmake install &&\
    cd /usr/bin &&\
    ln -s cmake3 cmake &&\
    cd /usr/local &&\
    git clone https://github.com/facebook/rocksdb.git &&\
    cd rocksdb &&\
    git checkout -b 6.6.0 ad528fe5ca08dafff47d79c85abbf3e1fbb21568 &&\
    make shared_lib &&\
    ln -s /usr/local/rocksdb/librocksdb.so.6.6.0  /lib64/librocksdb.so.6 &&\
    cd /usr/local &&\
    git clone https://github.com/gflags/gflags.git &&\
    cd gflags &&\
    git checkout -b 2.2 v2.2.2 &&\
    cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_SHARED_LIBS=ON -DGFLAGS_NAMESPACE=google -G "Unix Makefiles" . &&\
    make && make install &&\
    ldconfig &&\
    ln -s /usr/local/lib/libgflags.so.2.2 /lib64


