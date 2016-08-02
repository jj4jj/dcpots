#pragma once
namespace google {
    namespace protobuf {
        class Message;
    }
}
struct cmdline_opt_t;
namespace dcs {
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
        dcxcmdconf_t(::google::protobuf::Message & msg, dcxconf_file_type type = DCXCONF_XML);
        int                             init(int argc, const char * argv[]);
        int                             reload();
        int                             parse(const char * desc = nullptr, const char * version = nullptr);
        int                             command();
        cmdline_opt_t &                 cmdopt();
        const std::string &             options() const;
        ::google::protobuf::Message &   config_msg();
        const char *                    config_file();
        dcxcmdconf_impl_t *             impl_;
    };
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    int  dcxconf_load(::google::protobuf::Message & msg, const char * file, dcxconf_file_type type = DCXCONF_XML);
    int  dcxconf_dump(const ::google::protobuf::Message & msg, const char * file, dcxconf_file_type type = DCXCONF_XML);
    void dcxconf_default(::google::protobuf::Message & msg);
}