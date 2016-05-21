#include <openssl/hmac.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/err.h>
#include <openssl/md5.h>
#include <openssl/pem.h>
#include <arpa/inet.h>
#include <openssl/x509.h>
#include <endian.h>

#include "../../base/stdinc.h"
#include "../../base/dcutils.hpp"
#include "../../base/logger.h"
#include "../../base/msg_buffer.hpp"

#include "dcrsa.h"

NS_BEGIN(dcsutil)
#define MAX_HASH_DIGEST_LENGTH	(1024)
#define MAX_SIGNATURE_LENGTH	(4096)
struct rsa_t {
	RSA * rsa{ nullptr };
	msg_buffer_t	hashbuff;
	msg_buffer_t	signbuff;

};
static inline void * rsa_from_perm(const char * keyfile, bool pubkey){
	if (!pubkey) return nullptr;
    FILE * fp = fopen(keyfile, "r");
	if (!fp){
        GLOG_ERR("open rsa pubkey file:%s error !", keyfile);
		return nullptr;
	}
	RSA * rsa = nullptr;
	if (pubkey){
		rsa = PEM_read_RSA_PUBKEY(fp, NULL, NULL, NULL);
	}
	else {
		rsa = PEM_read_RSAPrivateKey(fp, NULL, NULL, NULL);
	}
	fclose(fp);
	if (!rsa){
        GLOG_ERR("read rsa parsing from pubkey file:%s error !", keyfile);
		return nullptr;
	}
	rsa_t * r = new rsa_t;
	r->rsa = rsa;
	r->hashbuff.create(MAX_HASH_DIGEST_LENGTH);
    int rsa_size = RSA_size(rsa);
    r->signbuff.create(rsa_size + 16);
    r->signbuff.valid_size = rsa_size;
	return r;
}

void * rsa_from_perm_prvkey(const char * prikeyfile){
    return rsa_from_perm(prikeyfile, false);
}
void * rsa_from_perm_pubkey(const char * pubkeyfile){
    return rsa_from_perm(pubkeyfile, true);
}
void   rsa_free(void * rsa){
	if (rsa){
		rsa_t * r = (rsa_t*)rsa;
		RSA_free(r->rsa);
		r->hashbuff.destroy();
		r->signbuff.destroy();
	}
}
bool    rsa_sign(std::string & signature, void * r, RSASignAlgoType meth, const char * buff, int ibuff){
	unsigned char* hash = nullptr;
    rsa_t * rsa = (rsa_t*)r;
    signature.clear();
    rsa->signbuff.valid_size = rsa->signbuff.max_size;
	switch (meth){
	case RSA_SIGN_SHA1:
        hash = SHA256((unsigned char*)buff, ibuff, (unsigned char *)rsa->hashbuff.buffer);
		return RSA_sign(NID_sha256, hash, SHA256_DIGEST_LENGTH,
            (unsigned char*)rsa->signbuff.buffer, (unsigned int *)&rsa->signbuff.valid_size, rsa->rsa) == 1;
        break;
	case RSA_SIGN_SHA256:
        hash = SHA1((unsigned char*)buff, ibuff, (unsigned char *)rsa->hashbuff.buffer);
		return RSA_sign(NID_sha1, hash, SHA_DIGEST_LENGTH,
            (unsigned char*)rsa->signbuff.buffer, (unsigned int *)&rsa->signbuff.valid_size, rsa->rsa) == 1;
        break;
	default:
		GLOG_ERR("error method :%d not support yet !", meth);
		return false;
	}
    signature.append(rsa->signbuff.buffer, rsa->signbuff.valid_size);
	return false;
}
bool    rsa_verify(const std::string & signature, void * r, RSASignAlgoType meth, const char * buff, int ibuff){
	unsigned char* hash = nullptr;
    rsa_t * rsa = (rsa_t*)r;
    bool ret = false;
	switch (meth){
	case RSA_SIGN_SHA1:
        hash = SHA256((unsigned char*)buff, ibuff, (unsigned char *)rsa->hashbuff.buffer);
        ret = RSA_verify(NID_sha256, hash, SHA256_DIGEST_LENGTH,
            (const unsigned char*)signature.data(), signature.length(), rsa->rsa) == 1;
		break;
	case RSA_SIGN_SHA256:
        hash = SHA1((unsigned char*)buff, ibuff, (unsigned char *)rsa->hashbuff.buffer);
        ret = RSA_verify(NID_sha1, hash, SHA_DIGEST_LENGTH,
            (const unsigned char*)signature.data(), signature.length(), rsa->rsa) == 1;
		break;
	default:
		GLOG_ERR("error method :%d not support yet !", meth);
		return false;
	}
	return ret;
}

NS_END()