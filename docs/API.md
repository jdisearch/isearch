检索服务接口列表
====
## 关键词搜索
----
### 接口描述
>对用户输入的关键词进行相关结果的搜索匹配，搜索结果默认按照关键词与文档的相关性得分进行排序。

### URL
```html
http://127.0.0.1/search
```
### Http Method
POST
### Http返回格式
JSON
### Http请求参数说明
| 参数 | 类型 | 是否必需 | 描述 |
| ------------ | ------------ | ------------ | ------------ |
| appid | int  | 是  | appname对应的ID  |
| query  | string  |  是 | 搜索查询词  |
| page_index  | int  | 否  |  页码 |
| page_size  | int | 否  | 每页条数  |
| sort_type  | int  | 否  | 排序方式  |
| sort_field  | string  | 否  | 排序字段  |
| fields | string  | 否  | 返回指定字段值  |

说明：query兼容elasticsearch协议，格式可参考tools/search.json文件。

应用的定义及支持的字段类型可参考：[项目配置文件](https://gitee.com/jd-platform-opensource/isearch#%E9%A1%B9%E7%9B%AE%E9%85%8D%E7%BD%AE%E6%96%87%E4%BB%B6)

### Http返回结果说明
| 字段  | 类型  | 描述  |
| ------------ | ------------ | ------------ |
| code  | int  | 执行结果  |
| count  | int  | 结果总数  |
| result  | string  | 结果内容  |
### CURL调用示例
```
curl -X POST \
  http://127.0.0.1/search \
  -H 'content-type: application/json' \
  -d '{"appid":10064,"query":{"match":{"author_id":"21386"}},"page_index":1,"page_size":3}'
```
### 成功返回示例
```json
{
	"code": 0,
	"count": 2,
	"result": [{
			"doc_id": "115",
			"score": 20.395478565398534,
		},
		{
			"doc_id": "105",
			"score": 20.016650933441753,
		}
	]
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
>该接口上传记录至索引服务，生成普通索引。

### URL
```html
http://127.0.0.1/insert
```
### Http Method
POST
### Http返回格式
JSON
### Http请求参数说明
| 参数 | 类型 | 是否必需 | 描述 |
| ------------ | ------------ | ------------ | ------------ |
| appid |  string  |  是 | 用户组id  |
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
|  style  |  int | 是  | 样式id  |
|  title | string  | 是  |  标题 |
|  author_id | string  | 是  | 作者id  |
|  createtime |  int |  是 | 时间 |

注：这里fields的参数与appid定义的字段相对应，这里仅是一个示例。

### Http 返回结果说明
| 参数 | 类型 |  描述 |
| ------------ | ------------ | ------------ |
| code  |  int |  执行结果 |
| message  |  string | 信息描述  |

### CURL调用示例
```html
curl -X POST \
  http://127.0.0.1/insert \
  -H 'content-type: application/json' \
  -H 'doc_id: 115' \
  -d '{"appid":10064,"table_content":{"cmd":"add","fields":{"author_id":"21386","createtime":1488981884,"doc_id":"115","status":3,"style":0,"sub_position":1,"title":"内存不够用？关闭这4个功能就行了"}}}'
```
### 成功返回示例
```json
{
  "code": 0
}
```
