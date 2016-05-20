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
	msg_buffer_t	hash;
	msg_buffer_t	sign;

};
static inline void * rsa_from_perm(const char * file, bool pubkey){
	if (!pubkeyfile) return nullptr;
	FILE * fp = fopen(pubkeyfile, "r");
	if (!fp){
		GLOG_ERR("open rsa pubkey file:%s error !", pubkeyfile);
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
		GLOG_ERR("read rsa parsing from pubkey file:%s error !", pubkeyfile);
		return nullptr;
	}
	rsa_t * r = new rsa_t;
	r->rsa = rsa;
	r->hash.create(MAX_HASH_DIGEST_LENGTH);
	r->sign.create(MAX_SIGNATURE_LENGTH);
	return r;
}

void * rsa_from_perm_prvkey(const char * file){
	return rsa_from_perm(file, false);
}
void * rsa_from_perm_pubkey(const char * pubkeyfile){
	return rsa_from_perm(file, true);
}
void   rsa_free(void * rsa){
	if (rsa){
		rsa_t * r = (rsa_t*)rsa;
		RSA_free(r->rsa);
		r->hash.destroy();
		r->sign.destroy();
	}
}
bool    rsa_sign(std::string & signature, void * rsa, RSASignAlgoType meth, const char * medssage, int msglen){
	unsigned char* hash = nullptr;
	char digiest_msgbuff[SHA256_DIGEST_LENGTH * 2];
	char rsa_sign_msgbuff[];
	switch (meth){
	case RSA_SIGN_SHA1:
		hash = SHA256((unsigned char*)message, msglen, digiest_msgbuff);
		return RSA_sign(NID_sha256, (const unsigned char*)hash, SHA256_DIGEST_LENGTH,
			(const unsigned char*)signature.data(), signature.length(), rsa) == 1;
		break;
	case RSA_SIGN_SHA256:
		hash = SHA1((unsigned char*)message, msglen, digiest_msgbuff);
		return RSA_sign(NID_sha1, (const unsigned char*)hash, SHA_DIGEST_LENGTH,
			(const unsigned char*)signature.data(), signature.length(), rsa) == 1;
		break;
	default:
		GLOG_ERR("error method :%d not support yet !", meth);
		return false;
	}
	return false;
}
bool    rsa_verify(const std::string & signature, void * rsa, RSASignAlgoType meth, const char * message, int msglen){
	unsigned char* hash = nullptr;
	char digiest_msgbuff[SHA256_DIGEST_LENGTH*2];
	switch (meth){
	case RSA_SIGN_SHA1:
		hash = SHA256((unsigned char*)message, msglen, digiest_msgbuff);
		return RSA_verify(NID_sha256, (const unsigned char*)hash, SHA256_DIGEST_LENGTH,
			(const unsigned char*)signature.data(), signature.length(), rsa) == 1;
		break;
	case RSA_SIGN_SHA256:
		hash = SHA1((unsigned char*)message, msglen, digiest_msgbuff);
		return RSA_verify(NID_sha1, (const unsigned char*)hash, SHA_DIGEST_LENGTH,
			(const unsigned char*)signature.data(), signature.length(), rsa) == 1;
		break;
	default:
		GLOG_ERR("error method :%d not support yet !", meth);
		return false;
	}
	return false;
}

NS_END()