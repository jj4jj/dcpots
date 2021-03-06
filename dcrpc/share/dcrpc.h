#pragma  once
#include <string>
namespace dcrpc {
    static const int MAX_RPC_MSG_BUFF_SIZE = (1024 * 1024);
    struct RpcValuesImpl;
    struct RpcValues {
        void extend(int n);
        int	 length() const;
        //////////////////////////////////////////////
        int64_t geti(int idx = 0) const;
        const std::string & gets(int idx = 0) const;
        double getf(int idx = 0) const;
        const std::string & getb(int idx = 0) const;
		//////////////////////////////////////////
        void seti(int64_t i, int idx = 0);
		void sets(const std::string & s, int idx = 0);
		void setf(double f, int idx = 0);
		void setb(const std::string & b, int idx = 0);
		void setb(const char * b, int len, int idx = 0);
		//////////////////////////////////////////
        void addi(int64_t i);
        void adds(const std::string & s);
        void addf(double f);
        void addb(const char * buff, int ibuf);
        //////////////////////////////////////////
        void seti(int64_t i, const std::string & name);
        void sets(const std::string & s, const std::string & name);
        void setf(double f, const std::string & name);
        void setb(const std::string & b, const std::string & name);
        void setb(const char * b, int len, const std::string & name);
        /////////////////////////////////////////////////////////
        int64_t geti(const std::string & name) const;
        const std::string & gets(const std::string & name) const;
        double getf(const std::string & name) const;
        const std::string & getb(const std::string & name) const;

    public:
        RpcValuesImpl * data();
        const RpcValuesImpl * data() const ;
        const char *    debug(std::string & str) const;
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