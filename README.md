### 项目背景与介绍

本团队开发的检索服务提供分词和建立索引功能，可自定义中文词库，可满足复杂查询需求，支持包括字段检索、字段排序、布尔搜索、范围检索、地理位置（POI、AOI）查询等功能。
优势在于：
1)    中文分词，内置各种中文支持。
2)    C++开发，轻量化，4核8G内存的机器就能支持大规模存储。
3)    支持存储节点分片数动态增加，方便扩容。
4)    日志即存储。

### 体验demo项目

体验demo项目请参考：[Quickstart.md](https://gitee.com/jd-platform-opensource/isearch/blob/master/docs/Quickstart.md)

### 架构图
![架构图](http://storage.jd.com/search-index/intelligentsearch.jpg)

### 特色优势
1.容器化管理，方便快速部署和扩容，通过dockerfile一键编译部署，快速上手  
2.支持http方式进行数据导入和查询（需部署接入层服务），若通过sdk或tcp方式访问，则只需部署索引层服务即可  
3.采用稳定高效的C++开发，高速搜索响应，架构简洁    
4.提供了丰富的功能，开发周期更短，支持包括字段检索、字段排序、布尔搜索、范围检索、地理位置（POI、AOI）查询等功能  

### 接口文档

RESTFul接口文档请参考：[API.md](https://gitee.com/jd-platform-opensource/isearch/blob/master/docs/API.md)。  
主要包含索引导入接口和索引查询两个接口。

### 性能表现

详细的压测数据请参考：[bench.md](https://gitee.com/jd-platform-opensource/isearch/blob/master/docs/bench.md)。  

### 项目配置文件
**索引写服务**

index_write的配置文件位于/usr/local/isearch/index_write/conf目录，以下为配置文件说明：

index_write.conf 采用json格式，主要定义程序日志级别、监听端口、以及对应缓存服务的端口信息等

words_base.txt 为词库文件，用于进行文本分词

app_field_define.txt 定义应用字段的信息，格式如下：

| app_id | field_id    | field_name  | is_primary_key | field_type | index_tag | snapshot_tag | segment_tag | unionField |
|  ----  | ----  |  ----  | ----  |  ----  | ----  |  ----  | ----  |  ----  |
|  10061 |           1 | doc_id      |              1 |          1 |         0 |            0 |           0 |           |
|  10061 |           2 | poi_name    |              0 |          2 |         1 |            1 |           1 |           |
|  10061 |           3 | longitude   |              0 |          5 |         1 |            1 |           0 |           |
|  10061 |           4 | latitude    |              0 |          6 |         1 |            1 |           0 |           |
|  10061 |           5 | gis         |              0 |          1 |         1 |            0 |           0 |           |
|  10061 |           0 | idx_gis_poi |              0 |         11 |         0 |            0 |           0 | 5,2       |

字段详细解释如下：

app_id：应用唯一标识

field_id：字段值，建索引时唯一标识一个字段

field_name：字段名称，由用户自行定义

is_primary_key：该字段是否为主键，一般需要将文档id字段设置为主键，唯一标识一条记录

field_type：字段类型，1:INT，2:SHORT_TEXT，3:TEXT，4:IP，5:GEO_POINT，9:DOUBLE，10:LONG，11:联合索引，14:GEO_SHAPE

index_tag：是否需要对该字段建索引

snapshot_tag：是否需要在快照中保存该字段

segment_tag：分词模式，0：不进行分词 1：默认分词模式 3：汉拼分词 5：支持范围查询

segment_feature：分词特征

unionField：联合索引对应的field_id

**索引读服务**

index_read的配置文件位于/usr/local/isearch/index_read/conf目录。

index_read.conf 采用json格式，主要定义程序日志级别、监听端口、以及对应缓存服务的端口信息等

app_field_define.txt 定义应用的字段信息

words_base.txt 为词库文件，用于进行文本分词

修改应用配置信息后，需要重启进程，启动和停止脚本都在bin目录下。


### 管理索引

添加文档的格式见send.json文件，操作命令为./client_test send.json 106 127.0.0.1 11017，该命令直接向index_write服务发送一个tcp请求，进行索引的导入，支持导入多行数据。

其中appid为应用id，fields_count为文档数量，cmd为add时表示新增文档，fields字段中为文档对应的数据，字段名必须与app_field_define.txt文件中定义的保持一致。

```
{"appid":10001,"table_content": {"cmd":"add", "fields": {"author":"张三","content":"亚运会第14天的比赛结束后，中国代表团以132枚金牌、92枚银牌和65枚铜牌列在金牌榜的首位，日本代表团以74枚金牌、56枚银牌和74枚铜牌列在次席，韩国代表团以49枚金牌、57枚银牌和70枚铜牌列在第三","title":"亚运会今闭幕 杭州8分钟传达什么信息","weight":430,"doc_id":"1381"}}}

```

### 使用搜索

查询的格式见search.json文件，tcp方式的操作命令为./client_test search.json 101 127.0.0.1 12003，请求示例如下：

```
// 查询content字段包含京东的记录
{"query":{"match":{"content":"京东"}}, "appid":10001, "page_index":"1", "page_size":"10","fields":"title"}

```

|  参数   | 类型  | 是否必须 | 描述 |
|  ----  | ----  | ----  | ----  |
| appid  | int | 是  | 应用id |
| query  | string | 否  | 搜索查询词 |
| page_index  | int | 否  | 页码 |
| page_size  | int | 否  | 每页条数 |
| sort_type  | int | 否  | 排序类型 |
| sort_field  | string | 否  | 排序字段 |
| fields  | string | 否  | 返回指定字段值 |

字段说明如下

appid：应用唯一标识

query：查询关键词，支持多字段检索，兼容elasticsearch查询协议

page_index：页码

page_size：每页条数

sort_type：排序类型，1：默认排序 3：不排序 4：按sort_field升序 5：按sort_field降序

sort_field：通过sort_field指定排序的字段

fields：返回指定字段值，多个字段用逗号隔开


**查询结果示例**

```
{"code":0,"count":2,"result":[{"doc_id":"1381","title":"测试标题1","score":2}, {"doc_id":"1382","title":"测试标题2","score":1}]}
```

其中code为返回码，count为匹配到的结果总数，result为匹配到的详细信息，score为匹配度得分，默认由高到低排序。

### 后续规划

1. 新功能
    1. 预计2021年Q2开源热备服务，提供检索系统的单机数据备份能力。
    2. 预计2021年Q3开源路由服务，提供检索系统的分布式能力。
    3. 预计2022年Q1开源哨兵监控服务，提供检索系统的容灾能力。
    4. 预计2022年Q1开源配置中心服务，提供检索系统的统一配置管理能力。
    5. 预计2022年Q2开源扩容服务，提供检索系统的扩展能力。
2. 功能优化
    1. 查询请求格式兼容elasticsearch协议。
    2. 提供restful API。
    3. 支持mysql数据源导入数据。
    4. 提供OLAP聚合功能，进行联机分析。

### 源码编译

运行build.sh脚本可以编译isearch需要的所有bin文件，编译环境说明如下：

由于检索存储层依赖rocksdb，所以编译时需要满足以下前置配置：  
1）CentOS 7.x  
2）gcc 4.8  
3）Cmake版本需要大于等于3.6.2  
4）安装gflags：  
   gflags是google开源的一套命令行参数解析工具，支持从环境变量和配置文件读取参数  
   安装命令：  
   ```
   git clone https://github.com/gflags/gflags.git
   cd gflags
   git checkout -b 2.2 v2.2.2
   cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_SHARED_LIBS=ON -DGFLAGS_NAMESPACE=google -G "Unix Makefiles" .
   make && make install
   sudo ldconfig
   sudo ln -s /usr/local/lib/libgflags.so.2.2 /lib64
   ```
   安装后，需要将gflags的包含路径添加到PATH环境变量中  
5）安装rocksdb依赖库：zlib，bzip2，lz4，snappy，zstandard  
   `sudo yum install -y snappy snappy-devel zlib zlib-devel bzip2 bzip2-devel lz4-devel libasan openssl-devel`

### 项目成员

付学宝--项目发起者、导师、总设计师  
林金明--项目开发  
仇路--项目开发  
朱林--项目开发  
杨爽--项目开发  
陈雨杰--项目开发

### 特别感谢

感谢京东副总裁王建宇博士给予项目的大力支持，多次参与指导提供建议和方向！