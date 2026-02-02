#include<net/Poller.h>
#include<net/Channel.h>

using namespace muduo;
using namespace muduo::net;

Poller::Poller(EventLoop*loop):ownerLoop_(loop){}

Poller::~Poller() = default;

bool Poller::hasChannel(Channel* Channel)const{
    assertInloopThread();
    ChannelMap::const_iterator it = channels_.find(Channel->fd());
    return it!= channels_.end() && it->second==Channel;
}






