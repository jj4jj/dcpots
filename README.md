#DAgent
distribute system basic componets

**DAgent main feature**
* lib for distribute agent client
* distribute agent server for report bussiness, monitor, machine statistics , python extension etc.



**Architecture**
* Bussiness Application [reporter , execter , monitor , machine statistics]
* DAgent
* DCNode

```

        1:N communication module



                                        root[tcp:server]


                    agent[smq:pull , tcp:server]

    node[leaf]


           leaf node:
                    1. register name
                    2. send msg by msgq to parent
           agent node:
                    1. register name
                    2. send msg by msgq or tcp to parent
                    3. forward msg [route] to other [known/unknown] node
           root node:
                    1. an agent node with no parent node



Note.
    msgq push and pull  1:N module register name process
    just for keeping the msgq communication channel not changed [persistence], client should recv the msg it lost for Crashing.
    also not need config the uniq id for msgq
    sms: msgq pull end [server]
    smc: msgq push end [client]
    sms:
        1. get shm for name maping
        2. get a request for register name
        3. if name is collision . reject registering
        4. set name map the name->id [etc info] , the ID must not a pid[long :seed+seq] .
        5. resonse to client
    smc:
        1. register name with sms [using pid] [if attach may be run process with same name, this is not valid]
        2. recv name response . got a valid key for communication.
        3. in ready state, smc can use name map shm to lookup peer node.

```





**depends**
* libprotobuf 2.6+
* libpython 2.7.5+
* cmake 2.6+

**optimal todo**
* router caching
* alloc msg buffer with zero copy [by lower layer allocated]
* msgq name manage
* python extension in agent module
* same agent [brother] communication with msgq directly [p2p]
* msg persistence

**install requried [dep]**
* libprotobuf-dev
