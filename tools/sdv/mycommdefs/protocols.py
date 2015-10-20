
PACK_FORMAT_ARRAY = 1
PACK_FORMAT_DICT = 2
class AbstractMsg():
    def __init__(self, format=PACK_FORMAT_ARRAY):
        self._format = format

    def _pack_array(self):
        ret = []
        keys = filter(lambda k: not (k.startswith('_')), self.__dict__.keys())
        skeys = sorted(keys)
        for k in skeys:
            ret.append(self.__dict__[k])
        return ret

    def _pack_dict(self):
        keys = filter(lambda k: not (k.startswith('_')), self.__dict__.keys())
        ret = {}
        for k in keys:
            ret[k] = self.__dict__[k]
        return ret

    def _unpack_array(self, msg):
        keys = filter(lambda k: not (k.startswith('_')), self.__dict__.keys())
        skeys = sorted(keys)
        i = 0
        for k in skeys:
            self.__dict__[k] = msg[i]
            i += 1
        return self

    def _unpack_dict(self, msg):
        self.__dict__.update(msg)
        return self


    def pack(self):
        if self._format == PACK_FORMAT_ARRAY:
            return self._pack_array()
        elif self._format == PACK_FORMAT_DICT:
            return self._pack_dict()
        raise NotImplementedError()

    def unpack(self, msg):
        if self._format == PACK_FORMAT_ARRAY:
            return self._unpack_array(msg)
        elif self._format == PACK_FORMAT_DICT:
            return self._unpack_dict(msg)
        raise NotImplementedError()

#######################################################################33
#puse user msg


class PushUserMsg(AbstractMsg):
    def __init__(self, uid=0, uinst=0, body=None):
        self.user_id = uid
        self.user_agent_inst = uinst
        self.body = body
        AbstractMsg.__init__(self)

    def pack(self):
        return [self.user_id, self.user_agent_inst, self.body]

    def unpack(self, msg):
        self.user_id = msg[0]
        self.user_agent_inst = msg[1]
        self.body = msg[2]
        return self


####################################################################
#GATEWAY<->DJANGO MSG


class DjangoGatewayMsg(AbstractMsg):
    def __init__(self, cmd = '', task={}):
        self.cmd = cmd
        self.task = task
        AbstractMsg.__init__(self)



class QuoteInfoItem(AbstractMsg):
    def __init__(self, quote='', info=None):
        self.quote = quote
        self.info = info
        AbstractMsg.__init__(self)

################################testing######################################
if __name__ == '__main__':
    dgm = DjangoGatewayMsg(2,45)
    print dgm.pack()

    dgm._format=2
    print dgm.pack()

    dgm._format=1
    dgm.unpack([35,36])
    print dgm.pack()

    dgm._format=2
    dgm.unpack({'cmd': 542, 'task': 45465})
    print dgm.pack()
