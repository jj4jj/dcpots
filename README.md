#DAgent
distribute system basic componets

**DAgent main feature**
*. lib for distribute agent client
*. distribute agent server for report bussiness, monitor, machine statistics , python extension etc.



**Architecture**
* Bussiness Application [reporter , execter , monitor , machine statistics]
* DAgent
* DCNode

"""

        1:N communication module



                                        root[tcp:server]


                    agent[smq:pull , tcp:server]

    node[leaf]


           leaf node:
                    send msg



"""




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
