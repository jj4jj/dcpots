#coding:utf8

import logging
import config
import sys


#caller info
#file,no,funcname
def get_function_frame_info(depth=1):
    frame = sys._getframe(depth)
    code = frame.f_code
    return (code.co_filename, code.co_firstlineno, code.co_name)

class MYLogger:
    def __init__(self, logfile):
        self.logger = logging.getLogger()
         #debug lv
        self.logger.setLevel(getattr(logging, config.LOG_LEVEL))
        rh = logging.handlers.TimedRotatingFileHandler(logfile, 'D')
        fm = logging.Formatter("%(asctime)s  %(levelname)s  %(message)s")
        rh.setFormatter(fm)
        self.logger.addHandler(rh)
    def get(self):
        return self.logger


_logger = None
def init_logger(logfile):
    global _logger
    if _logger is None:
        _logger = MYLogger(logfile)

def set_level(lv):
    #todo
    pass

def get_internal_logger():
    if _logger is None:
        return logging
    return _logger.get()

def debug(msg, layer=2):
    fn, no, fc = get_function_frame_info(layer)
    dmsg = '%s:%i (%s)-' % (fn, no, fc)
    get_internal_logger().debug(dmsg+msg)

def info(msg, layer=2):
    fn, no, fc = get_function_frame_info(layer)
    dmsg = '%s:%i (%s)-' % (fn, no, fc)
    get_internal_logger().info(dmsg+msg)


def warn(msg, layer=2):
    fn, no, fc = get_function_frame_info(layer)
    dmsg = '%s:%i (%s)-' % (fn, no, fc)
    get_internal_logger().warn(dmsg+msg)

def error(msg, layer=2):
    fn, no, fc = get_function_frame_info(layer)
    dmsg = '%s:%i (%s)-' % (fn, no, fc)
    get_internal_logger().error(dmsg+msg)

def fatal(msg, layer=2):
    fn, no, fc = get_function_frame_info(layer)
    dmsg = '%s:%i (%s)-' % (fn, no, fc)
    get_internal_logger().critical(dmsg+msg)
