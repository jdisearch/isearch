### 项目背景与介绍

本团队开发的检索服务提供分词和建立索引功能，可自定义中文词库，可满足复杂查询需求，支持包括字段检索、字段排序、布尔搜索、范围检索、地理位置（POI、AOI）查询等功能。
优势在于：
1)    中文分词，内置各种中文支持。
2)    C++开发，轻量化，4核8G内存的机器就能支持大规模存储。
3)    支持存储节点分片数动态增加，方便扩容。

### 体验demo项目
为了省去配置机器环境的麻烦，建议通过docker来运行demo容器，在终端执行如下命令：  
```
// KEYWORD_CACHE_SIZE和HANPIN_CACHE_SIZE用于设置缓存的大小，单位为M，默认是100M
docker run --env KEYWORD_CACHE_SIZE=1024 --env HANPIN_CACHE_SIZE=1024 -d intelligentsearch/search_local:1.0
```
启动容器后，通过如下命令进入容器：  
```
docker exec -it 容器id /bin/sh

```
容器id替换为上一步启动的容器对应的id

然后执行如下命令：

```
cd /usr/local/intelligentsearch/tools
./client_test send.json 106 127.0.0.1 11017
./client_test search.json 101 127.0.0.1 12003
```
以上命令首先将send.json中的数据导入到索引中，然后对search.json中的内容进行查询，并返回查询结果。

### 架构图
![架构图](http://storage.jd.com/search-index/intelligentsearch.jpg)

### 特色优势
1.容器化管理，方便快速部署和扩容，通过dockerfile一键编译部署，快速上手  
2.支持http方式进行数据导入和查询（需部署接入层服务），若通过sdk或tcp方式访问，则只需部署索引层服务即可  
3.采用稳定高效的C++开发，高速搜索响应，架构简洁    
4.提供了丰富的功能，开发周期更短，支持包括字段检索、字段排序、布尔搜索、范围检索、地理位置（POI、AOI）查询等功能  

### 项目配置文件
**索引写服务**

index_write的配置文件位于/usr/local/intelligentsearch/index_write/conf目录，以下为配置文件说明：

index_write.conf 采用json格式，主要定义程序日志级别、监听端口、以及对应缓存服务的端口信息等

words_base.txt 为词库文件，用于进行文本分词

app_field_define.txt 定义应用字段的信息，格式如下：

| id    | app_id  | field_name | is_primary_key | field_type  | index_tag | snapshot_tag | segment_tag | field_value | create_time  | segment_feature | reserved2 |
|  ----  | ----  | ----  | ----  |  ----  | ----  | ----  | ----  |  ----  | ----  | ----  | ----  |
|  1    |  10001  | doc_id | 1 |     2 |    0 |     0 |        0 |       4 | 2018-12-10 16:51:47 |  0 |   0 |
|  2    |  10001  | weight | 0 |     1 |    1 |     1 |        5 |       5 | 2018-12-10 16:52:11 |  0 |   0 |
|  3    |  10001  | author | 0 |     2 |    1 |     1 |        3 |       6 | 2018-12-10 16:52:21 |  0 |   0 |
|  4    |  10001  | title  | 0 |     2 |    1 |     1 |        0 |       2 | 2018-12-10 16:52:32 |  0 |   0 |
|  5    |  10001  | content| 0 |     3 |    1 |     1 |        1 |       1 | 2018-12-10 16:52:43 |  0 |   0 |


字段详细解释如下：

id：自增id

app_id：应用唯一标识

field_name：字段名称，由用户自行定义

is_primary_key：该字段是否为主键，一般需要将文档id字段设置为主键，唯一标识一条记录

field_type：字段类型，1:INT，2:SHORT_TEXT，3:TEXT，4:IP，5:经度，6:纬度，9:DOUBLE，10:LONG

index_tag：是否需要对该字段建索引

snapshot_tag：是否需要在快照中保存该字段

segment_tag：分词模式，0：不进行分词 1：默认分词模式 3：汉拼分词 5：支持范围查询

field_value：字段值，建索引时唯一标识一个字段

create_time：创建时间

segment_feature：分词特征

reserved2：保留字段

**索引读服务**

index_read的配置文件位于/usr/local/intelligentsearch/index_read/conf目录。

index_read.conf 采用json格式，主要定义程序日志级别、监听端口、以及对应缓存服务的端口信息等

app_field_define.txt 定义应用的字段信息

words_base.txt 为词库文件，用于进行文本分词

修改应用配置信息后，需要重启进程，启动和停止脚本都在bin目录下。

### 配置文件转换工具

在/usr/local/intelligentsearch/tools目录下的parse_ini工具可以将ini格式的配置文件转化为index_write需要的格式，app_field.ini的示例如下：

```
[appid]
project.appid = 10001
 
[doc_id]
is_primary_key = 1
field_type = 2
index_tag = 0
snapshot_tag = 0
 
[title]
field_type = 2
 
[author]
field_type = 2
segment_tag = 3
 
[content]
field_type = 3
segment_tag = 1
 
[weight]
field_type = 1
segment_tag =5
```

在appid字段下面定义一个应用id，其它字段则依据应用的字段来定义，建议与数据库保持一致，执行parse_ini后会在当前目录生成相应的app_field_define.txt文件，将其拷贝到index_write和index_read的相应目录即可。

### 管理索引

添加文档的格式见send.json文件，操作命令为./client_test send.json 106 127.0.0.1 11017，该命令直接向index_write服务发送一个tcp请求，进行索引的导入，支持导入多行数据。

其中appid为应用id，fields_count为文档数量，cmd为add时表示新增文档，fields字段中为文档对应的数据，字段名必须与app_field_define.txt文件中定义的保持一致。

```
{"appid":10001,"fields_count":1, "table_content": [{"cmd":"add", "fields": {"author":"张三","content":"亚运会第14天的比赛结束后，中国代表团以132枚金牌、92枚银牌和65枚铜牌列在金牌榜的首位，日本代表团以74枚金牌、56枚银牌和74枚铜牌列在次席，韩国代表团以49枚金牌、57枚银牌和70枚铜牌列在第三","title":"亚运会今闭幕 杭州8分钟传达什么信息","weight":430,"doc_id":"1381"}}]}

```

### 使用搜索

查询的格式见index_read.conf文件，tcp方式的操作命令为./client_test search.json 101 127.0.0.1 12003，请求示例如下：

```
// 查询content字段包含京东的记录
{"key_and":"content:京东", "appid":10001, "page_index":"1", "page_size":"10","fields":"title"}

```

|  参数   | 类型  | 是否必须 | 描述 |
|  ----  | ----  | ----  | ----  |
| appid  | int | 是  | 应用id |
| key  | string | 否  | 搜索查询词 |
| key_and  | string | 否  | 包含全部关键词 |
| key_invert  | string | 否  | 过滤关键词 |
| page_index  | int | 否  | 页码 |
| page_size  | int | 否  | 每页条数 |
| sort_type  | int | 否  | 排序类型 |
| sort_field  | string | 否  | 排序字段 |
| fields  | string | 否  | 返回指定字段值 |

字段说明如下

appid：应用唯一标识

key：查询关键词，支持多字段检索，多个条件之间取并集

key_and：查询关键词，支持多字段检索，多个条件之间取交集

key_invert：需要过滤的关键词

page_index：页码

page_size：每页条数

sort_type：排序类型，1：默认排序 3：不排序 4：按sort_field升序 5：按sort_field降序

sort_field：通过sort_field指定排序的字段

fields：返回指定字段值，多个字段用逗号隔开

**布尔搜索**

"key_and":"content:京东 author:张三"  // 并且

"key":"content:京东 author:张三" // 或者

"key":"content:京东" key_invert:"author:张三" // 排除

**查询结果示例**

```
{"code":0,"count":2,"hlWord":["京东"],"result":[{"doc_id":"1381","title":"测试标题1","score":2}, {"doc_id":"1382","title":"测试标题2","score":1}]}
```

其中code为返回码，count为匹配到的结果总数，hlWord为结果需要高亮的词语，result为匹配到的详细信息，score为匹配度得分，默认由高到低排序。

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

### 项目成员

付学宝--项目发起者、导师、总设计师  
林金明--项目开发  
仇路--项目开发  
朱林--项目开发  
杨爽--项目开发  
陈雨杰--项目开发

### 特别感谢

感谢京东副总裁王建宇博士给予项目的大力支持，多次参与指导提供建议和方向！