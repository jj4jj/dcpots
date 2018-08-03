#pragma once
#include <functional>
#include "singleton.hpp"

namespace dcs {

struct ChannelPoint;
struct AsyncChannelImpl;
typedef std::function<void()>    OpenFuture;
typedef std::function<void()>    CloseFuture;
typedef std::function<void(ChannelPoint * peer, const char * buff, int ibuff)>    RecvFuture;
struct AsyncChannel : public singleton<AsyncChannel> {
    ChannelPoint *      Open(const char * addr, int max_channel_size, OpenFuture onopen = nullptr, CloseFuture onclose = nullptr);
    ChannelPoint *      Listen(const char * addr, int max_channel_size, RecvFuture onrecv = nullptr, CloseFuture onclose = nullptr);
    void                Close(ChannelPoint * point);
    int                 Send(ChannelPoint * point, const char * buff, int ibuff);
    int                 Recv(ChannelPoint * point, char * buff, int maxbuff, bool block = false);
    int                 Update();
private:
    AsyncChannelImpl *  impl {nullptr};
};


};

