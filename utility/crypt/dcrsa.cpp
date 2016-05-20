#include "dcrsa.h"

#if 0
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

//#define _DEBUG
#define MAX_KEY_LEN     (512)

/* global area */
RSA* g_pRsa = NULL;



 getRsa(RSA** p, const char* path, const char* pubkey)
{
    if (NULL == pubkey)
    {
        char* pub_key = "public.pem";

        FILE* pub_fp = NULL;
        if (NULL != path)
        {
            if (NULL == (pub_fp = fopen(path, "r")))
            {
                fprintf(stderr, "failed to open pub_key file %s!\n", path);
                return ERET_GENERAL_ERROR;
            }
        }
        else
        {
            if (NULL == (pub_fp = fopen(pub_key, "r")))
            {
                fprintf(stderr, "failed to open pub_key file %s!\n", pub_key);
                return ERET_GENERAL_ERROR;
            }
            if (NULL == prsa)
            {
                fprintf(stderr, "unable to read public key!\n");
                return ERET_GENERAL_ERROR;
            }
            fclose(pub_fp);
            *p = prsa;
        }
    else
    {
        //get RSA from mem data, not recommended
        BIO *key_bio = NULL;
        RSA *key = NULL;
        char pub_key_data[MAX_KEY_LEN] = { 0 };
        strncpy(pub_key_data, pubkey, sizeof(pub_key_data));

        key_bio = BIO_new_mem_buf(pub_key_data, -1);
        key = PEM_read_bio_RSA_PUBKEY(key_bio, NULL, NULL, NULL);
        *p = key;
        BUF_MEM *bptr;
        BIO_get_mem_ptr(key_bio, &bptr);
        BIO_set_close(key_bio, BIO_NOCLOSE); /* So BIO_free() leaves BUF_MEM alone */
        BIO_free(key_bio);
    }

    return ERET_SUCCESS;
    }

    RET_CODE initVerify(const char* path)
    {
        RET_CODE iRet = ERET_SUCCESS;
        if (NULL != g_pRsa)
        {
            RSA_free(g_pRsa);
            g_pRsa = NULL;
        }
        //RSA* prsa = RSA_new();//mem leak
        RSA* prsa = NULL;
        if (ERET_SUCCESS != (iRet = getRsa(&prsa, path, NULL)))
            //if (ERET_SUCCESS != (iRet = getRsa(&prsa, path, pub_key_data)))
        {
            RSA_free(prsa);
            g_pRsa = NULL;
            return iRet;
        }
        g_pRsa = prsa;
        return ERET_SUCCESS;
    }

    RET_CODE verifyRaw(const char* pmsg, int msglen, const char* psig, ALGO_TYPE type)
    {
        RET_CODE iRet = ERET_SUCCESS;

        if (NULL == g_pRsa)//if not inited
        {
            if (ERET_SUCCESS != (iRet = initVerify(NULL)))
            {
                return iRet;
            }
        }

        //decode b64 of sign 
        char strDecodedSign[MAX_TOKEN_LEN] = { 0 };
        int signLen = 0;
        if (NULL == b64Decode(psig, strlen(psig), strDecodedSign, &signLen) || signLen <= 0)
        {
            return ERET_DECODE64_FAILED;
        }

        unsigned char* pszSHA = NULL;
        if (ALGO_TYPE_SHA256 == type)
        {
            pszSHA = SHA256((unsigned char*)pmsg, msglen, NULL);
            iRet = RSA_verify(NID_sha256, (const unsigned char*)pszSHA, SHA256_DIGEST_LENGTH,
                (const unsigned char*)strDecodedSign, signLen, g_pRsa);
        }
        else
        {
            pszSHA = SHA1((unsigned char*)pmsg, msglen, NULL);
            iRet = RSA_verify(NID_sha1, (const unsigned char*)pszSHA, SHA_DIGEST_LENGTH,
                (const unsigned char*)strDecodedSign, signLen, g_pRsa);
        }
#ifdef _DEBUG
        FILE* fp1 = fopen("out1.txt", "w");
        if (fp1)
        {
            fwrite(strDecodedSign, signLen, 1, fp1);
            fclose(fp1);
        }
        char aaa[1024] = { 0 };
        int tmpl = 0;
        b64Encode(strDecodedSign, signLen, aaa, &tmpl);
        FILE* fp3 = fopen("out3.txt", "w");
        if (fp3)
        {
            fwrite(aaa, tmpl, 1, fp3);
            fclose(fp3);
        }
#endif
        if (1 != iRet)
        {
            return ERET_VERIFY_ERROR;
        }

        return ERET_SUCCESS;
    }

    RET_CODE verifySession(const char* pch, const VerifySession* ps)
    {
        char* pRawTokenStr = NULL;

        time_t nowtm = time(NULL);
        if (NULL == pch || NULL == ps)
        {
            return ERET_INVALID_PARAM;
        }
#ifdef _DEBUG
        fprintf(stdout, "chann: (%s)-(%s)\n", pch, ps->channel);
#endif
        if (strncmp(pch, ps->channel, MAX_CHANNEL_LEN) != 0)
        {
            return ERET_INVALID_CHANNEL;
        }
#ifdef _DEBUG
        fprintf(stdout, "time: (%lld)-(%lld)\n", nowtm, ps->expired);
#endif
        if (nowtm > ps->expired)
        {
            return ERET_TIME_EXPIRED;
        }

        pRawTokenStr = (char*)calloc(1, MAX_UID_LEN + MAX_CHANNEL_LEN + 32);
        sprintf(pRawTokenStr, "%s%d%lld%s",
            ps->channel, ps->aid, ps->expired, ps->uid);
#ifdef _DEBUG
        fprintf(stdout, "session: (%s)-(%d)\n", pRawTokenStr, strlen(ps->token));
        fprintf(stdout, "session: (%s)\n", ps->token);
#endif

        ALGO_TYPE atype = ALGO_TYPE_SHA1;
        if ((ALGO_TYPE)ps->atype > ALGO_TYPE_BEGIN && (ALGO_TYPE)ps->atype < ALGO_TYPE_END)
        {
            atype = (ALGO_TYPE)ps->atype;
        }

        RET_CODE ret = verifyRaw(pRawTokenStr, strlen(pRawTokenStr), ps->token, atype);

        free(pRawTokenStr);
        return ret;
    }

    RET_CODE verifySessionStr(const char* pstr, VerifySession* ps)
    {
        json_error_t errt;
        json_t* ptmpjson = json_loads(pstr, JSON_DECODE_ANY, &errt);
        if (NULL == ptmpjson)
        {
            fprintf(stderr, "err in loads: %s\n", errt.text);
            return ERET_INVALID_PARAM;
        }
        ps->is_bound = 1;
        ps->atype = 1;
        const char *puid;
        const char *ptoken;
        const char *pch;
        int myiret = json_unpack_ex(ptmpjson, &errt, JSON_STRICT, "{s:s,s:s,s:i,s:i,s:s,s?b,s?i}",
            "uid", &puid,
            "token", &ptoken,
            "aid", &(ps->aid),
            "expired", &(ps->expired),
            "channel", &pch,
            "isbound", &(ps->is_bound),
            "algo_type", &(ps->atype)
            );
        if (0 != myiret)
        {
            fprintf(stderr, "json_unpack_ex failed!%d\n", myiret);
            return ERET_INVALID_PARAM;
        }
        strncpy(ps->uid, puid, MAX_UID_LEN);
        strncpy(ps->token, ptoken, MAX_TOKEN_LEN);
        strncpy(ps->channel, pch, MAX_CHANNEL_LEN);
#ifdef _DEBUG
        fprintf(stdout, "session: %s-%s-%d-%d-%s-%d-%d\n",
            ps->uid,
            ps->token,
            ps->aid,
            ps->expired,
            ps->channel,
            ps->is_bound,
            ps->atype
            );
#endif

        return verifySession(ps->channel, ps);
    }



#endif