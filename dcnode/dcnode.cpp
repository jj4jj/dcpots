#include "../base/logger.h"
#include "../base/msg_proto.hpp"
#include "../base/dcsmq.h"
#include "../base/dctcp.h"
#include "../base/logger.h"
#include "../base/dcutils.hpp"
#include "../base/dcshm.h"
#include "../base/profile.h"
#include "../base/dcobjects.hpp"

#include "../utility/drs/dcproto.h"

#include "proto/dcnode.pb.h"

#include "dcnode.h"

typedef msgproto_t<dcnode::MsgDCNode>	dcnode_msg_t;

struct dcnode_name_map_entry_t {
	char		name[32];
	uint64_t	id;
};

struct dcnode_name_map_t {
	int			nc;
	dcnode_name_map_entry_t	names[DCNODE_MAX_LOCAL_NODES_NUM];
	dcnode_name_map_t(){
		bzero(this, sizeof(*this));
	}
};

struct dcnode_id_t {
	const char * name;
	int			 fd;
	uint64_t	 smqid;
    //////////////////
    dcnode_id_t(){
        destruct();
    }
    void construct(const char * n, int sfd, uint64_t mqid){
        name = n;
        fd = sfd;
        smqid = mqid;
    }
    void destruct(){
        name = nullptr;
        fd = -1;
        smqid = 0;
    }
    bool invalid(){
        return name == nullptr && fd == -1 && smqid == 0;
    }

};
struct dcnode_addr_t {
    string msgq_sharekey;       //file path for machine sharing , push:/xxxx
    bool   msgq_push;           //client push mode as a client , not server
    string tcp_listen_addr;     //listen ip:port
    string tcp_parent_addr;     //parent ip:port
    void   init(){
        this->msgq_push = true;
        this->msgq_sharekey = "";
        this->tcp_listen_addr = "";
        this->tcp_parent_addr = "";
    }
    int    parse(const char * addrpatt){
        init();
        if (!addrpatt){ return -1; }
        //mode
#define PULL_MODE   "pull:"
#define PUSH_MODE   "push:"
#define PROXY_MODE  "proxy:"
        //proto
#define MSGQ_PROTO  "msgq://"
#define TCP_PROTO   "tcp://"
        enum dcnode_mode {
            DCNODE_MODE_PUSH,
            DCNODE_MODE_PULL,
            DCNODE_MODE_PROXY
        } mode = DCNODE_MODE_PUSH;
        const char * s_addr = addrpatt;
        if (strstr(addrpatt, PULL_MODE)){
            mode = DCNODE_MODE_PULL;
            msgq_push = false;
            s_addr += strlen(PULL_MODE);
        }
        else if (strstr(addrpatt, PUSH_MODE)){
            mode = DCNODE_MODE_PUSH;
            msgq_push = true;
            s_addr += strlen(PUSH_MODE);
        }
        else if (strstr(addrpatt, PROXY_MODE)){
            mode = DCNODE_MODE_PROXY;
            s_addr += strlen(PROXY_MODE);
        }
        else {
            GLOG_ERR("proto mode error addr pattern:%s", addrpatt);
            init();
            return -2;
        }
        ///////////////////////////////////////////////////////////////	
        std::vector<string> vs;
        dcsutil::strsplit(s_addr, "->", vs);
        if (vs[0].find(MSGQ_PROTO) == 0){
            msgq_sharekey = vs[0].substr(strlen(MSGQ_PROTO));
            if (mode == DCNODE_MODE_PROXY){
                msgq_push = false;
            }
        }
        else if (vs[0].find(TCP_PROTO) == 0){
            if (mode == DCNODE_MODE_PULL ||
                mode == DCNODE_MODE_PROXY){ //->a->b
                tcp_listen_addr = vs[0].substr(strlen(TCP_PROTO));
            }
            else {
                tcp_parent_addr = vs[0].substr(strlen(TCP_PROTO));
            }
        }
        else {
            GLOG_ERR("proto type error addr pattern:%s", addrpatt);
            init();
            return -3;
        }

        if (mode == DCNODE_MODE_PROXY){
            //should be a proxy
            if (vs.size() != 2 || (vs[0].find(MSGQ_PROTO) == 0 && vs[1].find(MSGQ_PROTO) == 0)){
                GLOG_ERR("dcnode not support 2msgq in proxy mode addr :%s vs size:%d !",
                    addrpatt, (int)vs.size());
                init();
                return -4;
            }
            if (vs[1].find(MSGQ_PROTO) == 0){
                msgq_sharekey = vs[0].substr(strlen(MSGQ_PROTO));
                msgq_push = true;
            }
            else if (vs[0].find(TCP_PROTO) == 0){
                tcp_parent_addr = vs[0].substr(strlen(TCP_PROTO));
            }
            else {
                GLOG_ERR("proto type error addr pattern:%s", addrpatt);
                init();
                return -5;
            }
        }
        return 0;
    }
};
struct dcnode_t {
	enum {
		DCNODE_ERROR = -1,	//ERROR , ABORT
		DCNODE_INIT = 0,
		DCNODE_CONNECTING = 1, //reconnect
		DCNODE_CONNECTED = 2,
		DCNODE_NAME_REG_ING = 3,
		DCNODE_NAME_REG = 4,
		DCNODE_READY = 5,
		DCNODE_ABORT = 42,	//ABORT
	};
	dcnode_config_t conf;
    dcnode_addr_t   addr;
	//tcp node
	dctcp_t	*		stcp;
	int				parentfd;
	//smq node
	dcsmq_t	*		smq;	//mq

	//as a client , fsm
	int			    fsm_state;
	int			    fsm_error;

	//local name map addr[id]
	dcnode_dispatcher_t		dispatcher;
	void *					dispatcher_ud;

	//timer cb
	std::multimap<uint64_t, uint64_t>						expiring_callbacks;
	std::unordered_map<uint64_t, dcnode_timer_callback_t>	timer_callbacks;
	std::unordered_map<uint64_t, uint64_t>					callback_periods;	//period call back timer

	//smq named nodes (agent)
	std::unordered_map<string, uint64_t>				named_smqid;
	std::unordered_map<uint64_t, time_t>				smq_hb_expire_time;	//children heart beat expire info
	dcnode_name_map_t									* smq_name_mapping_shm;	//shm

	//tcp named children (agent)
	std::unordered_map<string, int>						named_tcpfd;
	std::unordered_map<int, string>						tcpfd_map_name;
	std::unordered_map<int, time_t>						tcp_hb_expire_time;	//children heart beat expire info

	//parent heart beat info
	time_t												parent_hb_expire_time;

	//send buffer
	msg_buffer_t										send_buffer;
	dcnode_id_t											processing_id;
    //send queue
    struct msg_queue_t {
        size_t                                                                msg_size;
        dcsutil::object_queue_t< msg_buffer_t, DCNODE_MAX_LOCAL_NODES_NUM>      q;
        msg_queue_t() :msg_size(0){}
    };
    std::unordered_map<string, msg_queue_t>             send_queue;
    /////////////////////////////////////////////////////////////////////////////////////
	dcnode_t(){
		init();
	}
	void init(){
		smq = nullptr;
		stcp = nullptr;
		fsm_state = DCNODE_INIT;
		dispatcher = nullptr;
        addr.init();
		timer_callbacks.clear();
		callback_periods.clear();
		expiring_callbacks.clear();
		named_smqid.clear();
		named_tcpfd.clear();
		tcpfd_map_name.clear();
		smq_hb_expire_time.clear();
		tcp_hb_expire_time.clear();
		parent_hb_expire_time = 0;
		parentfd = -1;
		smq_name_mapping_shm = nullptr;
        //////////////////////////////////
        send_buffer.destroy();
        processing_id.destruct();
	}
};

const char * _dcnode_state_str(int st){
    static char state_buffer[12];
    switch (st){
    case dcnode_t::DCNODE_ERROR:	//ERROR , ABORT
        return "error";
    case dcnode_t::DCNODE_INIT:
        return "init";
    case dcnode_t::DCNODE_CONNECTING: //reconnect
        return "connecting";
    case dcnode_t::DCNODE_CONNECTED:
        return "connected";
    case dcnode_t::DCNODE_NAME_REG_ING:
        return "registering";
    case dcnode_t::DCNODE_NAME_REG:
        return "registered";
    case dcnode_t::DCNODE_READY:
        return "ready";
    case dcnode_t::DCNODE_ABORT:
        return "abort";
    default:
        snprintf(state_buffer, sizeof(state_buffer), "0X%X", st);
        return state_buffer;
    }
}
//multithread is harmless
std::atomic<uint32_t> timer_cb_seq(0);
static inline uint64_t  _insert_timer_callback(dcnode_t * dc , int32_t expired_ms, dcnode_timer_callback_t cb, bool repeat = false) {
	uint64_t cookie = dcsutil::time_unixtime_s();
	cookie <<= 24; //24+32 = 56;
    timer_cb_seq.fetch_add(1);
	cookie |= timer_cb_seq;
    //57
	cookie <<= 1;
	if (repeat)
		cookie |= 1;

	uint64_t expiretimems = expired_ms + dcsutil::time_unixtime_ms();
	dc->expiring_callbacks.insert(std::make_pair(expiretimems, cookie));
	dc->timer_callbacks[cookie] = cb;
	if (repeat)
		dc->callback_periods.insert(std::make_pair(cookie, expired_ms));
	GLOG_TRA("add callback timer delay:%dms ... cookie:%lu",expired_ms, cookie);
	return cookie;
}
static void	_remove_timer_callback(dcnode_t* dc, uint64_t cookie){
	if (cookie & 1)
	{
		dc->callback_periods.erase(cookie);
	}
	dc->timer_callbacks.erase(cookie);
	GLOG_TRA("remove dcnode callback timer ... cookie:%lu", cookie);
}

//leaf: smq client but no any tcp info
static bool _is_smq_leaf(dcnode_t* dc){
	if (dc->addr.tcp_listen_addr.empty() &&	//not tcp node
		dc->addr.tcp_parent_addr.empty()){
		if (!dc->addr.msgq_sharekey.empty() &&
			!dc->addr.msgq_push)	//but a smq server
		{
			return false;
		}
		return true;
	}
	return false;
}
static bool _is_root(dcnode_t* dc){
	return (dc->addr.tcp_parent_addr.empty() &&
			dc->addr.msgq_push == false);
}

static int	_fsm_check(dcnode_t * dc, bool checkforce);
static inline int _switch_dcnode_fsm(dcnode_t * dc, int state) {
    GLOG_IFO("switch dcnode fsm %s -> %s ", _dcnode_state_str(dc->fsm_state), _dcnode_state_str(state));
	dc->fsm_state = state;
	return _fsm_check(dc, true);
}
static int _fsm_register_name(dcnode_t * dc){
	GLOG_TRA("register panrent name with:%s", dc->conf.name.c_str());
	if (_is_root(dc)){
		return _switch_dcnode_fsm(dc, dcnode_t::DCNODE_NAME_REG);
	}
	dcnode_msg_t	msg;
	msg.set_type(dcnode::MSG_REG_NAME);
	msg.set_src(dc->conf.name);
	msg.mutable_ext()->set_unixtime(dcsutil::time_unixtime_s());
	if (!msg.Pack(dc->send_buffer)) {
		//pack error
		return -1;
	}
	dc->fsm_state = dcnode_t::DCNODE_NAME_REG_ING;
	if (_is_smq_leaf(dc)) {
		//to agent
		return dcsmq_send(dc->smq, dcsmq_session(dc->smq),
			dcsmq_msg_t(dc->send_buffer.buffer, dc->send_buffer.valid_size));
	}
	else {
		if (dc->addr.tcp_parent_addr.empty()){
			//no need
			return _switch_dcnode_fsm(dc, dcnode_t::DCNODE_NAME_REG);
		}
		//to parent
		return dctcp_send(dc->stcp, dc->parentfd,
			dctcp_msg_t(dc->send_buffer.buffer, dc->send_buffer.valid_size));
	}
	return 0;
}
static int _send_to_parent(dcnode_t * dc, dcnode_msg_t & dm) {
	dm.set_src(dc->conf.name);
	dm.mutable_ext()->set_unixtime(dcsutil::time_unixtime_s());
	if (!dm.Pack(dc->send_buffer)){
		//error pack
		return -1;
	}
	if (_is_smq_leaf(dc)){
		//to agent
		return dcsmq_send(dc->smq, dcsmq_session(dc->smq), dcsmq_msg_t(dc->send_buffer.buffer, dc->send_buffer.valid_size));
	}
	else if (!dc->addr.tcp_parent_addr.empty() && dc->parentfd != -1)
	{
		return dctcp_send(dc->stcp, dc->parentfd, dctcp_msg_t(dc->send_buffer.buffer, dc->send_buffer.valid_size));
	}
	return -2;
}
static  int _fsm_connect_parent(dcnode_t * dc){
	GLOG_TRA("connect parent for name registering ...");
	if (dc->addr.tcp_parent_addr.empty()){
		return _switch_dcnode_fsm(dc, dcnode_t::DCNODE_CONNECTED);
	}
	dc->fsm_state = dcnode_t::DCNODE_CONNECTING;
    return dctcp_connect(dc->stcp, dc->addr.tcp_parent_addr);
}
static dcnode_name_map_entry_t * _name_smq_entry_find(dcnode_t * dc, uint64_t session) {
	struct dcnode_name_map_entry_t dnme;
	dnme.id = session;
	typedef dcnode_name_map_entry_t entry_t;
	entry_t * beg = dc->smq_name_mapping_shm->names,
			* end = dc->smq_name_mapping_shm->names + dc->smq_name_mapping_shm->nc;
	auto comp = [](const entry_t & a, const entry_t & b){
		return a.id < b.id;
	};
	auto it = std::lower_bound(beg, end, dnme, comp);
	if (it != end && it->id == session){
		return it;
	}
	return nullptr;
}
static dcnode_name_map_entry_t * _name_smq_entry_find(dcnode_t * dc, const string & name) {
	auto it = dc->named_smqid.find(name);
	if (it != dc->named_smqid.end())
	{
		return _name_smq_entry_find(dc, it->second);
	}
	return nullptr;
}
//must has no name map
static dcnode_name_map_entry_t * _name_smq_entrty_alloc(dcnode_t * dc, const string & name) {
	if (dc->smq_name_mapping_shm->nc >= DCNODE_MAX_LOCAL_NODES_NUM){
		//error for maxx children
		return nullptr;
	}
	//just sorted
	static uint64_t s_smqpid_seq = 0;
	if (s_smqpid_seq == 0){
		s_smqpid_seq = dcsutil::time_unixtime_s();
		s_smqpid_seq <<= 18; //>2^18 not equal getpid()
	}
	s_smqpid_seq++;

	dcnode_name_map_entry_t * entry = &(dc->smq_name_mapping_shm->names[dc->smq_name_mapping_shm->nc]);
	dc->smq_name_mapping_shm->nc++;
	//map
	dc->named_smqid[name] = s_smqpid_seq;
	//entry
	entry->id = s_smqpid_seq;
	strncpy(entry->name, name.c_str(), sizeof(entry->name) - 1);

	return entry;
}
static int _smq_msgpid_close(dcnode_t * dc, uint64_t msgpid){
	//children name -> id map is const table , no changed
	if (msgpid == dcsmq_session(dc->smq)) { //parent
		assert(_is_smq_leaf(dc));
		GLOG_TRA("smq parent ndoe closed ....");
		dcsmq_set_session(dc->smq, getpid()); //
		return _switch_dcnode_fsm(dc, dcnode_t::DCNODE_INIT);
	}
	else {
		dc->smq_hb_expire_time.erase(msgpid);
		return 0;
	}
}
static int _stcp_sockfd_close(dcnode_t * dc, int fd) {
	auto it = dc->tcpfd_map_name.find(fd);
	if (it != dc->tcpfd_map_name.end())
	{
		dc->named_tcpfd.erase(it->second);
		dc->tcpfd_map_name.erase(it);
	}
	if (fd == dc->parentfd)
	{
		dc->parentfd = -1;
		GLOG_TRA("parent ndoe closed ....");
		return _switch_dcnode_fsm(dc, dcnode_t::DCNODE_INIT);
	}
	dc->tcp_hb_expire_time.erase(fd);
	return 0;
}

static void _fsm_update_hearbeat_timer(dcnode_t * dc, int sockfd, uint64_t smqsession){
	if (_is_smq_leaf(dc)) {
		dc->parent_hb_expire_time = dcsutil::time_unixtime_s() + dc->conf.max_children_heart_beat_expired;
	}
	else if (smqsession > 0) {
		if (dc->addr.msgq_push){
			if (dcsmq_session(dc->smq) == smqsession){
				dc->parent_hb_expire_time = dcsutil::time_unixtime_s() + dc->conf.max_children_heart_beat_expired;
			}
		}
		else{
			if (_name_smq_entry_find(dc, smqsession)){
				//named node
				dc->smq_hb_expire_time[smqsession] = dcsutil::time_unixtime_s() + dc->conf.max_children_heart_beat_expired;
			}
		}
	}
	else if (sockfd != -1){
		if (sockfd == dc->parentfd){
			dc->parent_hb_expire_time = dcsutil::time_unixtime_s() + dc->conf.max_children_heart_beat_expired;
		}
		else{
			dc->tcp_hb_expire_time[sockfd] = dcsutil::time_unixtime_s() + dc->conf.max_children_heart_beat_expired;
		}
	}
}
static void _node_expired(dcnode_t * dc, int sockfd , uint64_t msgpid = 0 ){
	//erorr log
	GLOG_TRA("node exipred sockfd:%d msgqpid:%lu",sockfd, msgpid);
	if (msgpid > 0){
		_smq_msgpid_close(dc, msgpid);
	}
	else if(sockfd != -1){
		//active close
		dctcp_close(dc->stcp, sockfd);
	}
	else{
		//parent tcp node
		_switch_dcnode_fsm(dc, dcnode_t::DCNODE_INIT);
	}
}

static void _fsm_start_heart_beat_timer(dcnode_t * dc){
	dcnode_timer_callback_t hb = [dc](){
		dcnode_msg_t dm;
		dm.set_type(dcnode::MSG_HEART_BEAT);
		dm.mutable_ext()->set_opt(dcnode::MSG_OPT_REQ);
		_send_to_parent(dc, dm);		
	};
	if (dc->conf.parent_heart_beat_gap > 0 && 
		(!dc->addr.tcp_parent_addr.empty() ||	//tcp client
		  (!dc->addr.msgq_sharekey.empty() && dc->addr.msgq_push )))		//mq client
	{
		GLOG_TRA("add parent heart-beat timer checker with:%ds", dc->conf.parent_heart_beat_gap);
		dc->parent_hb_expire_time = dcsutil::time_unixtime_s() + dc->conf.max_children_heart_beat_expired;
		_insert_timer_callback(dc, dc->conf.parent_heart_beat_gap * 1000, hb, true);
	}
	//start check all children timer
	dcnode_timer_callback_t hb_cheker = [dc](){
		time_t tNow = dcsutil::time_unixtime_s();
		if (dc->parent_hb_expire_time > 0 &&
			dc->parent_hb_expire_time < tNow){
			GLOG_TRA("parent node is expired ...");
			_node_expired(dc, dc->parentfd, dcsmq_session(dc->smq));
		}
		vector<uint64_t>	msgq_expired;
		vector<int>			tcp_expired;
		for (auto & it : dc->smq_hb_expire_time){
			if (it.second < tNow){
				msgq_expired.push_back(it.first);
			}
		}
		for (auto & it : dc->tcp_hb_expire_time){
			if (it.second < tNow){
				tcp_expired.push_back(it.first);
			}
		}

		for (auto it : msgq_expired){
			GLOG_TRA("smq children expired :%lu...",it);
			_node_expired(dc, -1, it);
		}
		for (auto it : tcp_expired){			
			GLOG_TRA("tcp children expired :%d...",it);
			_node_expired(dc, it);
		}

	};
	if (dc->conf.max_children_heart_beat_expired > 0 &&
		dc->conf.max_children_heart_beat_expired > dc->conf.parent_heart_beat_gap){
		GLOG_TRA("add heart-beat timer checker with:%ds", dc->conf.max_children_heart_beat_expired);
		_insert_timer_callback(dc, 1000 * dc->conf.max_children_heart_beat_expired, hb_cheker, true);
	}
}
static void _rebuild_smq_name_map(dcnode_t *dc){
	if (!dc->named_smqid.empty()){
		return;
	}
	dc->named_smqid.clear();
	dcnode_name_map_t * p = dc->smq_name_mapping_shm;
	for (auto i = 0; i < p->nc; i++){
		dc->named_smqid[p->names[i].name] = p->names[i].id;
	}
}
static int _name_smq_maping_shm_create(dcnode_t * dc, bool  owner){
	if (dc->smq_name_mapping_shm || !dc->smq){
		return 0;
	}
	dcshm_config_t shm_conf;
	shm_conf.attach = !owner;
	shm_conf.shm_path = dc->addr.msgq_sharekey;
	if (owner){
		shm_conf.shm_size = sizeof(dcnode_name_map_t);
	}
	else {
		shm_conf.shm_size = 0;
	}
	if (dcshm_create(shm_conf, (void **)&dc->smq_name_mapping_shm, shm_conf.attach)){
        GLOG_ERR("attach shm error attach:%d !", shm_conf.attach);
        return -1;
	}
	return 0;
}
static int	_fsm_check(dcnode_t * dc, bool checkforce){
	PROFILE_FUNC();
    UNUSED(checkforce);
	switch (dc->fsm_state)
	{
	case dcnode_t::DCNODE_INIT:
		//add timer hb
		return _fsm_connect_parent(dc);//connecting
	case dcnode_t::DCNODE_CONNECTED:
		return _fsm_register_name(dc);//registering
	case dcnode_t::DCNODE_NAME_REG: //-> ready
		if (dc->smq){ //register name succes , create the shm or attach		
			if (_name_smq_maping_shm_create(dc, false)){
				dc->fsm_state = dcnode_t::DCNODE_ABORT;
				return -1;
			}
			_rebuild_smq_name_map(dc);
		}
		dc->fsm_state = dcnode_t::DCNODE_READY;
		GLOG_TRA("fsm ready !");
		return 0;
	case dcnode_t::DCNODE_READY:
		//nothing report parent
		return 0;
    case dcnode_t::DCNODE_ABORT:
        //abort nothing todo
        return 0;
	default: return -1;
	}
}
static int _update_tcpfd_name(dcnode_t * dc, int sockfd, const string & name, bool force){
	auto it = dc->named_tcpfd.find(name);
	if (it != dc->named_tcpfd.end())
	{
		if (it->second != sockfd) {
			if (force){
				dc->tcpfd_map_name.erase(it->second);
				dc->tcp_hb_expire_time.erase(it->second);
				dc->named_tcpfd.erase(it);
			}
			else{
				GLOG_TRA("the request name:%s is collision [%d] ...",name.c_str(), it->second);
				return -1;
			}
		}
	}
	GLOG_TRA("update tcp name map %s->%d", name.c_str(), sockfd);
	dc->named_tcpfd[name] = sockfd;
	dc->tcpfd_map_name[sockfd] = name;
	return 0;
}
static int _response_msg(dcnode_t * dc, int sockfd, uint64_t msgqpid, dcnode_msg_t & dm, const dcnode_msg_t & dmsrc) {
	dm.set_src(dc->conf.name);
	dm.set_dst(dmsrc.src());
	dm.set_type(dmsrc.type());
	dm.mutable_ext()->set_unixtime(dcsutil::time_unixtime_s());
	dm.mutable_ext()->set_opt(dcnode::MSG_OPT_RSP);
	GLOG_TRA("response msg from:%s dst:%s", dmsrc.src().c_str(), dm.dst().c_str());
	bool ret = dm.Pack(dc->send_buffer);
	if (!ret){
		//error pack
		return -1;
	}
	if (sockfd != -1){
		return dctcp_send(dc->stcp, sockfd, dctcp_msg_t(dc->send_buffer.buffer, dc->send_buffer.valid_size));
	}
	else{
		assert(msgqpid > 0);
		return dcsmq_send(dc->smq, msgqpid, dcsmq_msg_t(dc->send_buffer.buffer, dc->send_buffer.valid_size));
	}
}

static int _fsm_abort(dcnode_t * dc, int error){
	dc->fsm_error = error;
    GLOG_IFO("dcnode abort error = %d", error);
	return _switch_dcnode_fsm(dc, dcnode_t::DCNODE_ABORT);
}
static inline int _children_num(dcnode_t * dc){
    return dc->named_smqid.size() + dc->named_tcpfd.size();
}

static int _fsm_update_name(dcnode_t * dc, int sockfd, uint64_t msgsrcid,const dcnode_msg_t & dm) {
	if (dm.ext().opt() == dcnode::MSG_OPT_REQ){
		//register children name
		int ret = 0;
		dcnode_msg_t dmrsp;
		uint64_t session = 0;
        string  error_msg;
		if (_children_num(dc) >= dc->conf.max_register_children){
			ret = -1;
            error_msg = "reached max register client , please retry later .";
		}
		else if (msgsrcid > 0){
			assert(sockfd == -1);
			dcnode_name_map_entry_t * entry = _name_smq_entry_find(dc, dm.src());
			if (!entry){//first , name is available
				entry = _name_smq_entrty_alloc(dc, dm.src());
				if (!entry){
                    ret = -1; 
                    error_msg = "allocate smq entry error , please retry later .";
                }
                else {
                    session = entry->id;
                }
			}
			else
			{
				session = entry->id;
				//name is busy , check it expired ?
				if (dm.reg_name().session() != entry->id){ //first time [init register]
					auto expireit = dc->smq_hb_expire_time.find(entry->id);
					if (expireit != dc->smq_hb_expire_time.end()){
						//collision , alived node
						string ftime;
						GLOG_TRA("request name:%s session:%lu is collision [%lu] expired time:%lds(%s) ...",
							dm.src().c_str(), dm.reg_name().session(), entry->id,
							expireit->second - dcsutil::time_unixtime_s(),
								dcsutil::strftime(ftime, expireit->second));
                        ret = -2;
                        error_msg = "request register name is collision ! check others name !";
					}
					else{
						//expired , name available
						dc->smq_hb_expire_time[entry->id] = dcsutil::time_unixtime_s() + dc->conf.max_children_heart_beat_expired;
					}
				}
			}
			if (ret == 0){
				GLOG_TRA("update smq name map %s->%lu [%lu]", dm.src().c_str(), session, msgsrcid);
			}
		}
		else {
			assert(sockfd != -1);
			//response to . no stat change - ready
			ret = _update_tcpfd_name(dc, sockfd, dm.src(), dm.reg_name().session() > 0);
		}
		dmrsp.mutable_reg_name()->set_ret(ret);
		dmrsp.mutable_reg_name()->set_session(session);
        if (ret){ dmrsp.mutable_reg_name()->set_error(error_msg); }
		return _response_msg(dc, sockfd, msgsrcid, dmrsp, dm);
	}
	else {
		assert(dm.dst() == dc->conf.name);
		if (dm.reg_name().ret() != 0) {
            GLOG_ERR("register name error :%s", dm.Debug());
			return _fsm_abort(dc, dm.reg_name().ret());
		}
		else {
			if (msgsrcid > 0){
				assert(sockfd == -1);
				//using sender id
				assert(dm.reg_name().session() > 0);
				dcsmq_set_session(dc->smq, dm.reg_name().session());
			}
			else {
				assert(sockfd != -1);
				_update_tcpfd_name(dc, sockfd, dm.src(), true);
			}
			return _switch_dcnode_fsm(dc, dcnode_t::DCNODE_NAME_REG);
		}
	}
}

//node handle msg
static int _handle_msg(dcnode_t * dc, const dcnode_msg_t & dm, int sockfd, uint64_t msgqpid){
	PROFILE_FUNC();
	if (dm.ext().unixtime() > 0 && dm.ext().unixtime() + dc->conf.max_msg_expired_time < dcsutil::time_unixtime_s() ) {
		//expired msg
		GLOG_TRA("expired msg :%s",dm.Debug());
		return -1;
	}
	_fsm_update_hearbeat_timer(dc, sockfd, msgqpid);
	dcnode_msg_t rspmsg;
	switch (dm.type())
	{
	case dcnode::MSG_DATA:
		//to up callback 
		GLOG_TRA("dispatch msg :%s->:%s size:%d",dm.src().c_str(), dm.dst().c_str(), dm.PackSize());
		return dc->dispatcher(dc->dispatcher_ud, dm.src().c_str(), msg_buffer_t(dm.msg_data().data(), dm.msg_data().length()));
	case dcnode::MSG_REG_NAME:
		//insert tcp src -> map name
		return _fsm_update_name(dc, sockfd, msgqpid, dm);
	case dcnode::MSG_HEART_BEAT:
		if (dm.ext().opt() == dcnode::MSG_OPT_REQ) {
			_response_msg(dc, sockfd, msgqpid, rspmsg, dm);
		}
		break;
	case dcnode::MSG_RPC:
		break;
	default:
		//error type
		return -1;
		break;
	}
	return 0;
}

static int _forward_msg(dcnode_t * dc, int sockfd, const char * buff, int buff_sz, const string & dst) {
	if (_is_smq_leaf(dc)){
		//to msgq
		GLOG_TRA("foward msg to :[%s] with smq to parent", dst.c_str());
		auto entry = _name_smq_entry_find(dc, dst);
		if (!entry){ //to parent 
			return dcsmq_send(dc->smq, dcsmq_session(dc->smq), dcsmq_msg_t((char*)buff, buff_sz));
		}
		else { //fast send to others
			return dcsmq_put(dc->smq, entry->id, dcsmq_msg_t((char*)buff, buff_sz));
		}
	}

	//check children name , if not found , send to parent except src
	auto it = dc->named_smqid.find(dst);
	if (it != dc->named_smqid.end())
	{
		GLOG_TRA("foward msg to :%s with smq [found dst]", dst.c_str());
		return dcsmq_send(dc->smq, it->second, dcsmq_msg_t((char*)buff, buff_sz));
	}
	auto tit = dc->named_tcpfd.find(dst);
	if (tit != dc->named_tcpfd.end())
	{
		GLOG_TRA("foward msg to :%s with tcp [found dst]", dst.c_str());
		return dctcp_send(dc->stcp, tit->second, dctcp_msg_t(buff, buff_sz));
	}

	//to up layer 
	if (sockfd != dc->parentfd && dc->parentfd != -1)
	{
		GLOG_TRA("foward msg to :%s not found name , so report to up layer with tcp:%d...",
			dst.c_str(), dc->parentfd);
		dctcp_send(dc->stcp, dc->parentfd, dctcp_msg_t(buff, buff_sz));
	}
	//all children
	for (auto it = dc->named_tcpfd.begin();
		it != dc->named_tcpfd.end(); it++)
	{
		if (it->second == sockfd ||
			it->second == dc->parentfd)
		{
			continue;
		}
		GLOG_TRA("foward msg to :%s not found name , so report to lower layer with tcp:%d ...",
			dst.c_str() , it->second);
		dctcp_send(dc->stcp, it->second, dctcp_msg_t(buff, buff_sz));
	}
	return 0;
}
static int _msg_cb(dcnode_t * dc, int sockfd, uint64_t msgqpid, const char * buff, int buff_sz){
	dcnode_msg_t dm;
	PROFILE_FUNC();
	GLOG_TRA("dcnode recv msg fd:%d msgqpid:%lu buff sz:%d",
		sockfd, msgqpid, buff_sz);

	if (!dm.Unpack(buff, buff_sz)) {
		//error for decode
		return -1;
	}
	//update processing
	if (dm.dst().length() == 0 ||
		dm.dst() == dc->conf.name) {
		//to me , handle it
        int ret = 0;
        dc->processing_id.construct(dm.src().c_str(), sockfd, msgqpid);
        ret = _handle_msg(dc, dm, sockfd, msgqpid);
        dc->processing_id.destruct();
        return ret;
	}
	else {
		return _forward_msg(dc, sockfd, buff, buff_sz, dm.dst());
	}
}
static int _smq_cb(dcsmq_t * smq, uint64_t src, const dcsmq_msg_t & msg, void * ud) {
    UNUSED(smq);
	dcnode_t * dc = (dcnode_t*)ud;
	return _msg_cb(dc, -1, src, msg.buffer, msg.sz);
}

//server
static int _stcp_cb(dctcp_t* stcp, const dctcp_event_t & ev, void * ud) {
    UNUSED(stcp);
    dcnode_t * dc = (dcnode_t*)ud;
	switch (ev.type)
	{
	case dctcp_event_type::DCTCP_NEW_CONNX:
		//new connection no need build the map , but in reg name
		break;
	case dctcp_event_type::DCTCP_CONNECTED:
		//connected
		dc->parentfd = ev.fd;
		return _switch_dcnode_fsm(dc, dcnode_t::DCNODE_CONNECTED);
	case dctcp_event_type::DCTCP_READ:
		//
		return _msg_cb(dc, ev.fd, 0, ev.msg->buff, ev.msg->buff_sz);
	case dctcp_event_type::DCTCP_CLOSED:
		//error record
		return _stcp_sockfd_close(dc, ev.fd);
	default: return -1;
	}
	return 0;
}
dcnode_t* dcnode_create(const dcnode_config_t & conf) {
    dcnode_addr_t addr;
    int ret = addr.parse(conf.addr.c_str());
    if (ret){ //address error
        GLOG_ERR("dcnode addr check error :%d!", ret);
		return nullptr;
	}
	dcnode_t * n = new dcnode_t();
    if (!n){//memerror
        GLOG_ERR("dcnode allocate memory !");
        return nullptr;
    }
	n->conf = conf;
    n->addr = addr;
	if (!_is_smq_leaf(n))
	{
		dctcp_config_t sc;	//tcp node
		sc.max_recv_buff = conf.max_channel_buff_size;
		sc.max_send_buff = conf.max_channel_buff_size;
		sc.max_tcp_send_buff_size = conf.max_channel_buff_size;
		sc.max_tcp_recv_buff_size = conf.max_channel_buff_size;
		n->stcp = dctcp_create(sc);
		if (!n->stcp) {
			dcnode_destroy(n); 
            return nullptr;
		}
		if (!addr.tcp_listen_addr.empty()){
            int fd = dctcp_listen(n->stcp, addr.tcp_listen_addr);
			if (fd < 0) {
				dcnode_destroy(n); 
                return nullptr;
			}
		}
		dctcp_event_cb(n->stcp, _stcp_cb, n);
	}
	if (!addr.msgq_sharekey.empty()){
		dcsmq_config_t smc;
		smc.keypath = addr.msgq_sharekey;
		smc.msg_buffsz = conf.max_channel_buff_size;
		smc.passive = !_is_smq_leaf(n);

		n->smq = dcsmq_create(smc);
		if (!n->smq) {
			GLOG_ERR("dcnode create msg q error !");;
			dcnode_destroy(n);
			return nullptr;
		}
		dcsmq_msg_cb(n->smq, _smq_cb, n);
		if (smc.passive){
			if (_name_smq_maping_shm_create(n, true)){
				GLOG_ERR("dcnode.smq create shm error !");
				dcnode_destroy(n);
				return nullptr;
			}				
            _rebuild_smq_name_map(n);
		}
		else {
			dcsmq_set_session(n->smq, getpid());	//default session
		}
	}
	n->send_buffer.create(conf.max_channel_buff_size);
	_fsm_start_heart_beat_timer(n);
	return n;
}
static inline void _dcnode_clear_send_queue(dcnode_t * dc){
    for (auto it : dc->send_queue){
        while (!it.second.q.empty()){
            auto buff = it.second.q.front();
            buff->destroy();
            it.second.q.pop();
        }
    }
    dc->send_queue.clear();
}
void      dcnode_destroy(dcnode_t* dc){
	if (dc->stcp){
		dctcp_destroy(dc->stcp);
		dc->stcp = nullptr;
	}
	if (dc->smq){
		dcsmq_destroy(dc->smq);
		dc->smq = nullptr;
	}
	if (dc->smq_name_mapping_shm){
		dcshm_destroy(dc->smq_name_mapping_shm);
	}
    //=================================
    _dcnode_clear_send_queue(dc);

    dc->init();
	delete dc;
}
static	void _check_timer_callback(dcnode_t * dc){
	PROFILE_FUNC();
	uint64_t currentms = dcsutil::time_unixtime_ms();
	std::vector<std::pair<uint64_t, uint64_t> >		addagin;
	for (auto it = dc->expiring_callbacks.begin(); it != dc->expiring_callbacks.end();)
	{
		if (it->first > currentms){ break; }
		auto previt = it++;
		uint64_t cookie = previt->second;
		auto funcit = dc->timer_callbacks.find(cookie);
		if (funcit != dc->timer_callbacks.end())
		{
			funcit->second();//call back
			if (cookie & 1)
			{
				//repeat
				auto periodit = dc->callback_periods.find(cookie);
				if (periodit != dc->callback_periods.end())
				{
					//add agin
					addagin.push_back(std::make_pair(currentms + periodit->second, cookie));
				}
			}
			else
			{
				dc->timer_callbacks.erase(funcit);
			}
		}
		dc->expiring_callbacks.erase(previt);
	}
	for (auto pair : addagin)
	{
		dc->expiring_callbacks.insert(pair);
	}
}
static  bool _name_exists(dcnode_t * dc, const char * name){
    if (!name || !name[0]){
        return false;
    }
    if (dc->named_smqid.find(name) != dc->named_smqid.end()){
        return true;
    }
    if (dc->named_tcpfd.find(name) != dc->named_tcpfd.end()){
        return true;
    }
    return false;
}
static inline void _check_send_queue(dcnode_t *dc, const string & dst, dcnode_t::msg_queue_t & queue){
    if (dc->fsm_state != dcnode_t::DCNODE_READY &&
        !_name_exists(dc, dst.c_str())) {
        //error not ready (name not reg) 
        return;
    }
    while (!queue.q.empty()){
        auto front = queue.q.front();
        assert(front != queue.q.null());
        int ret = _forward_msg(dc, -1, front->buffer, front->valid_size, dst);
        if (ret == 0){
            queue.msg_size -= front->valid_size;
            queue.q.pop();
        }
        else {
            break;
        }
    }
}
static inline void _check_send_queue(dcnode_t *dc){
    for (auto & q : dc->send_queue){
        _check_send_queue(dc, q.first, q.second);
    }
}
static inline int _send_msg(dcnode_t *dc, const std::string & dst, const dcnode_msg_t & dm){
    //immediately send out
    if (dst.empty()){ //to parent immediately , no queue
        if (!dm.Pack(dc->send_buffer)){
            GLOG_ERR("dcnode pack msg error !");
            return -1;
        }
        return _forward_msg(dc, -1, dc->send_buffer.buffer, dc->send_buffer.valid_size, dst);
    }
    //
    auto it = dc->send_queue.find(dst);
    if (it == dc->send_queue.end()){ //create one 
        dc->send_queue[dst] = dcnode_t::msg_queue_t();
    }
    //push msg
    auto & queue = dc->send_queue[dst];
    _check_send_queue(dc, dst, queue);
    if ((int)queue.q.size() >= dc->conf.max_send_queue_size){
        GLOG_ERR("reached queue max size dst:%s! queue size:%zu msg size:%zu",
            dst.c_str(), queue.q.size(), queue.msg_size);
        return E_DCNODE_SEND_FULL;
    }
    auto pq = queue.q.push();
    if (queue.q.null() == pq){
        GLOG_ERR("allocate queue error dst:%s queue size:%zu msg size:%zu",
            dst.c_str(), queue.q.size(), queue.msg_size);
        return E_DCNODE_SEND_FULL;
    }
    else { //push
        pq->reserve(dm.ByteSize());
        if (!dm.Pack(*pq)){
            //error
            GLOG_ERR("dcnode pack msg error dst:%s msg:%s!", dst.c_str(), dm.Debug());
            queue.q.pop();
            return -3;
        }
        dc->send_queue[dst].msg_size += dm.ByteSize();
    }
    _check_send_queue(dc);
    return 0;
}
int      dcnode_send(dcnode_t* dc, const std::string & dst, const char * buff, int sz){
    PROFILE_FUNC();
    if (dc->fsm_state == dcnode_t::DCNODE_ABORT){
        GLOG_ERR("dcnode is abort , can not send msg !");
        return -1;
    }
    if (!dc->conf.durable){ //not durable
        if (dc->fsm_state != dcnode_t::DCNODE_READY &&
            !_name_exists(dc, dst.c_str())) {
            //error not ready (name not reg) 
            GLOG_ERR("dcnode state:%d not ready can't send msg(size:%d) and name not exist !",
                dc->fsm_state, sz);
            return -1;
        }
    }
    dcnode_msg_t dm;
    dm.set_src(dc->conf.name);
    dm.set_dst(dst);
    dm.set_type(dcnode::MSG_DATA);
    dm.set_msg_data(buff, sz);
    dm.mutable_ext()->set_unixtime(dcsutil::time_unixtime_s());
    return _send_msg(dc, dst, dm);
}
int      dcnode_update(dcnode_t* dc, int timout_us) {
	PROFILE_FUNC();
	if (dc->fsm_state == dcnode_t::DCNODE_ABORT){
		return 0;
	}
	//check cb
	_check_timer_callback(dc);

	_fsm_check(dc, false);
	int n = 0;
	if (dc->smq) {
		n += dcsmq_poll(dc->smq, timout_us);
	}
	if (dc->stcp) {
		n += dctcp_poll(dc->stcp, timout_us);
	}
	_check_timer_callback(dc);
    //check send queue
    _check_send_queue(dc);
	return n;
}

//typedef int(*dcnode_dispatcher_t)(const char* src, const dcnode_msg_t & msg);
void      dcnode_set_dispatcher(dcnode_t* dc, dcnode_dispatcher_t dspatch, void * ud) {
	dc->dispatcher = dspatch;
	dc->dispatcher_ud = ud;
}
uint64_t  dcnode_timer_add(dcnode_t * dc, int delayms, dcnode_timer_callback_t cb, bool repeat) {
	return	_insert_timer_callback(dc, delayms, cb, repeat);
}
void	  dcnode_timer_cancel(dcnode_t * dc, uint64_t cookie) {
	_remove_timer_callback(dc, cookie);
}
int      dcnode_reply(dcnode_t* dc, const char * buff, int sz){
    dcnode_id_t * nodeid = &dc->processing_id;
    if (nodeid->invalid()){
        GLOG_ERR("not found the processsing msg env !");
        return -1;
    }
    ////////////////////////////////////////////////////////////////////////
	dcnode_msg_t dm;
	dm.set_src(dc->conf.name);
	if (nodeid->name){
		dm.set_dst(nodeid->name);
	}
	dm.set_type(dcnode::MSG_DATA);
	dm.set_msg_data(buff, sz);
	dm.mutable_ext()->set_unixtime(dcsutil::time_unixtime_s());
	if (!dm.Pack(dc->send_buffer)){
		//error
		GLOG_ERR("dcnode pack msg error !");
		return -1;
	}
	if (nodeid->smqid > 0){
		return dcsmq_send(dc->smq, nodeid->smqid, dcsmq_msg_t(dc->send_buffer.buffer, dc->send_buffer.valid_size));
	}
	else {
		return dctcp_send(dc->stcp, nodeid->fd, dctcp_msg_t(dc->send_buffer.buffer, dc->send_buffer.valid_size));
	}
}
static inline void   _dcnode_dump_send_queue(dcnode_t * dc, dcnode::DCNodeDumpSendQueue * sendq){
    auto dumpq = dc->send_queue;
    for (auto it : dumpq){
        auto q = sendq->add_send_queue();
        q->set_dst(it.first);
        while (!it.second.q.empty()){
            auto buff = it.second.q.front();
            /////////////////////////////////
            q->add_queue((*buff).buffer, (*buff).valid_size);
            /////////////////////////////////
            it.second.q.pop();
        }
    }
    dumpq.clear();
}
static inline const char * _dcnode_dump_format_type(dcnode_dump_format_type dtype){
    static const char * s_type_name[] = { "bin", "text", "json", "xml" };
    return s_type_name[dtype];
}
int       dcnode_dump(dcnode_t * dc, const char * fname, dcnode_dump_format_type dtype){
    msgproto_t<dcnode::DCNodeDump> dump;
    //file format protobuf
    dump.set_name(dc->conf.name);
    dump.set_format(_dcnode_dump_format_type(dtype));
    dcsutil::strftime(*dump.mutable_time());//now
    {//begin dump
        _dcnode_dump_send_queue(dc, dump.mutable_send_queue());
    }
    msg_buffer_t filebuffer;
    string writesbuffer;
    if (dtype == DCNODE_DUMP_BIN){
        filebuffer.create(dump.ByteSize());
        if (dump.Pack(filebuffer)){
            GLOG_ERR("dump file pack error !");
            filebuffer.destroy();
            return -1;
        }
    }
    else if (dtype == DCNODE_DUMP_TEXT){
        writesbuffer = dump.DebugString();
        filebuffer.buffer = (char*)writesbuffer.data();
        filebuffer.valid_size = writesbuffer.length();
    }
    else if (dtype == DCNODE_DUMP_JSON){
        return dcsutil::protobuf_msg_to_json_file(dump, fname);
    }
    else if (dtype == DCNODE_DUMP_XML){
        return dcsutil::protobuf_msg_to_xml_file(dump, fname);
    }
    int wsz = dcsutil::writefile(fname, filebuffer.buffer, filebuffer.valid_size);
    if (wsz != filebuffer.valid_size){
        GLOG_ERR("dump file error  ret: %d", wsz);
        filebuffer.destroy();
        return -2;
    }
    filebuffer.destroy();
    return 0;
}


void	  dcnode_abort(dcnode_t * dc){
	_fsm_abort(dc, 0);
    dcnode_dump(dc, dc->conf.dumpfile.c_str(), DCNODE_DUMP_BIN);
}
//-1:stop ; 0:not ready;1:ready
int		  dcnode_ready(dcnode_t *  dc){
	if (dc->fsm_state == dcnode_t::DCNODE_READY){
		return 1;
	}
	else if (dc->fsm_state != dcnode_t::DCNODE_ABORT){
		return 0;
	}
	else {
		return -1;
	}
}


