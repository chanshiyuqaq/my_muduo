#ifndef NET_ACCEPTOR_H
#define NET_ACCEPTOR_H

#include<functional>

#include<net/Channel.h>
#include<net/Socket.h>
#include<muduo/base/noncopyable.h>

namespace muduo
{

namespace net
{

class EventLoop;
class InetAddress;


//只需要知道它要监听一个 InetAddress 即可

//ip与端口都在 Inetaddress 中

class Acceptor : noncopyable
{
    public:
    typedef std::function<void(int sockfd,const InetAddress&)> NewConnectionCallback;
    Acceptor(EventLoop* loop , const InetAddress& listenAddr,bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb)
    {
        newConnectionCallback_ = cb;
    }
    void listen();

    bool listening()const{return listening_;}

    private:
    void handleRead();
    
    EventLoop*loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listening_;
    int idleFd_;
}; 


}

}

#endif //NET_ACCEPTOR_H