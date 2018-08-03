#pragma once

namespace dcs {
    int         path_to_key(const char * path, unsigned char proj_id = 1);

    struct  ipsync_t;
    ipsync_t *  ipsync_init(int key, bool attach = false);
    int         ipsync_lock(ipsync_t *, int wait_us = 0);
    void        ipsync_unlock(ipsync_t *);
    void        ipsync_free(ipsync_t *);
};
