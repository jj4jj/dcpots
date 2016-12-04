#DCPots
DCPots is a distribute system basic componets container solution .

##dcpots##

- lib components for distribute server (C++)



##Features##


```
1. App framework library (base/App.hpp)
2. A non-blocked tcp socket msg event handler lib (client/server) [base/dctcp.h]
3. many utilities function {base/dcutils.hpp)
4. Linux share memory / SystemV msgq wrrapper in a common scene .
5. Comman Server command line parser (getopt wrapper) [base/cmdline_opt.h]
6. DateTime , Collections [hash table and memory pool / block list] (with static flat memory or dynmaic) 
7. Simple Logger and timer (from cloudwu)
8. Corouties (support nested in coroutine) (implement refer from cloudwu)
9. Google Protobuffer meta extensions related, convert all data reprensntation each-other [json/xml/protobuf/mysql ORM](utility/drs)
10. Mysql client multi-thread worker
11. Multi-Thread worker simple GP
12. An tcp/protobuf based RPC implementation (dcrpc)
13. Mongoproxy server (app/mongoproxy)
14. A cluster communication model (dcnode) 
15. etc

```





##depends##

1. libprotobuf 2.6+ (libprotobuf-dev)
2. libpython 2.7.5+ (python-dev)
3. cmake 2.6+


##doing / done / todo / opt##

- router caching [done]
- alloc msg buffer with zero copy [by lower layer allocated]  [opt]
- msgq name manage      [done]
- python extension in agent module  [done]
- same agent [brother] communication with msgq directly [p2p] [done]
- msg persistence [opt]
- dcnode_send should create a send queue . [todo]
- bench mark todo [doing]
- dagent python export [test swig? ]  [done]
- dagent python plugins   [done]
- add dbproxy for mysql [orm]  [done]
- push service [done with (sdv)[https://github.com/jj4jj/sdv.git] ]
- data visualization with echarts [done with (sdv)[https://github.com/jj4jj/sdv.git]]
- utilities
- application framework




##build##
    make clean #optional
    make



##test##
    ls bin/				#show the the test programs 
	./dagent n	l1		#agent
	./dagent n  l2		#root
	./dagent n	l		#leaf
	mkdir -p /tmp/dagent 	#for path token to key
	./collector			#collector recv from reporter
	./reporter			#reporter send to collector


