FROM centos:centos7.2.1511

ARG basepath=/usr/local/isearch/search_agent

RUN mkdir -p $basepath/bin
RUN mkdir -p $basepath/conf
RUN mkdir -p $basepath/log

COPY search_agent $basepath/bin/search_agent
COPY sa.conf $basepath/conf/sa.conf

CMD /usr/local/isearch/search_agent/bin/search_agent -d -c /usr/local/isearch/search_agent/conf/sa.conf -v 3