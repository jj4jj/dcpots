#coding:utf8

import json
import config
from .redis_message_queue import RedisMessageQueue


class ServiceMessageQueue():
    def __init__(self, name, host=config.REDIS_MQ_HOST, port=config.REDIS_MQ_PORT, db=config.REDIS_MQ_DB):
        self.rmq = RedisMessageQueue(name=name, host=host, port=port, db=db)

    def get_message(self):
        #json
        jmsg = self.rmq.get()
        msg = json.loads(jmsg)
        #src,data,type
        return msg

    @staticmethod
    def get_msg_type(msg):
        return msg['type']

    @staticmethod
    def get_msg_src(msg):
        return msg['src']

    @staticmethod
    def get_msg_data(msg):
        return msg['data']

    @staticmethod
    def format_msg(src, mtype, data):
        rawmsg = {'src': src, 'type': mtype, 'data': data}
        jmsg = json.dumps(rawmsg)
        return jmsg

    def put_message(self, mtype, msg, src):
        jmsg = ServiceMessageQueue.format_msg(src, mtype , msg)
        self.rmq.put(jmsg)


#########################################################################
def stress_wtest(N):
    s = ServiceMessageQueue('mytestqueue')
    m = ['hello', 'world', '!', {'hello': 'world!'}]
    for i in xrange(0, N):
        s.put_message('hsld', m, 'test')
def stress_rtest(N):
    s = ServiceMessageQueue('mytestqueue')
    m = ['hello', 'world', '!', {'hello': 'world!'}]
    msgs = []
    for i in xrange(0, N):
        s.get_message()


def function_test():
    s = ServiceMessageQueue('mytestqueue')
    m = ['hello', 'world', '!', {'hello': 'world!'}]
    print m
    s.put_message('hsld', m, 'test')
    msg = s.get_message()
    print '-'*40
    print msg
    print '-'*40
    for i in msg:
        print i

if __name__ == '__main__':
    import sys
    if len(sys.argv) > 0:
        N=int(sys.argv[1])
        stress_wtest(N)
        stress_rtest(N)
    else:
        function_test()