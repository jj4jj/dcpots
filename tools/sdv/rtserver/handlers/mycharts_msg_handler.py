#coding:utf8

#the mychart agent is a clien chart agent
#that can generate the chart data
from user.user_msg_handler import BaseUserMsgHandler
from modules.mycharts_agent import MyChartAgent
from service.quote_center import check_quote_item_valid
import mycommdefs.log as log
from modules.quote_listener import QuoteListenerMgr

class MyChartMsgHandler(BaseUserMsgHandler):

    def __init__(self):
        BaseUserMsgHandler.__init__(self, 'mycharts')

    def on_useragent_remove(self, ug):
        #must delete by order
        #reverse with create order
        with ug.lock:
            ug.remove_part('quote_listeners')
            ug.remove_part('chart_agent')

    def on_chart_quote_msg(self, useragent, chart_id, data):
        exchange = data['exchange']
        item = data['item']
        update = data['update']
        period = int(data['period'])
        date = data['date']
        #todo check words valid

        if not check_quote_item_valid(item):
            log.error('client item = %s is invalid' % item)
            return
        if update not in ('static', 'dynamic'):
            log.error('client update = %s is invalid' % item)
            return
        if period <= 0 or period >= 6000:
            period = 1

        ####################################################################
        req_quote = QuoteListenerMgr.make_request_quote(exchange, item, date)
        ################## guarentee useragent has a quote listener ########
        if not hasattr(useragent, 'quote_listeners'):
            useragent.quote_listeners = QuoteListenerMgr(useragent)

        ####################################################################
        if update == 'static':
            useragent.quote_listeners.render_static_quote(chart_id, req_quote, period)

        elif update == 'dynamic':
            useragent.quote_listeners.listen(chart_id, req_quote, period)


    def on_useragent_msg(self, useragent, data):
        log.debug('useragent:'+str(useragent) +'msg type:('+self.type()+') data:'+str(data))
        if not hasattr(useragent, 'chart_agent'):
            useragent.chart_agent = MyChartAgent(useragent)
        #####################################################################################
        #data type is quote
        chart_id = data['chart_id']
        if data['type'] == 'test':
            useragent.push_message('hahahaha, hello!', self.type())
            return
        if data['type'] == 'test_chart':
            shape = 'line'
            if data.get('shape', None) is not None:
                shape = data['shape']
            useragent.chart_agent.test(chart_id, shape)
            return
        #####################################################################################

        if data['type'] == 'quote':
            self.on_chart_quote_msg(useragent, chart_id, data)

