#-*-coding:utf8-*-
from mycommdefs import log
from mycommdefs.message_dispatcher import ServiceMsgHandler
from mycommdefs.config import QUOTE_CENTER_MSG_TYPE
from mycommdefs.protocols import QuoteInfoItem

class _InternalQuoteCenter(ServiceMsgHandler):
    def __init__(self):
        self.listen_quote_map = {}
        self.quote_history = {}
        ServiceMsgHandler.__init__(self, QUOTE_CENTER_MSG_TYPE)

    def add_quote_srouce(self, src, formatter):
        #subscribe quote source data
        pass

    def start(self):
        #dummy
        pass

    def get_history_quote(self, quote):
        return self.quote_history.get(quote, None)

    def feed_quote(self, quote, info):
        listeners = self.listen_quote_map.get(quote, None)
        if listeners:
            for listener in listeners:
                listener.on_update_quote(quote, info)

    def subscribe(self, quote_listener, quote):
        if self.listen_quote_map.get(quote, None) is None:
            self.listen_quote_map[quote] = []
        self.listen_quote_map[quote].append(quote_listener)
        if self.quote_history.get(quote, None) is None:
            quote_listener.on_init_quote(quote, [])
        else:
            quote_listener.on_init_quote(quote, self.quote_history[quote])

    def unsubscribe(self, quote_listener, quote):
        if self.listen_quote_map.get(quote, None) is None:
            return
        log.debug('unsubscribe user listener chart_id:(%s)' % quote_listener.chart_id)
        self.listen_quote_map[quote].remove(quote_listener)

    #when quote info message coming
    def __call__(self, src, data):
        #quote center
        #log.debug('recv quote info message : (%s) from (%s) ' % (str(data), src))
        #data -> [{quote, info}, ]
        for qii in data:
            qo = QuoteInfoItem().unpack(qii)
            self.feed_quote(qo.quote, qo.info)
            ###############################################
            if self.quote_history.get(qo.quote, None) is None:
                self.quote_history[qo.quote] = []
            self.quote_history[qo.quote].append(qo.info)

def check_quote_item_valid(item):
    return True


###############################################################################
_quote_center = _InternalQuoteCenter()
def get_quote_center():
    global _quote_center
    return _quote_center