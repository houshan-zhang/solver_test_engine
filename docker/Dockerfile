# 以ubuntu20.04为基础
FROM ubuntu:20.04
# 设置工作目录
WORKDIR /root
# 从主机上拷贝文件到镜像的root目录
COPY scipoptsuite-8.0.3.tgz ./
COPY Bliss-master.zip ./
# 设置非交互模式
ENV DEBIAN_FRONTEND noninteractive
# 更新源的索引
RUN apt-get update
# 换源前准备
RUN apt-get -y install wget openssl
# 更新清华源
COPY sources.list /etc/apt/sources.list
# 重新更新源的索引
RUN apt-get update
# 安装自己需要的包
RUN apt-get -y install vim
# scip/INSTALL.md中所要求的第三方库
RUN apt-get -y install cmake g++ m4 xz-utils libgmp-dev unzip zlib1g-dev libboost-program-options-dev \
   libboost-serialization-dev libboost-regex-dev libboost-iostreams-dev libtbb-dev libreadline-dev \
   pkg-config liblapack-dev libgsl-dev flex bison libcliquer-dev gfortran file dpkg-dev libopenblas-dev rpm
# 安装 bliss
RUN unzip Bliss-master.zip
RUN cd Bliss-master && mkdir build && cd build && cmake .. && make -j 4 && make install
# 安装 scip
RUN tar -zxvf scipoptsuite-8.0.3.tgz
RUN cd scipoptsuite-8.0.3 && mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug -DNOBLKBUFMEM=off -DIPOPT=off && make -j 4 && make install
# 设置更新脚本
RUN printf '#!/bin/bash\n\ncd scipoptsuit*/build\nmake -j 4\nmake install' > scip_update.sh && chmod +x scip_update.sh
# 删除安装包
RUN rm -r scipoptsuite-8.0.3.tgz Bliss-master.zip
# 创建挂载目录
RUN mkdir instances source_code results
