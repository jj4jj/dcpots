#coding:utf8

#the mychart agent is a clien chart agent
#that can generate the chart data
from mycommdefs import log
from user.user_msg_handler import BaseUserAgentModule
import threading

class MyChartAgent(BaseUserAgentModule):

    def __init__(self, useragent):
        self.chart = {}
        self.state = 0
        self.updates_queue = []
        self.chart_msg_type = 'mycharts'
        self.chart_lock = threading.Lock()
        BaseUserAgentModule.__init__(self, useragent)

    #title , point sets
    """
    data.chart.xAxis =  [
                        {
                            type : 'category',
                            data : ["衬衫","羊毛衫","雪纺衫","裤子","高跟鞋","袜子"]
                        }
                    ],
    data.chart.yAxis =  [
                        {
                            type : 'value'
                        }
                    ],
    data.chart.legends = ['title1','title2'],
    data.chart.series =  [
                        {
                            "name":"销量",
                            "type":"bar",
                            "data":[5, 20, 40, 10, 10, 20]
                        }
                    ]
    """

    def _draw_charts(self, titles, names, types, datasets):
        self.chart['legends'] = titles
        self.chart['xAxis'] = map(
            lambda dataset: {'type': 'category',
                'data': [d[0] for d in dataset]}, datasets)

        self.chart['yAxis'] = [
            {'type': 'value'} for d in datasets
        ]
        series = []
        for i in xrange(len(datasets)):
            series.append({
                'name': names[i],
                'type': types[i],
                'data': [d[1] if len(d) <= 2 else d[1:] for d in datasets[i]]
            })
        self.chart['series'] = series

    def draw_line(self, title, name, dataset):
        self._draw_charts([title], [name], ['line'], [dataset])

    def draw_ohlc(self, title, name, dataset):
        self._draw_charts([title], [name], ['k'], [dataset])

    def draw_bar(self, title, name, dataset):
        self._draw_charts([title], [name], ['bar'], [dataset])

    def draw_scatter_point(self, title, name, dataset):
        self._draw_charts([title], [name], ['scatter'], [dataset])

    def data(self):
        return self.chart

    #send client all data
    def dump_push(self, chart_id):
        msg = {'type': 'render',
               'update': 'static',
               'chart': self.data(),
               'chart_id': chart_id}
        self.useragent.push_message(msg, self.chart_msg_type)


    def add_point(self, chart_id, point):
        self.updates_queue.append((chart_id, point,))

    def format_series(self, points):
        """
        // 动态数据接口 addData
        myChart.addData([
            [
                0,        // 系列索引
                Math.round(Math.random() * 1000), // 新增数据
                true,     // 新增数据是否从队列头部插入
                false     // 是否增加队列长度，false则自定删除原有数据，队头插入删队尾，队尾插入删队头
            ],
            [
                1,        // 系列索引
                lastData, // 新增数据
                false,    // 新增数据是否从队列头部插入
                false,    // 是否增加队列长度，false则自定删除原有数据，队头插入删队尾，队尾插入删队头
                axisData  // 坐标轴标签
            ]
        ]);
        """
        series = []
        for i in xrange(0, len(points)):
            series.append([i, points[i][1], False, False, points[i][0]])
        return {'series': series}

    #send client addition
    def update(self):
        try:
            point = self.updates_queue.pop(0)
        except IndexError:
            return
        chart_id = point[0]
        pt = point[1]
        update_data = self.format_series([pt])
        msg = {'type': 'render',
               'update': 'dynamic',
               'chart': update_data,
               'chart_id': chart_id}
        self.useragent.push_message(msg, self.chart_msg_type)


    #############################################################################
    def test(self, chart_id, shape):
        dataset1 = [[1, 2], [2, 3], [4, 5], ['hf', 24], ['new x', 3], ['x p', 20]]
        dataset2 = [[1, 2, 3, 4, 5], [2, 3, 5, 7, 8], [4, 5 , 4, 5, 6], ['hf', 24, 3, 5, 7], ['new x', 3, 3, 5, 7], ['x p', 20, 23, 45, 23]]
        dataset = dataset1
        if shape == 'ohlc':
            shape = 'k'
            dataset = dataset2
        self._draw_charts(['my title'], ['my name'], [shape], [dataset])
        self.dump_push(chart_id)


if __name__ == '__main__':
    mychart = MyChartAgent('')
    mychart.draw_line('hello', 'name', [[1, 2], [2, 3], [4, 5], ['hf', 24]])
    print mychart.data()

