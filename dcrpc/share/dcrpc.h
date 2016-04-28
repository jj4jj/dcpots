#pragma  once
#include <string>
#include <inttypes.h>
using std::string;
namespace dcrpc {
    static const int MAX_RPC_MSG_BUFF_SIZE = (1024 * 1024);
    struct RpcValuesImpl;
    struct RpcValues {
        int64_t geti(int idx = 0) const;
        const string & gets(int idx = 0) const;
        double getf(int idx = 0) const;
        const string & getb(int idx = 0) const;
        int length() const;
        void addi(int64_t i);
        void adds(const std::string & s);
        void addf(double f);
        void addb(const char * buff, int ibuf);
        RpcValuesImpl * data();
        const RpcValuesImpl * data() const ;

    public:
        RpcValues(const RpcValuesImpl & data);
        RpcValues(RpcValuesImpl * data);
        RpcValues(RpcValues & v);
        RpcValues(const RpcValues & v);
        RpcValues & operator = (const RpcValues & v);
        RpcValues();
        ~RpcValues();
    private:
        RpcValuesImpl * data_{ nullptr };
        bool own_{ false };
    };
}