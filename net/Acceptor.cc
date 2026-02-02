#include <net/Acceptor.h>
#include <net/EventLoop.h>
#include <net/InetAddress.h>
#include <net/SocketsOps.h>

#include <muduo/base/Logging.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;


Acceptor::Acceptor(EventLoop* loop , const InetAddress& listenAddr , bool reuseport)
:loop_(loop),acceptSocket_(sockets::createNonblockingOrDie(listenAddr.family())),
acceptChannel_(loop,acceptSocket_.fd()),
listening_(false),
idleFd_(::open("/dev/null",O_RDONLY | O_CLOEXEC))
{
    assert(idleFd_ >=0 );
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);
    acceptSocket_.bindAddress(listenAddr);
    acceptChannel_.setReadCallBack(std::bind(&Acceptor::handleRead,this));
    //这里channel的回调函数
    
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
    ::close(idleFd_);
}

void Acceptor::listen()
{
    loop_->assertInLoopThread();
    listening_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

//这里可见一斑 ， Acceptor就是封装了 channel 和 socket
//一个端口例如port 80，可以创建多个套接字，一个套接字(五元组)对应一个fd(channel)

void Acceptor::handleRead()
{
    loop_->assertInLoopThread();
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd>=0)
    {
        if(newConnectionCallback_)
        {
            newConnectionCallback_(connfd,peerAddr);
        }
        else
        {
            sockets::close(connfd);
        }

    }
    else
    {
        LOG_SYSERR << "in Acceptor::handleRead";
        if(errno == EMFILE)
        {
            ::close(idleFd_);
            idleFd_ = ::accept(acceptSocket_.fd(),NULL,NULL);
            ::close(idleFd_);
            idleFd_ = ::open("/dev/null",O_RDONLY | O_CLOEXEC);
            
            //因为采用了水平触发
            //只要队列里有连接内核会不断通知，需要取出来，处理掉
        }
    }

}

