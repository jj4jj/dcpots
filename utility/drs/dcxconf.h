#pragma once
namespace google {
    namespace protobuf {
        class Message;
    }
}
struct cmdline_opt_t;
namespace dcsutil {
    enum dcxconf_file_type {
        DCXCONF_XML,
        DCXCONF_JSON,
        DCXCONF_MSGB,
        DCXCONF_YAML,
        DCXCONF_XINI,
    };
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //default support --conf=file, --version, --help, --config-dump-def=file,--conf-...
    struct dcxcmdconf_impl_t;
    struct dcxcmdconf_t {
        dcxcmdconf_t(int argc, const char * argv[], ::google::protobuf::Message & msg, dcxconf_file_type type = DCXCONF_XML);
        int                             parse(const char * desc = "", const char * version = nullptr);        
        cmdline_opt_t &                 cmdopt();
        ::google::protobuf::Message &   config_msg();
        const char *                    config_file();
        dcxcmdconf_impl_t *             impl;
    };
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    int  dcxconf_load(::google::protobuf::Message & msg, const char * file, dcxconf_file_type type = DCXCONF_XML);
    int  dcxconf_dump(const ::google::protobuf::Message & msg, const char * file, dcxconf_file_type type = DCXCONF_XML);
    void dcxconf_default(::google::protobuf::Message & msg);
}