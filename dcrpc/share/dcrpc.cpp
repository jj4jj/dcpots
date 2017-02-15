#include "dcrpc.pb.h"
#include "dcrpc.h"
using namespace google::protobuf;
namespace dcrpc {
    static std::string s_null_str;
    struct RpcValuesImpl : public RepeatedPtrField< ::dcrpc::RpcMsg_ValueItem > {
    };
    int64_t RpcValues::geti(int idx) const {
        if(data_->size() == 0){return 0;}
        return data_->Get(idx).i();
    }
    const string & RpcValues::gets(int idx) const {
        if (data_->size() == 0) { return s_null_str; }
        return data_->Get(idx).s();
    }
    double RpcValues::getf(int idx) const {
        if (data_->size() == 0) { return .0; }
        return data_->Get(idx).f();
    }
    const string & RpcValues::getb(int idx) const {
        if (data_->size() == 0) { return s_null_str; }
        return data_->Get(idx).b();
    }
	void RpcValues::extend(int n){
		for (int i = data_->size(); i < n; ++i){
			data_->Add();
		}
	}
	void RpcValues::seti(int64_t i, int idx){
		data_->Mutable(idx)->set_i(i);
	}
	void RpcValues::sets(const string & s, int idx){
		data_->Mutable(idx)->set_s(s);
	}
	void RpcValues::setf(double f, int idx){
		data_->Mutable(idx)->set_f(f);
	}
	void RpcValues::setb(const string & b, int idx){
		data_->Mutable(idx)->set_b(b.data(), b.length());
	}
	void RpcValues::setb(const char * b, int len, int idx){
		data_->Mutable(idx)->set_b(b, len);
	}
    int RpcValues::length() const {
        return data_->size();
    }
    void RpcValues::addi(int64_t i){
        return data_->Add()->set_i(i);
    }
    void RpcValues::adds(const std::string & s){
        return data_->Add()->set_s(s);
    }
    void RpcValues::addf(double f){
        return data_->Add()->set_f(f);
    }
    void RpcValues::addb(const char * buff, int ibuf){
        return data_->Add()->set_b(buff, ibuf);
    }
    static inline RpcMsg_ValueItem * _mutable_named_item(RpcValuesImpl * data_, const char * name){
        for (int i = 0; i < data_->size(); ++i){
            if (!strcmp(data_->Get(i).name().c_str(),name)){
                return data_->Mutable(i);
            }
        }
        RpcMsg_ValueItem * itemp = data_->Add();
        itemp->set_name(name);
        return itemp;
    }
    void RpcValues::seti(int64_t i, const std::string & name){
        auto itemp = _mutable_named_item(data_, name.c_str());
        itemp->set_i(i);
    }
    void RpcValues::sets(const std::string & s, const std::string & name){
        auto itemp = _mutable_named_item(data_, name.c_str());
        itemp->set_s(s);
    }
    void RpcValues::setf(double f, const std::string & name){
        auto itemp = _mutable_named_item(data_, name.c_str());
        itemp->set_f(f);
    }
    void RpcValues::setb(const std::string & b, const std::string & name){
        setb(b.data(), b.length(), name);
    }
    void RpcValues::setb(const char * b, int len, const std::string & name) {
        auto itemp = _mutable_named_item(data_, name.c_str());
        itemp->set_b(b, len);
    }
    /////////////////////////////////////////////////////////
    int64_t RpcValues::geti(const std::string & name) const {
        auto itemp = _mutable_named_item(data_, name.c_str());
        return itemp->i();
    }
    const std::string & RpcValues::gets(const std::string & name) const {
        auto itemp = _mutable_named_item(data_, name.c_str());
        return itemp->s();
    }
    double RpcValues::getf(const std::string & name) const {
        auto itemp = _mutable_named_item(data_, name.c_str());
        return itemp->f();
    }
    const std::string & RpcValues::getb(const std::string & name) const {
        auto itemp = _mutable_named_item(data_, name.c_str());
        return itemp->b();
    }
    RpcValues::RpcValues(const RpcValuesImpl & data){
        this->data_ = const_cast<RpcValuesImpl*>(&data);
        this->own_ = false;
    }
    RpcValues::RpcValues(RpcValuesImpl * data){
        this->data_ = data;
        this->own_ = false;
    }
    RpcValues::RpcValues(const RpcValues & v){
        this->data_ = v.data_;
        this->own_ = false;
    }
    RpcValues & RpcValues::operator = (const RpcValues & v){
        if (this->data_ && this->own_ ){
            delete this->data_;
        }
        this->data_ = v.data_;
        this->own_ = false;
        return *this;
    }
    RpcValues::RpcValues(RpcValues & v){
        this->data_ = v.data_;
        if (v.own_){
            v.own_ = false;
            this->own_ = true;
        }
        else {
            this->own_ = false;
        }
    }
    RpcValues::RpcValues(){
        data_ = new RpcValuesImpl();
        this->own_ = true;
    }
    RpcValues::~RpcValues(){
        if (data_ && own_){
            delete data_;
            data_ = nullptr;
        }
    }
    const RpcValuesImpl * RpcValues::data() const {
        return this->data_;
    }
    RpcValuesImpl * RpcValues::data(){
        return this->data_;
    }
    const char *    RpcValues::debug(std::string & str) const {
        str.clear();
        for (int i = 0; i < this->length(); ++i){
            str.append(this->data_->Get(i).ShortDebugString().c_str());
        }
        return str.c_str();
    }


}
