#coding:utf8

from threading import Thread
import tornado.ioloop
import tornado.web
import tornado.websocket
import config
import mycommdefs.log as log
import json

try:
    # py2
    from urllib.parse import urlparse
except ImportError:
    # py3
    from urlparse import urlparse


class WebSocketMsgHandler():
    def __init__(self):
        pass

    def on_client_open(self, uid, socket):
        log.debug('open uid:%d' % uid)

    def on_client_message(self, uid, mtype, mdata):
        log.debug('msg:'+mdata+' fd:%d ' % uid)

    def on_client_close(self, uid):
        log.debug('close uid %d:' % uid)

    def on_client_handshake(self, uid, request):
        log.debug('handshake  %d: ' % uid)
        return true


class WebSocketServer(Thread):
    def __init__(self, uri, um, host, port=8888):
        Thread.__init__(self)
        #############################################
        self.uri = uri
        self.um = um
        self.port = port
        self.host = host

        class _WebSocketServerHandlerProxy(tornado.websocket.WebSocketHandler):
            def open(self):
                um.on_client_open(id(self), self)
            def on_message(self, message):
                #log.debug('received msg:'+str(message))
                msg = json.loads(message)
                mtype = msg.get('type', None)
                mdata = msg.get('data', None)
                if mtype is None or \
                    mdata is None:
                    log.error('client message format is error !')
                else:
                    um.on_client_message(id(self), mtype, mdata)
            def on_close(self):
                um.on_client_close(id(self))
            def check_origin(self, origin):
                #self.request.connection.stream.socket._sock.fileno()
                #self.stream.socket._sock.fileno()
                authencated = um.on_client_handshake(id(self), self.request)
                parsed_origin = urlparse(origin)
                log.debug('origin : (%s) parsed origin : (%s) auth : (%s)' % (origin, parsed_origin.netloc, str(authencated)))
                return authencated

        self.app = tornado.web.Application([(config.WSS_PREFIX_RGX, _WebSocketServerHandlerProxy)])
        self.app.listen(address=host, port=port)
        self.io = tornado.ioloop.IOLoop.current()

    def stop(self):
        #stop io
        pass

    def run(self):
        self.io.start()

if __name__ == "__main__":
    ws = WebSocketServer('', WebSocketMsgHandler())
    ws.start()
    ws.join()

