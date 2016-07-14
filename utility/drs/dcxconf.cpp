
#include "google/protobuf/message.h"
#include "../../base/stdinc.h"
#include "../../base/logger.h"
#include "../../base/cmdline_opt.h"
#include "dcxml.h"
#include "dcjson.hpp"
#include "dcproto.h"
///////////////////////////////////////////////////
#include "dcxconf.h"

NS_BEGIN(dcsutil)
int  dcxconf_load(::google::protobuf::Message & msg, const char * file, dcxconf_file_type type) {
    std::string error;
    int ret = 0;
    switch (type) {
    case DCXCONF_XML:
    ret = protobuf_msg_from_xml_file(msg, file, error);
    break;
    case DCXCONF_JSON:
    ret = protobuf_msg_from_json_file(msg, file, error);
    break;
    case DCXCONF_MSGB:
    ret = protobuf_msg_from_msgb_file(msg, file);
    break;
    default:
    GLOG_ERR("unknown config file:%s type:%d ", file, type);
    return -1;
    }
    if (ret) {
        GLOG_ERR("dcxconf load file:%s error ret:%d reason:%s", file, ret, error.c_str());
    }
    return ret;
}
int  dcxconf_dump(const ::google::protobuf::Message & msg, const char * file, dcxconf_file_type type) {
    int ret = 0;
    switch (type) {
    case DCXCONF_XML:
    ret = protobuf_msg_to_xml_file(msg, file);
    break;
    case DCXCONF_JSON:
    ret = protobuf_msg_to_json_file(msg, file);
    break;
    case DCXCONF_MSGB:
    ret = protobuf_msg_to_msgb_file(msg, file);
    break;
    default:
    GLOG_ERR("unknown config file:%s type:%d ", file, type);
    return -1;
    }
    if (ret) {
        GLOG_SER("dcxconf dump file:%s error ret:%d", file, ret);
    }
    return ret;
}
void dcxconf_default(::google::protobuf::Message & msg) {
    protobuf_msg_fill_default(&msg);
}

//////////////////////////////////////////////////////////////////////////////
struct dcxcmdconf_impl_t {
    cmdline_opt_t cmdline;
    ::google::protobuf::Message & msg;
    dcxconf_file_type type;
    dcxcmdconf_impl_t(int argc, const char * argv[], ::google::protobuf::Message & msg_, dcxconf_file_type type_) :
        cmdline(argc, argv), msg(msg_), type(type_){
    }
};
//default support --conf=file, --version, --help, --default-conf-dump=file,--conf-...
dcxcmdconf_t::dcxcmdconf_t(int argc, const char * argv[], ::google::protobuf::Message & msg, dcxconf_file_type type) {
    impl = new dcxcmdconf_impl_t(argc, argv, msg, type);
}

struct convert_to_cmdline_pattern_ctx {
    std::vector<std::string>    path;
    std::string                 pattern;
};
static void
convert_to_cmdline_pattern(const string & name, const ::google::protobuf::Message & msg, int idx,
                            int level, void * ud, protobuf_sax_event_type ev_type) {
    UNUSED(level);
    convert_to_cmdline_pattern_ctx * ctx = (convert_to_cmdline_pattern_ctx*)ud;
    std::string pattern, full_field_path;
    pattern.reserve(255);
    //GLOG_DBG("event:%d level:%d idx:%d name:%s", ev_type, level, idx,name.c_str());
    switch (ev_type) {
    case BEGIN_MSG:
    if (level > 0){
        ctx->path.push_back(name);
    }
    break;
    case END_MSG:
    if (level > 0){
        ctx->path.pop_back();
    }
    break;
    case BEGIN_ARRAY:
    break;
    case END_ARRAY:
    break;
    case VISIT_VALUE:
    if (!ctx->path.empty()) {
        dcsutil::strjoin(full_field_path, "-", ctx->path);
        dcsutil::strprintf(pattern, "conf-%s-%s:r::dcxconf option:%s;", 
                           full_field_path.c_str(), name.c_str(),
                           protobuf_msg_field_get_value(msg, name, idx).c_str());
    }
    else {
        dcsutil::strprintf(pattern, "conf-%s:r::dcxconf option:%s;",
                           name.c_str(),
                           protobuf_msg_field_get_value(msg, name, idx).c_str());
    }
    ctx->pattern.append(pattern.data());
    break;
    }
    //GLOG_DBG("ctx->path:%zu full_field_path:%s", ctx->path.size(), full_field_path.c_str());
}

int dcxcmdconf_t::parse(const char * desc, const char * version) {
    std::string ndesc = desc;
    if (ndesc.back() != ';') {
        ndesc.append(";");
    }
    ndesc.append("conf:r::set config file path;config-dump-def:r::dump the default config file:config.def.xml;");
    dcxconf_default(impl->msg);
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    convert_to_cmdline_pattern_ctx ctx;
    protobuf_msg_sax(impl->msg.GetDescriptor()->name(), impl->msg, convert_to_cmdline_pattern, &ctx);
    ndesc.append(ctx.pattern);
    GLOG_DBG("desc new ...:%s (msg:%s)", ndesc.c_str(), impl->msg.DebugString().c_str());
    impl->cmdline.parse(ndesc.c_str(), version);
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    int ret = 0;
    if (cmdopt().hasopt("conf")){
        const char * config_file = cmdopt().getoptstr("conf");
        ret = dcxconf_load(impl->msg, config_file);
        if (ret){
            GLOG_ERR("load config file:%s error :%d!", config_file, ret);
            exit(-1);
        }
    }
    auto & cmdmap = impl->cmdline.options();
    std::string error, path;
    std::string last_option = "";
    int option_idx = -1;
    for (auto it = cmdmap.begin(); it != cmdmap.end(); ++it) {
        if (it->first.find("conf-") != 0){
            continue;
        }
        path = it->first.substr(5);
        if (last_option != it->first){
            option_idx = -1;
        }
        else {
            if (option_idx == -1){
                option_idx = 1;
            }
            else {
                ++option_idx;
            }
        }
        dcsutil::strreplace(path, "-", ".");
        if (option_idx > 0){
            path.append(":");
            path.append(std::to_string(option_idx));
        }
        ret = protobuf_msg_field_path_set_value(impl->msg, path, it->second, error);
        if (ret) {
            GLOG_ERR("set field value path:%s value:%s error:%d",
                it->first.c_str(), it->second.c_str(), error.c_str());
            return -1;
        }
    }
    if (cmdopt().hasopt("config-dump-def")){
        const char * config_file = cmdopt().getoptstr("config-dump-def");
        ret = dcxconf_dump(impl->msg, config_file);
        if (ret){
            GLOG_ERR("dump config file:%s error :%d!", config_file, ret);
            exit(-1);
        }
    }

    return 0;
}
cmdline_opt_t & dcxcmdconf_t::cmdopt() {
    return impl->cmdline;
}
::google::protobuf::Message & dcxcmdconf_t::config_msg() {
    return impl->msg;
}
const char *      dcxcmdconf_t::config_file() {
    return impl->cmdline.getoptstr("conf");
}
NS_END()