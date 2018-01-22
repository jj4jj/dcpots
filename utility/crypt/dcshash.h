#pragma  once

namespace dcs {


const char * crc16(const char * buff, int ibuff);
const char * crc32(const char * buff, int ibuff);
const char * md5(const char * buff, int ibuff);
const char * sha1(const char * buff, int ibuff);
const char * sha256(const char * buff, int ibuff);
const char * hmac(const char * buff, int ibuff);


}