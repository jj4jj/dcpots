#coding:utf8
import os
DEBUG = True

#the dir of mycommdefs parent
ROOT_PATH = '..'

#websocket server uri address
WSS_URI = 'ws://192.168.1.202/'
WSS_PORT = 8888
WSS_HOST = '127.0.0.1'
WSS_PREFIX_RGX = r'/rt/'


#redis message queue
REDIS_MQ_NAME = 'rts'
REDIS_MQ_HOST = '127.0.0.1'
REDIS_MQ_PORT = 6379
REDIS_MQ_DB = 0

###################################
INSTALL_MSG_HANDLERS = [
]

INSTALL_SERVICES = [
]

#
LOG_FILE = 'log/rts.log'
LOG_LEVEL = 'DEBUG'
