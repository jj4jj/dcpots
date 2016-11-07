
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/aes.h>

#include "dccrypt.h"

namespace dcs {
    int     pkcs7_padding_size(int ibuffer, unsigned char align) {
        unsigned char pad = align - (ibuffer % align);
        return ibuffer + pad;
    }
    int     pkcs7_padding(unsigned char * buffer, int ibuffer, unsigned char align) {
        unsigned char pad = align - (ibuffer % align);
        for (int i = 0; i < pad; ++i) {
            buffer[i] = pad;
        }
        return ibuffer + pad;
    }
    int     pkcs7_unpadding_size(const unsigned char * buffer, int ibuffer, unsigned char align) {
        unsigned char pad = buffer[ibuffer-1];
        return ibuffer - pad;
    }

    struct aes_impl_t {
        AES_KEY             aes;
        int                 key_bytes;
        unsigned char       key[256];  //
        unsigned char       iv[256];   //
        aes_encrypt_mode    mode;
    };

    void *  aes_create(const unsigned char * key, int key_bytes, aes_encrypt_mode mode) {
        aes_impl_t * aes = new aes_impl_t();
        memset(aes, 0, sizeof(*aes));
        aes->key_bytes = key_bytes;
        int ret = AES_set_encrypt_key(aes->key, key_bytes * 8, &aes->aes);
        if (ret) {
            delete aes;
            fprintf(stderr, "AES_set_encrypt_key error:%d\n",ret);
            return nullptr;
        }

        ret = AES_set_decrypt_key(aes->key, key_bytes * 8, &aes->aes);
        if (ret) {
            delete aes;
            fprintf(stderr, "AES_set_decrypt_key error:%d\n", ret);
            return nullptr;
        }

        
        return aes;
    }
    int     aes_destroy(void * aes_) {
        if (aes_) {
            aes_impl_t * aes = (aes_impl_t*)(aes_);
            delete aes;
            return 0;
        }
        return -1;
    }
    int     aes_encrypt(void * aes_, unsigned  char * buffer, const unsigned char * data, int idata) {
        // encrypt (iv will change)
        aes_impl_t * aes = (aes_impl_t*)aes;
        switch (aes->mode) {
        case AES_ENC_ECB:   
        for (int i = 0; i < idata; i += AES_BLOCK_SIZE) {
            AES_ecb_encrypt(data + i, buffer + i, &aes->aes, AES_ENCRYPT);
            return idata;
        }
        break;
        case AES_ENC_CBC:{
            int ipdata = pkcs7_padding_size(idata, AES_BLOCK_SIZE);
            unsigned char * pdd = (unsigned char*)malloc(ipdata);
            memcpy(pdd, data, idata);
            AES_cbc_encrypt(pdd, buffer, ipdata, &aes->aes, aes->iv, AES_ENCRYPT);
            free(pdd);
            return ipdata;
        }
        break;
        default:
        return -1;
        }
    }
    int     aes_decrypt(void * aes_, unsigned  char * buffer, const unsigned char * data, int idata) {
        if (idata % AES_BLOCK_SIZE) {
            return -1;
        }
        aes_impl_t * aes = (aes_impl_t*)aes;
        switch (aes->mode) {
        case AES_ENC_ECB:
        for (int i = 0; i < idata; i += AES_BLOCK_SIZE) {
            AES_ecb_encrypt(data + i, buffer + i, &aes->aes, AES_DECRYPT);
            return idata;
        }
        break;
        case AES_ENC_CBC:
        AES_cbc_encrypt(data, buffer, idata, &aes->aes, aes->iv, AES_DECRYPT);
        return pkcs7_unpadding_size(buffer, idata, AES_BLOCK_SIZE);
        break;
        default:
        return -1;
        }
        return 0;
    }
}