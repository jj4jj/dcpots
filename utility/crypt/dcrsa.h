#pragma  once
#include <string>

namespace dcsutil {

void * rsa_from_perm(const char * file);
void * rsa_from_der(const char * buff, int ibuff);
void   rsa_free(void * rsa);

int    rsa_sign(void * rsa, int meth, const char * message, int msglen, char * signature, int * siglen);
int    rsa_verify(void * rsa, int meth, const char * message, int msglen, const char * signature, int siglen);

int    rsa_with_sha1(std::string & signature, void * rsa, const char * buff, int ibuff);
int    rsa_with_sha256(std::string & signature, void * rsa, const char * buff, int ibuff);

}