检索服务接口列表
====
## 关键词搜索
----
### 接口描述
>对用户输入的关键词进行相关结果的搜索匹配，搜索结果默认按照关键词与文档的相关性得分进行排序。

### URL
```html
http://127.0.0.1/index/search
```
### Http Method
GET
### Http返回格式
JSON
### Http请求参数说明
| 参数 | 类型 | 是否必需 | 描述 |
| ------------ | ------------ | ------------ | ------------ |
| appid | int  | 是  | appname对应的ID  |
| key  | string  |  否 | 搜索查询词  |
| key_and | string  | 否  | 包含全部关键词  |
| key_invert  | string  |  否 | 过滤关键词  |
| page_index  | int  | 否  |  页码 |
| page_size  | int | 否  | 每页条数  |
| sort_type  | int  | 否  | 排序方式  |
| sort_field  | string  | 否  | 排序字段  |
| fields | string  | 否  | 返回指定字段值  |

说明：key支持多字段检索，如key为`content:京东 author:张三`表示搜索content字段包含京东并且author是张三的记录，其中content和city为应用内配置的字段名，应用可以自定义字段名称。

应用的定义及支持的字段类型可参考：[项目配置文件](https://gitee.com/jd-platform-opensource/isearch#%E9%A1%B9%E7%9B%AE%E9%85%8D%E7%BD%AE%E6%96%87%E4%BB%B6)

### Http返回结果说明
| 字段  | 类型  | 描述  |
| ------------ | ------------ | ------------ |
| code  | int  | 执行结果  |
| count  | int  | 结果总数  |
| result  | string  | 结果内容  |
### CURL调用示例
```
curl 'http://127.0.0.1/index/search?page_index=0&key=京东&appid=10001'
```
### 成功返回示例
```json
{
  "code": 0,
  "count": 10,
  "result": [7976,12984,11705,2572,239,474,343,290,10086,318]
}
```
### 错误返回示例
```json
{
  "code": -1,
  "message": "keyword is required"
}
```

## 索引上报
----
### 接口描述
>该接口上传记录至索引服务，生成普通索引表。

### URL
```html
http://127.0.0.1/index/common
```
### Http Method
POST
### Http返回格式
JSON
### Http请求参数说明
| 参数 | 类型 | 是否必需 | 描述 |
| ------------ | ------------ | ------------ | ------------ |
| appid |  string  |  是 | 用户组id  |
| fields_count  | int  |  是 | 内容数量  |
| table_content  |  object |  是 | 内容对象  |
#### table_content的参数
| 参数 | 类型 | 是否必需 | 描述 |
| ------------ | ------------ | ------------ | ------------ |
|  cmd  |  string | 是  |  操作类型 |
|  fields | object  | 是  |  文章对象 |

注：cmd取值包括add,delete,update。

#### fields的参数
| 参数 | 类型 | 是否必需 | 描述 |
| ------------ | ------------ | ------------ | ------------ |
|  doc_id  |  string  | 是  | 文章ID  |
|  weight  |  int | 是  | 文章权重  |
|  author | string  | 是  |  文章作者 |
|  title | string  | 是  | 文章标题  |
|  content |  string |  是 | 文章内容 |
### Http 返回结果说明
| 参数 | 类型 |  描述 |
| ------------ | ------------ | ------------ |
| code  |  int |  执行结果 |
| message  |  string | 信息描述  |
### CURL调用示例
```html
curl -X POST \
  http://127.0.0.1/index/common \
  -H 'content-type: application/json' \
  -d '{
	"appid": 10001,
	"fields_count": 1,
	"table_content": [{
		"cmd": "add",
		"fields": {
			"doc_id": "28394556",
			"weight": 218,
			"author": "zhangsan",
			"title": " 京东阅读电子阅读器即将上线 ",
			"content": "今天小编得到了一个令人振奋的消息！它是什么呢？京东阅读官方即将推出搭载京东阅读客户端"
		}
	}]
  }'
```
### 成功返回示例
```json
{
  "code": 0
}
```
