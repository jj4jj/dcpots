#pragma  once
#include <string>

namespace dcsutil {

void * rsa_from_perm_pubkey(const char * file);
void * rsa_from_perm_prvkey(const char * file);
void   rsa_free(void * rsa);

enum RSASignAlgoType {
	RSA_SIGN_SHA1,
	RSA_SIGN_SHA256,
};

bool    rsa_sign(std::string & signature, void * rsa, RSASignAlgoType meth, const char * medssage, int msglen);
bool    rsa_verify(const std::string & signature, void * rsa, RSASignAlgoType meth, const char * message, int msglen);

}