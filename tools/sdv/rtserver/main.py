#coding:utf8

def bootstrap():
    from boot.init_env import init_env
    init_env()
bootstrap()

##################################################################
import sys
sys.path.append('../')
from websocket_server import WebSocketServer
from mycommdefs.message_dispatcher import ServiceMessageDispatcher
from mycommdefs.message_loop import MessageLoop
import mycommdefs.log as log
import config
from user.user_manager import UserManager


def rts_import(path):
    ps = path.split('.')
    np = '.'.join(ps[:-1])
    md = __import__(np)
    for p in ps[1:]:
        md = getattr(md, p)
    return md

def install_services(smd, services):
    for s in services:
        SVC = rts_import('service.'+s)
        smd.add_handler(SVC())


def install_user_msg_handlers(um, handlers):
    for h in handlers:
        HDL = rts_import('handlers.'+h)
        um.add_msg_handler(HDL())

def main():
    log.init_logger(config.LOG_FILE)
    log.set_level(config.LOG_LEVEL)
    ##############################################
    um = UserManager.instance()
    smd = ServiceMessageDispatcher()
    ################################################

    install_services(smd, config.INSTALL_SERVICES)
    install_user_msg_handlers(um, config.INSTALL_MSG_HANDLERS)
    ###################################################
    workers = []
    #websocket
    wss = WebSocketServer(config.WSS_URI, um, host=config.WSS_HOST, port=config.WSS_PORT)
    workers.append(wss)

    #message queue loop (service)
    mqs = MessageLoop(myname=config.REDIS_MQ_NAME, handler=smd)
    workers.append(mqs)

    ########################################################
    if config.DEBUG:
        print 'for debuging mock should be starting ...'
        pass

    #init_env
    for w in workers:
        #w.setDaemon(True)
        w.start()
    ###############################
    #setup signal quite
    def stop():
        #exit thread
        for w in workers:
            w.stop()

    #import signal
    #signal.signal(signal.SIGQUIT, stop)
    ################################
    for w in workers:
        w.join()


if __name__ == '__main__':
    main()
