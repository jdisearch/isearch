为了省去配置机器环境的麻烦，建议通过docker来运行demo容器，在终端执行如下命令：  
```
// KEYWORD_CACHE_SIZE和HANPIN_CACHE_SIZE用于设置缓存的大小，单位为M，默认是100M
docker run --env KEYWORD_CACHE_SIZE=1024 --env HANPIN_CACHE_SIZE=1024 -d intelligentsearch/search_local:1.0
```
启动容器后，通过如下命令进入容器：  
```
docker exec -it 容器id /bin/bash

```
容器id替换为上一步启动的容器对应的id

### 1. 查看项目配置文件

isearch应用的字段配置文件名称为app_field_define.txt，通过以下指令查看配置文件内容：
```
cat /usr/local/intelligentsearch/index_write/conf/app_field_define.txt
```
可以看到，demo应用的appid为10001，对应的字段为doc_id、title、content、author、weight、telephone、longitude、latitude。

### 2. 上报索引数据

执行如下命令进行测试数据的导入。
```
cd /usr/local/intelligentsearch/tools
./client_test send.json 106 127.0.0.1 11017
```
若屏幕显示如下信息，则表示数据导入成功：
```
send count == 1
recv: 27 bytes
server message: {"code":0}
```
可以通过修改send.json中的数据来导入更多的测试数据。

### 3. 测试搜索

首先，我们分别以亚运会、金牌、公园进行搜索（修改search.json文件）：
```
{"key":"亚运会", "appid":10001, "fields":"title"}
{"key":"金牌", "appid":10001, "fields":"title"}
{"key":"公园", "appid":10001, "fields":"title"}
```
执行如下命令进行数据的查询：
```
cd /usr/local/intelligentsearch/tools
./client_test search.json 101 127.0.0.1 12003
```
以下为公园的搜索结果：
```
{
	"code": 0,
	"count": 1,
	"hlWord": ["公园"],
	"result": [{
		"doc_id": "12351",
		"score": 11.689424985915906,
		"title": "非洲特色动植物首次亮相北京市属公园 "
	}]
}
```
接下来测试复杂的字段以及布尔搜索：
```
// title包含公园的记录
{"key":"title:公园", "appid":10001, "fields":"title"}
// title包含公园或者author是张三的记录
{"key":"title:公园 author:张三", "appid":10001, "fields":"title"}
// content字段包含京东并且author是张三的记录
{"key_and":"content:京东 author:张三", "appid":10001, "fields":"title"}
// content字段包含京东并且author不是张三的记录
{"key":"content:京东", "key_invert":"author:张三", "appid":10001, "fields":"title"}
```
接下来测试模糊匹配的情形：
```
{"key":"author:zs", "appid":10001,"fields":"title"}
{"key":"author:张", "appid":10001,"fields":"title"}
```
接下来测试范围查的情形：
```
// weight字段在21到24之间的记录
{"key":"weight:[21,24]", "appid":10001, "fields":"title"}
// weight字段在21到24之间的记录，并且结果按照weight升序排列
{"key":"weight:[21,24]", "sort_type":"4", "sort_field":"weight", "appid":10001 ,"fields":"title"}
```
接下来测试地理位置查询的场景：
```
// 查询坐标点[116.50, 39.76]附近900米范围内的记录
{"key_and":"longitude:116.50 latitude:39.76 distance:0.9", "appid":10001, "fields":"title"}
```