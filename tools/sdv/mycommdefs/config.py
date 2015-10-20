#coding:utf8

#redis message queue
REDIS_MQ_HOST = '127.0.0.1'
REDIS_MQ_PORT = 6379
REDIS_MQ_DB = 0


###################################
#backend service name (addr)
REALTIME_REDIS_MQ_NAME = 'rts'
DJANGO_REDIS_MQ_NAME = 'django'
GATEWAY_REDIS_MQ_NAME = 'gateway'
QUOTE_SOURCE_MQ_NAME = 'quote_source'
##################################

#service msg type
#gateway<->django site
SITE_RSS_TASK_MSG_TYPE = 'site_rss_task'
QUOTE_CENTER_MSG_TYPE = 'quote'
PUSH_USER_MSG_TYPE = 'pushuser'


###################################
LOG_FILE = 'log/rts.log'
LOG_LEVEL = 'DEBUG'