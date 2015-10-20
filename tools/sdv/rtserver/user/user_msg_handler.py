#coding:utf8
from mycommdefs import log

class BaseUserMsgHandler(object):
    mtype = ''
    def __init__(self, msgtype=''):
        self.mtype = msgtype

    def type(self):
        return self.mtype

    def on_useragent_remove(self, ug):
        pass

    def on_useragent_msg(self, useragent, data):
        log.debug('useragent:'+str(useragent) + 'msg type:(' + self.type()+') data:'+str(data))


class UserHeartBeatMsgHandler(BaseUserMsgHandler):
    def __init__(self):
        BaseUserMsgHandler.__init__(self, 'ping')

    def on_useragent_msg(self, useragent, data):
        useragent.push_message('pong', self.type())



class BaseUserAgentModule(object):
    def __init__(self, useragent):
        self.useragent = useragent

    def on_useragent_dettach(self):
        self.useragent = None
        pass
