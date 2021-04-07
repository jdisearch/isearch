### 测试内容

本次测试是针对isearch的index_read进行的压力测试，只对查询接口进行压力测试，单机压力测试通过tcp进行请求。

### 测试方法

本次采用自己编译的测试工具isearchbench，原理是开启多个线程，每个线程依次与index_read建立tcp连接，发送检索请求，并等待结果返回。测试数据来源于discovery_release_article表，共100w+文档，事先已经将索引导入到机器中。

### 测试目标

获取在单机部署情况下查询接口最大QPS值

### 测试环境

| 机器 | 机器编号    | 操作系统  | CPU | 内存 |
|  ----  | ----  |  ----  | ----  |  ----  |
|  isearch机器 |           A | CentOS Linux release 7.2.1511    |              4核 |          8G |
|  压测脚本机器 |           B | CentOS Linux release 7.2.1511    |              4核 |          8G |
|  压测脚本机器 |           C | CentOS Linux release 7.2.1511    |              4核 |          8G |

### 性能测试结果

以下为压测的详细数据：

| 检索词 | 机器数 | 每台机器进程数 | 总进程数 | 每个进程开启线程数 | 每个线程请求数 | 总请求数   | 总耗时(平均) | index_read进程cpu | cache进程cpu | qps   |
|-----|-----|---------|------|-----------|---------|--------|---------|---------|----------|-------|
| 京东  | 2   | 2       | 4    | 200       | 500     | 400000 | 37s     | 98%    | 35%      | 10272 |
| 京东  | 2   | 2       | 4    | 200       | 1000    | 800000 | 84s     | 98%    | 35%      | 9574  |

以下为压测时通过top查看index_read进程所在机器的信息：

![bench-top](http://storage.jd.com/search-index/image/bench-top.png)

以下为请求曲线：

![bench-curve](http://storage.jd.com/search-index/image/bench-curve.png)

可以得出index_read进程的qps最高为10272。