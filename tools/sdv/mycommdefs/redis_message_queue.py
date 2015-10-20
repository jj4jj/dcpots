#coding:utf8

import redis

class RedisMessageQueue(object):
    """Simple Queue with Redis Backend"""
    def __init__(self, name, namespace='queue', **redis_kwargs):
        """The default connection parameters are: host='localhost', port=6379, db=0
        """
        self.namespace = namespace
        self.__db = redis.Redis(**redis_kwargs)
        self.key = RedisMessageQueue.make_key(namespace, name)

    @staticmethod
    def make_key(namespace, qname):
        return ':'.join((namespace, qname,))

    def qsize(self):
        """Return the approximate size of the queue."""
        return self.__db.llen(self.key)

    def empty(self):
        """Return True if the queue is empty, False otherwise."""
        return self.qsize() == 0

    def put_message(self, dst, msg):
        qkey = RedisMessageQueue.make_key(self.namespace, dst)
        self.__db.rpush(qkey, msg)
        pass

    def put(self, item):
        """Put item into the queue."""
        self.__db.rpush(self.key, item)

    def get(self, block=True, timeout=None):
        """Remove and return an item from the queue.

        If optional args block is true and timeout is None (the default), block
        if necessary until an item is available.
        """
        if block:
            item = self.__db.blpop(self.key, timeout=timeout)
        else:
            item = self.__db.lpop(self.key)

        if item:
            item = item[1]
        return item

    def get_nowait(self):
        """Equivalent to get(False)."""
        return self.get(False)


################################################################################

def stress_test_write(N):
    rmq = RedisMessageQueue('mytest', host='127.0.0.1', port=6379)
    for i in xrange(0, N):
        rmq.put('hello,world!')

def stress_test_read(N):
    rmq = RedisMessageQueue('mytest', host='127.0.0.1', port=6379)
    for i in xrange(0, N):
        rmq.get()

def stress_test(N):
    rmq = RedisMessageQueue('mytest', host='127.0.0.1', port=6379)
    for i in xrange(0, N):
        rmq.put('hello,world!')
        rmq.get()

if __name__ == '__main__':
    import sys
    stress_test(int(sys.argv[1]))