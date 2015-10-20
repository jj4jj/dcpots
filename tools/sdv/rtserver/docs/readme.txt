数据可视化服务设计


功能描述
	1. 实时行情图表展示
	2. 历史行情图表分析
	3. 实盘数据推送
	4. 监控等实时推送应用

性能需求：
	1. 支持1000 TPS （推送 > 4K）每秒（请求）
	2. 实时业务延时小于1s

功能分解：
	1. 实时数据推送
	2. 实时行情直播
	3. 实盘数据更新
	4. 图表数据转换
	5. 行情图表指标计算 × （计算服务）

层次分解设计：

	按照数据流顺序可将系统化为三个层次 ，每个层次按照处理顺序以及逻辑关系可以再简单分为若干模块，一种分解方式如下 。
	1. 接入层（性能要求）
		1.1 websocket server
		1.2 负载均衡

	2. 逻辑层
		2.1 实时数据推送服务
		2.2 行情直播
		2.3 实盘数据处理
		2.4 图表数据转换
		2.5 数据技术分析
		2.6 与其他服务通信 (IPC)

	3. 数据层
		3.1 历史数据（KDB+）
		3.2 实时数据（各种行情源，监控上报等）


模块结构分解设计:

	1. 总体设计

————————————————————————————————————————————————————————————————
|  总体结构：|
——————
							 kdb+			        data stream source
								 \				    /
								  \			      /											        数据层
							_______\____________/__________________________________________________
										|							            IPC
									 datastore		(misc apps)  ————————|
										|			   |
										|——————————————  data analysis
										|			   |
										|		   mychart agent
										|	     ________|											逻辑层
										|	    /
							______________|_____/__________________________________________________
											|
											|
										Websocket server		websocket server
											|					   |
											LB————————————|
										        |													接入层
										        |

*注解：
	按照层次设计将系统化为3层，
	1. 接入层
		1.1 负责管理用户的持久连接 （WEBSOCKET CONNECTION）， 将连接映射到用户 （认证）。
		1.2 将用户上传消息（如果有的话）投递给逻辑层处理
		1.3 承接逻辑层的用户消息，推送给用户
	2. 逻辑层
		2.1 接受用户消息转发给相应的实时应用（如果应用订阅了消息的话）
		2.2 接受其他服务（django ， 其他后端等）的推送消息，转发给接入层推送 。
		2.3 图表代理（构造客户端可以直接渲染的图表数据）
		2.4 业务数据分析
		2.5 从db取数据或者订阅其他流式数据源
	3. 数据层
		3.1 提供历史数据（DB）
		3.2 提供动态直播数据
————————————————————————————————————————————————————————————————

	2. 实时推送（push）
		2.1 本质是一个websocketserver  (tcpserver）， 与浏览器建立持久连接，便于即时推送消息 。
		2.2 性能要求，server 在连接层面应当能支持并发连接1000以上（活跃连接80%，800活跃，每个活跃连接下行推送速度 20KB/s）。
		2.3 包含两个主要功能：连接管理（websocket 连接），用户认证 （每个websocket 在hand shake时候需要先认证，认证完成后才可以握手成功）。
		2.4 websocket server技术选型
			2.4.1 django-websocket-redis  优势：集成了权限验证和前端库，以及频道 ，使用简单; 劣势：性能不详， 与redis绑定很紧密，客户端的上传消息直接放到redis中 ， 服务器给客户端推送也直接到redis 。
			2.4.2 tornado ， 基于gevent，性能良好 ，含有websocket server封装 。
			2.4.3 bottle , flask 其他 python webserver（集成websocket server)  .
			2.4.5 C - host websocket server

	3. 实时应用
		3.1 行情直播
			3.1.1 接受行情源数据
			3.1.2 推送到行情中心
			3.1.3 推送到订阅行情的客户端
			3.1.4 客户代理可视化
		3.2 其他服务数据推送
			3.2.1 IPC组件支持接受其他服务消息
			3.2.2 服务代理，定制可视化内容
		3.3 可视化代理
			3.3.1 在服务端抽象图表组件
			3.3.2 服务端可以直接画图（代理）生成想要的图

	4. 计算服务
		4.1 指标计算
		4.2 分析应用

关键流程

	1. 连接管理
		websocketserver （gevent）
	2. 用户验证，连接映射
		websocket 协议握手时验证session ， session通过redis（参考django-websocket-redis）
	3. 消息投递处理
		消息中间件 ， 可选为zmq或者redis（list，pubsub）
	4. 图表代理
		制定前端图表支持的数据，操作等 协议 ，形成服务端代理，使得服务器看起来可以直接操作图表
	5. 计算
		计算库，python - anaconda ， scipy 等支持
	6. IPC
		与其他服务的通信（收发）， 消息中间件 （消息投递代理）。
		收取本服务消息，分发消息（客户端消息是否区分） 。
	7. 直播同步
		同步方案：直播目前数据，客户端请求历史数据 。
	8. 负载均衡/扩容
		nginx



逻辑组件关系：







		IOLoop
			|
			|									useragent 	messagedispatcher	mychartagent		live		quotecenter
			|——————————|
						|		client_msg_handler     (websocket msg)
					    |		service_msg_handler （redis list / pubsub）		RedisMessageQueue , RedisPublisher/RedisSubscriber
			<——————————
			|



		1. 独立进程
			1.1. 启动一个websocket server  io 线程处理连接
			1.2. 线程使用gevent库收到消息，将消息放到 client_recv_msg_queue 中 。
			1.3  从client_send_msg_queue 中取消息，发送给客户端 。
		2. 进程主线程有个消息循环
			2.1 消息循环从client_recv_msg_queue 中取消息
			2.2 redis message queue 取消息
			2.3 从redis subscriber 取消息
		3. 处理消息
			3.1 handler
			3.2 功能实体


		ClientConnectionHandler (manager)
			def on_client_handshake(request) -> check user
			def on_client_connected(socket)	-> map an useragent
			def on_client_close()
			def on_client_msg()

		UserManager
			map(socket, useragent)
			put()
			remove()
			get_user_byid(id)
			get_users(username)	#multi users connection
		UserAgent:
			socket
			userstate
			on_open()
			on_mesasge()
			on_close()

		main():
			client_msg_queue = (recv_q , send_q)
			redis_session_storage = []
			um = UserManager()
			###### io thread
			websocket_server =  WebSocketServer(bindaddr , client_msg_queue)
			websocket_server.set_handler( ClientConnectionHandler(um) )
			websocket_server.start()
			##### worker service msg handler
			whiel True:
				msg = service_msg_q.get_msg()
				dispatch_service_msg()
			######main
			join(workers)

	service_msg_q -> RedisMessageQueue
		init(queue_name)
			connect
		put
			redis.rpush
		get
			redis.lpop
	UserAuthorization
		session = get(request.session['sessionid'])
			session -> user



——————————————————————————————————————————————————————————————————
性能测试：
	1. http 处理能力
		1.1 ab （apache benchmark)
		1.2 siege (http load testing)
——————————————————————————————————
			部署方式为：nginx 负载均衡 到 4个 tornado
——————————————————————————————————
1000并发用户测试(非loopback)：
————————————
Lifting the server siege...      done.

Transactions:		      102840 hits
Availability:		       99.90 %
Elapsed time:		       52.26 secs
Data transferred:	    16936.84 MB
Response time:		        0.01 secs
Transaction rate:	     1967.85 trans/sec
Throughput:		      324.09 MB/sec
Concurrency:		       11.78
Successful transactions:      102840
Failed transactions:	         104
Longest transaction:	        1.01
Shortest transaction:	        0.00
——————————————————————————————————
吞吐量测试（非loopback）：
—————————————

Lifting the server siege...      done.

Transactions:		      110745 hits
Availability:		      100.00 %
Elapsed time:		       41.06 secs
Data transferred:	    18238.70 MB
Response time:		        0.01 secs
Transaction rate:	     2697.15 trans/sec
Throughput:		      444.20 MB/sec
Concurrency:		       14.94
Successful transactions:      110745
Failed transactions:	           0
Longest transaction:	        0.10
Shortest transaction:	        0.00

——————————————————————————————————
			部署方式为：直连 tornado
——————————————————————————————————
吞吐量测试：
————————
Lifting the server siege...      done.

Transactions:		       58324 hits
Availability:		      100.00 %
Elapsed time:		       44.92 secs
Data transferred:	     9605.44 MB
Response time:		        0.01 secs
Transaction rate:	     1298.40 trans/sec
Throughput:		      213.83 MB/sec
Concurrency:		       14.98
Successful transactions:       58324
Failed transactions:	           0
Longest transaction:	        0.21
Shortest transaction:	        0.00
——————————————————————————————————
1000并发用户测试：
———————————
Lifting the server siege...      done.

Transactions:		       86045 hits
Availability:		      100.00 %
Elapsed time:		       68.61 secs
Data transferred:	    14170.83 MB
Response time:		        0.29 secs
Transaction rate:	     1254.12 trans/sec
Throughput:		      206.54 MB/sec
Concurrency:		      363.32
Successful transactions:       86045
Failed transactions:	           0
Longest transaction:	       44.00
Shortest transaction:	        0.00
——————————————————————————————————

———————————————————————————————————————————————————————
|     tornado websocet server  1000/500个用户并发 ， 部署方式，单进程 tornado websocket server			  |
———————————————————————————————————————————————————————
10000次请求处理时间（使用loopback）为2.272s
平均单个请求时间 2.272/10 ms = 0.272s ms = 272 us
每秒系统可以处理最大请求数目为：4401个/s
——————————————————————————————————
pax@pax:~/mywork/git/mytest/tornado$ time python ws_client_benchmark.py 1000 10000

real	0m2.272s
user	0m1.880s
sys	0m0.180s

500并发用户，请求1000次时间为：
pax@pax:~/mywork/git/mytest/tornado$ time python ws_client_benchmark.py 500 10000

real	0m1.689s
user	0m1.316s
sys	0m0.144s

——————————————————————————————————
tornado websocet server  1000个用户并发，100000 次请求处理时间（使用loopback）为15.541s
——————————————————————————————————
平均结果时单个请求在15.541s/100 ms= 151 us左右
最大请求处理能力为：6434个/s
—————————————————————
pax@pax:~/mywork/git/mytest/tornado$ time python ws_client_benchmark.py 1000 100000

real	0m15.541s
user	0m12.096s
sys	0m1.064s


500并发用户：
pax@pax:~/mywork/git/mytest/tornado$ time python ws_client_benchmark.py 500 100000

real	0m12.974s
user	0m9.660s
sys	0m0.892s


————————
100W次
APT= 150.124s/1000 ms = 150 us
TPS = 6661个/s

pax@pax:~/mywork/git/mytest/tornado$ time python ws_client_benchmark.py 1000 1000000

real	2m30.124s
user	1m56.980s
sys	0m9.552s

——————————————————————————————————————————————————
广播：
1000个用户并发，服务器推送，客户端收到1000个响应的时间为：0.964s (包括连接时间)。
pax@pax:~/mywork/git/mytest/tornado$ time python broadcast_cli_ws.py 1000 1000

real	0m0.964s
user	0m0.748s
sys	0m0.072s


————————————————————————————————————————————————————————————
django_websocket_redis
并发1000个客户端，接受1000条消息（来自服务器心跳）

pax@pax:~/mywork/git/mytest/websocket_perftest$ time python test_django_websocket_cli.py 1000 1000

real	0m1.195s
user	0m0.860s
sys	0m0.124s

——————————
1W条记录：
——————————
pax@pax:~/mywork/git/mytest/websocket_perftest$ time python test_django_websocket_cli.py 1000 10000

real	0m2.304s
user	0m2.012s
sys	0m0.232s
------------------------------（未使用uwsgi） 。

bottle server
——————————————————————————————————————————————————————
—————————1000个并发用户，1W次请求响应 。
pax@pax:~/mywork/git/mytest/websocket_perftest$ time python test_bottle_websocket_cli.py 1000 10000

real	0m2.295s
user	0m2.064s
sys	0m0.228s

——————————10W
pax@pax:~/mywork/git/mytest/websocket_perftest$ time python test_bottle_websocket_cli.py 1000 100000

real	0m13.686s
user	0m12.588s
sys	0m1.088s

___________________100W

pax@pax:~/mywork/git/mytest/websocket_perftest$ time python test_bottle_websocket_cli.py 1000 1000000

real	2m1.638s  -> 121.638 s
user	1m52.556s
sys	0m9.024s

——————————————————————————————————————————————————
结论：tornado 单进程websocket server性能可以达到1000并发用户最大每秒处理6000个请求左右 。
从测试结果看，tornado 的websocket server 性能足以支撑可视化服务器前端（单进程即可）（交互协议和推送）
bottle使用gevent.wsgihandler ， 性能比tornado差不多 。
django_websocket_redis 比 tornado 在非代理情况发持平：
总体来看
django >= bottle >= django_websocket_redis
websocket server上几乎不分伯仲 。

——————————————————————————————————————————————————————
apache benchmark （nginx代理4个tornado webserver） 测试（结果与siege的测试结果RPS高）：
TPS为 2400 个每秒
平均请求响应时间为409ms
Requests per second:    2442.16 [#/sec] (mean)
Time per request:       409.474 [ms] (mean)
Time per request:       0.409 [ms] (mean, across all concurrent requests)

——————————————————————————————————————————————
并发数为1000
————————
Server Software:        nginx/1.6.2
Server Hostname:        192.168.1.123
Server Port:            8080

Document Path:          /
Document Length:        192 bytes

Concurrency Level:      1000
Time taken for tests:   4.095 seconds
Complete requests:      10000
Failed requests:        9118
   (Connect: 0, Receive: 0, Length: 9118, Exceptions: 0)
Non-2xx responses:      883
Total transferred:      1576723022 bytes
HTML transferred:       1574593363 bytes
Requests per second:    2442.16 [#/sec] (mean)
Time per request:       409.474 [ms] (mean)
Time per request:       0.409 [ms] (mean, across all concurrent requests)
Transfer rate:          376035.46 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0   65 244.8      0    1002
Processing:     1  161 419.2    105    3059
Waiting:        1  160 419.3    105    3059
Total:          1  226 588.2    107    4058

Percentage of the requests served within a certain time (ms)
  50%    107
  66%    119
  75%    125
  80%    128
  90%    136
  95%   1116
  98%   2214
  99%   4020
 100%   4058 (longest request)

——————————————————————————————————————————
并发数为500
——————
Server Software:        TornadoServer/4.0.2
Server Hostname:        192.168.1.123
Server Port:            8080

Document Path:          /
Document Length:        172691 bytes

Concurrency Level:      500
Time taken for tests:   4.231 seconds
Complete requests:      10000
Failed requests:        0
Total transferred:      1729090000 bytes
HTML transferred:       1726910000 bytes
Requests per second:    2363.36 [#/sec] (mean)
Time per request:       211.563 [ms] (mean)
Time per request:       0.423 [ms] (mean, across all concurrent requests)
Transfer rate:          399068.66 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    0   1.2      0       6
Processing:     1  139 218.3    122    3008
Waiting:        1  138 218.3    121    3008
Total:          1  139 218.2    122    3008

Percentage of the requests served within a certain time (ms)
  50%    122
  66%    132
  75%    137
  80%    142
  90%    162
  95%    184
  98%   1087
  99%   1131
 100%   3008 (longest request)

——————————————————————————————————————————————————————————————————————————

















