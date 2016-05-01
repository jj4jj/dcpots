#include "dcrpc.pb.h"
#include "dcrpc.h"
using namespace google::protobuf;
namespace dcrpc {
    struct RpcValuesImpl : RepeatedPtrField< ::dcrpc::RpcMsg_ValueItem > {
    };
    int64_t RpcValues::geti(int idx) const {
        return data_->Get(idx).i();
    }
    const string & RpcValues::gets(int idx) const {
        return data_->Get(idx).s();
    }
    double RpcValues::getf(int idx) const {
        return data_->Get(idx).f();
    }
    const string & RpcValues::getb(int idx) const {
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

}
