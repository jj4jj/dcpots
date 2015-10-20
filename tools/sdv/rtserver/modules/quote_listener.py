#coding:utf8
from service.quote_center import get_quote_center
from user.user_msg_handler import BaseUserAgentModule
import datetime
import mycommdefs.log as log
import traceback

#create listener
class QuoteListenerMgr(BaseUserAgentModule):

    def __init__(self, useragent):
        BaseUserAgentModule.__init__(self, useragent)
        self.listeners = {}

    def listen(self, chart_id, quote, period):
        if self.listeners.get(chart_id, None) is None:
            self.listeners[chart_id] = QuoteListener(self.useragent)
        self.listeners[chart_id].start_listen(chart_id, quote, period)

    def unlisten(self, chart_id):
        if self.listeners.get(chart_id, None) is not None:
            self.listeners[chart_id].stop_listen()

    def on_useragent_dettach(self):
        for l in self.listeners.values():
            l.stop_listen()
        self.listeners.clear()
        BaseUserAgentModule.on_useragent_dettach(self)

    def render_static_quote(self, chart_id, quote, period):
        qc = get_quote_center()
        if qc:
            #get history data
            hq = qc.get_history_quote(quote)
            #compute avearge (index)
            cinfos = QuoteListenerMgr.compute_average(hq, period)
            dataset = []
            #formatter
            title = QuoteListenerMgr.get_title_from_quote(quote)
            name = QuoteListenerMgr.extract_request_quote(quote)['item']
            if cinfos:
                dataset = [info['point'] for info in cinfos]
            #draw
            self.useragent.chart_agent.draw_line(title, name, dataset)
            self.useragent.chart_agent.dump_push(chart_id)


    @staticmethod
    def compute_average(quoteinfos, period):
        #todo
        return quoteinfos

    @staticmethod
    def make_request_quote(exchange, item, date):
        return ':'.join((exchange, item, date))

    @staticmethod
    def get_title_from_quote(request_quote):
        attrs = request_quote.split(':')
        return attrs[1] + ' in ' + attrs[2]

    @staticmethod
    def extract_request_quote(request_quote):
        attrs = request_quote.split(':')
        return {'exchange': attrs[0], 'item': attrs[1], 'date': attrs[2]}


class QuoteListener():
    def __init__(self, useragent, chart_id=''):
        self.current_listen = None
        self.period = 1
        self.period_buffer_quote = []
        self.current_tick = 0
        self.last_update = None
        self.chart_id = chart_id
        self.useragent = useragent

    def get_current_average(self):
        #todo compute (no storage)
        #mid
        return self.period_buffer_quote[0]

    def get_init_average(self, infos):
        return QuoteListenerMgr.compute_average(infos, self.period)

    def on_update_quote(self, quote, info):
        #draw quote
        self.period_buffer_quote.append(info)
        now = datetime.datetime.now()
        if self.last_update is None:
            self.last_update = now
        self.current_tick += (now - self.last_update).total_seconds()*1000
        self.last_update = now
        ####################################
        if self.current_tick < self.period:
            return
        assert quote == self.current_listen
        #compute current quote
        cinfo = self.get_current_average()
        #clear
        self.current_tick = 0
        self.period_buffer_quote = []
        with self.useragent.lock:
            try:
                self.useragent.chart_agent.add_point(self.chart_id, cinfo['point'])
                self.useragent.chart_agent.update()
            except AttributeError, e:
                log.error('chart agent has been lost (%s)!' % str(e))
                pass
            except Exception, e:
                traceback.print_exc()
                log.error('exception raised for :' + str(e))
                raise

    def on_init_quote(self, quote, infos):
        #draw quote
        assert quote == self.current_listen
        cinfos = self.get_init_average(infos)
        dataset = []
        title = QuoteListenerMgr.get_title_from_quote(quote)
        name = QuoteListenerMgr.extract_request_quote(quote)['item']
        if cinfos:
            dataset = [info['point'] for info in cinfos]
        #draw line
        self.useragent.chart_agent.draw_line(title, name, dataset)
        self.useragent.chart_agent.dump_push(self.chart_id)

    def start_listen(self, chart_id, quote, period):
        self.stop_listen()
        qc = get_quote_center()
        self.current_listen = quote
        self.chart_id = chart_id
        self.period = period
        self.period_buffer_quote = []
        qc.subscribe(self, quote)


    def stop_listen(self):
        qc = get_quote_center()
        if self.current_listen:
            qc.unsubscribe(self, self.current_listen)
