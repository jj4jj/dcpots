#pragma once

namespace dcs {
    int         path_to_key(const char * path, unsigned char proj_id = 1);

    struct  ipsync_t;
    ipsync_t *  ipsync_lock(int key, bool wait = false, bool attach = false);
    void        ipsync_unlock(ipsync_t *);

};
