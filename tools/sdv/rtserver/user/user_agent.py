import json
from mycommdefs import log
import traceback
import threading

class UserAgent():
    def __init__(self, uid, user):
        self.socket = None
        self.uid = uid
        self.user = user
        self.lock = threading.Lock()

    def __del__(self):
        log.debug('user = %d  agent del' % self.get_uid())

    def set_socket(self, sock):
        self.socket = sock

    def push_message(self, msg, mtype=''):
        #json
        try:
            emsg = {'type': mtype, 'data': msg}
            jmsg = json.dumps(emsg)
            self.socket.write_message(jmsg)
        except Exception, e:
            traceback.print_exc()
            log.error('write message error for :'+str(e))
            return False
        else:
            return True

    def get_uid(self):
        return self.uid


