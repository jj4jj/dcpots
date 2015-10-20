#coding:utf8
from user.auth import authenticate
from websocket_server import WebSocketMsgHandler
from user.user_agent import UserAgent
from .user_msg_handler import BaseUserMsgHandler
from .user_msg_handler import UserHeartBeatMsgHandler
from mycommdefs import log

class UserManager(WebSocketMsgHandler):
    _inst = None

    @staticmethod
    def instance():
        if UserManager._inst is None:
            UserManager._inst = UserManager()
        return UserManager._inst


    def __init__(self):
        WebSocketMsgHandler.__init__(self)
        self.uid_map_user = {}
        #userid is an user info primary key
        #uid is a object uid
        self.userid_map_users = {}
        self.msg_handlers = {}
        #default
        self.add_msg_handler(BaseUserMsgHandler())
        self.add_msg_handler(UserHeartBeatMsgHandler())

    def add_msg_handler(self,  msg_handler):
        self.msg_handlers[msg_handler.type()] = msg_handler

    def on_client_open(self, uid, socket):
        ug = self.get_user(uid)
        if ug is not None:
            ug.set_socket(socket)
        else:
            log.error('user uid = %d not found !' % uid)
            raise Exception('user not found')
        ################################################
        #user info
        #user info [instid(uid)]
        ug.push_message(ug.uid, 'userinfo')

    def on_client_handshake(self, uid, request):
        """
            when new client connect shake , authorization
        :param socket:
        :param request:
        :return:    when authorizing ok , return true, otherwise , return false
        """
        user = authenticate(request)
        if user:
            ug = UserAgent(uid, user)
            self.uid_map_user[uid] = ug
            if self.userid_map_users.get(user.id, None) is None:
                self.userid_map_users[user.id] = [ug]
            else:
                self.userid_map_users[user.id].append(ug)
            return True
        return False

    def on_client_message(self, uid, mtype, mdata):
        ug = self.get_user(uid)
        if ug is None:
            log.error('ug (%d) not found !' % uid)
        elif self.msg_handlers.get(mtype, None) is None:
            log.error('mtype (%s) not found!' % mtype)
        else:
            self.msg_handlers[mtype].on_useragent_msg(ug, mdata)

    def notify_handler_close(self, ug):
        for hdl in self.msg_handlers.values():
            hdl.on_useragent_remove(ug)

    def on_client_close(self, uid):
        WebSocketMsgHandler.on_client_close(self, uid)
        log.debug('uid close , remove user')
        ug = self.get_user(uid)
        if ug is not None:
            self.notify_handler_close(ug)
            del self.uid_map_user[uid]
            self.userid_map_users[ug.user.id].remove(ug)

    def get_user(self, uid):
        return self.uid_map_user.get(uid, None)

    def get_users(self, userid):
        return self.userid_map_users.get(userid, [])

#############################################################


def main():
    um1 = UserManager.instance()
    um2 = UserManager.instance()
    print id(um1)
    print id(um2)

if __name__ == '__main__':
    main()
