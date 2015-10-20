#coding:utf8
import log
import traceback
class ServiceMsgHandler(object):
    msg_type = ''
    def __init__(self, msg_type):
        self.msg_type = msg_type

    def get_msg_type(self):
        return self.msg_type

    def __call__(self, src, data):
        pass

class ServiceMessageDispatcher():
    def __init__(self):
        self.handlers = {}

    def add_handler(self, handler):
        self.handlers[handler.get_msg_type()] = handler

    def __call__(self, src, data, mtype):
        if self.handlers.get(mtype, None) is None:
            log.error('type (%s) handler not found ' % mtype)
        else:
            try:
                self.handlers[mtype](src, data)
            except Exception, e:
                traceback.print_exc()
                log.error('exception ocuured when dealing msg for :%s' % str(e))
                pass



