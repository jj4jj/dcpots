#-*-coding:utf8-*-
import threading

from .message_queue import ServiceMessageQueue


class MessageLoop(threading.Thread):
    def __init__(self, handler, myname, thread_name='msg_loop'):
        self.mq = ServiceMessageQueue(myname)
        self.handler = handler
        self.my_q_name = myname
        self._quit = False
        threading.Thread.__init__(self, name=thread_name)

    def put_message(self, mtype, msg):
        self.mq.put_message(mtype, msg, self.my_q_name)

    def stop(self):
        self._quit = True

    def get_mq(self):
        return self.mq

    def run(self):
        while not self._quit:
            msg = self.mq.get_message()
            self.handler(ServiceMessageQueue.get_msg_src(msg),
                         ServiceMessageQueue.get_msg_data(msg),
                         ServiceMessageQueue.get_msg_type(msg))

