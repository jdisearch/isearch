## 源码编译方式（推荐）

建议通过isearch_env镜像进行源码编译，获取isearch_env镜像的方式为： `docker pull intelligentsearch/isearch_env:2.0`

也可以自行编译isearch_env镜像，Dockerfile文件位于dockerfiles\env目录： `docker build -t intelligentsearch/isearch_env:2.0 .`

然后运行容器： `docker run -itd intelligentsearch/isearch_env:2.0`

进入容器： `docker exec -it 容器id /bin/bash`

执行如下命令进行编译和安装：
```
cd /usr/local
git clone https://gitee.com/jd-platform-opensource/isearch.git
mv isearch jdisearch
cd jdisearch
sh build.sh
sh install.sh
```

## 直接获取镜像（包含编译好的程序）

为了省去配置机器环境的麻烦，建议通过docker来运行demo容器，在终端执行如下命令：  
```
docker run -d intelligentsearch/isearch:latest
```
启动容器后，通过如下命令进入容器：  
```
docker exec -it 容器id /bin/bash

```
容器id替换为上一步启动的容器对应的id

### 1. 查看项目配置文件

isearch应用的字段配置文件名称为app_field_define.txt，通过以下指令查看配置文件内容：
```
cat /usr/local/isearch/index_read/conf/app_field_define.txt
```

### 2. 上报索引数据

1）tcp方式

执行如下命令进行测试数据的导入。
```
cd /usr/local/isearch/tools
./client_test send.json 106 127.0.0.1 11017
```
若屏幕显示如下信息，则表示数据导入成功：
```
send count == 1
recv: 27 bytes
server message: {"code":0}
```
可以通过修改send.json中的数据来导入更多的测试数据。

2）http方式

执行如下命令
```
cd /usr/local/isearch/tools
sh load_data.sh
```

### 3. 测试搜索


执行如下命令进行数据的查询：
```
cd /usr/local/isearch/tools
// tcp方式
./client_test search.json 101 127.0.0.1 12003
// http方式
sh query.sh
```

以下为搜索结果的示例：
```
{
	"code": 0,
	"count": 1,
	"result": [{
		"doc_id": "12351",
		"score": 11.689424985915906,
		"title": "非洲特色动植物首次亮相北京市属公园 "
	}]
}
```

以下是针对search.json中查询请求的说明
```
// match查询
{"appid":10010,"query":{"match":{"birthPlace":"上海市"}}}
{"appid":10010,"query":{"match":{"dreamPlace":"王国"}}}
{"appid":10010,"query":{"match":{"name":"Jackie"}}}
// term查询
{"appid":10010,"query":{"term":{"year":20}}}
// range查询
{"appid":10010,"query":{"range":{"height":{"gte":175 ,"lte": 180}}}}
// geo_distance查询
{"appid":10010,"query":{"geo_distance":{"currentLocation":"39.452, -76.589","distance":"0.5"}}}
// geo_polygon查询
{"appid":10010,"query":{"geo_polygon":{"currentShape":{"points":"POLYGON((121.437271 31.339747, 121.438022 31.337291, 121.435297 31.336814, 121.434524 31.339252, 121.437271 31.339747))"}}}}
// bool查询
{"appid":10010,"query":{"bool":{"must":{"match":{"birthPlace":"中华人民共和国"},"range":{"brithday":{"gt":19900654,"lte":19931124}}},"must_not":{"match":{"homeAddress":"上海市"}}}}}
{"appid":10010,"query":{"bool":{"must":{"match":{"birthPlace":"上海市"},"geo_distance":{"currentLocation":"39.452, -76.589","distance":"1.123"}}}}}
{"appid":10010,"query":{"bool":{"must":{"match":{"birthPlace":"上海市"},"term":{"gender":"男"}}}}}
// 通过page_index进行翻页
{"appid":10010,"query":{"match":{"dreamPlace":"阿姆斯"}},"fields":"birthPlace,homeAddress,dreamPlace,name","page_index":1,"page_size":3}
// 按height字段进行排序
{"appid":10010,"query":{"range":{"height":{"gte":174 ,"lte": 180}}},"fields":"birthPlace,height,name","sort_type":"5","sort_field":"height"}
```