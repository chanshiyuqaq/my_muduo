#ifndef NET_POLLER_H
#define NET_POLLER_H

#include<map>
#include<vector>
#include<muduo/base/Timestamp.h>
#include<net/EventLoop.h>


namespace muduo
{
namespace net
{

class Channel;

class Poller:noncopyable
{

public:
    typedef std::vector<Channel*>ChannelList;
    Poller(EventLoop*loop);
    virtual ~Poller();

    virtual Timestamp poll(int timeoutMs,ChannelList* activeChannels) = 0;
   
    //针对一个channel进行更新
    virtual void updateChannel(Channel*channel) = 0;
    virtual void removeChannel(Channel*Channel) = 0;
    virtual bool hasChannel(Channel*Channel)const;
   
    //针对channel的删除, 这里只是删除了存储在容器指针
    //真正的删除是在其对应的TCPconnection析构之后，自然析构的

    static Poller* newDefaultPoller(EventLoop*loop);
    //什么意思？

    void assertInloopThread()const{ownerLoop_->assertInLoopThread();}



protected:
    typedef std::map<int,Channel*> ChannelMap;
    ChannelMap channels_;

private:
    EventLoop* ownerLoop_;     
};
}

}

#endif //NET_POLLER_H
