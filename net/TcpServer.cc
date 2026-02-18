//服务器端的函数实现

#include <net/TcpServer.h>
#include <muduo/base/Logging.h>
#include <net/Acceptor.h>
#include <net/EventLoop.h>
#include <net/SocketsOps.h>
#include <net/EventLoopThreadPool.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;


//构造函数

TcpServer::TcpServer(EventLoop* loop,const InetAddress& listenAddr,const string&nameArg,Option option)
:
loop_(CHECK_NOTNULL(loop)),
ipPort_(listenAddr.toIpPort()),
name_(nameArg),
acceptor_(new Acceptor(loop,listenAddr,option == kReusePort)),
threadPool_(new EventLoopThreadPool(loop,name_)),       //初始化线程池
connectionCallback_(defaultConnectionCallback),
messageCallback_(defaultMessageCallback),
nextConnId_(1)
{
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection,this,_1,_2));

}

TcpServer::~TcpServer()
{
    loop_->assertInLoopThread();
    LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";
    for(auto& item:connections_)
    {
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed,conn));
    }
}

void TcpServer::setThreadNum(int numThreads)
{
    assert(0<=numThreads);
    threadPool_->setThreadNum(numThreads);
}


void TcpServer::start()
{
    if(started_.getAndSet(1) == 0)
    {
        threadPool_->start(threadInitCallback_);
        //线程池开始工作
        //开很多的EventLoopthread对象，本质是（线程+事件循环）


        //开始监听
        assert(!acceptor_->listening());
        loop_->runInLoop(std::bind(&Acceptor::listen,get_pointer(acceptor_)));
    }
}

void TcpServer::newConnection(int sockfd,const InetAddress& peerAddr)
{
    //处理新连接加入
    //调用线程池给新的连接分配线程
    loop_->assertInLoopThread();
    EventLoop*ioLoop = threadPool_->getNextLoop();
    char buf[64];
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    string connName = name_+buf;
    LOG_INFO << "TcpServer::newConnection [" << name_
           << "] - new connection [" << connName
           << "] from " << peerAddr.toIpPort();

    InetAddress localAddr(sockets::getLocalAddr(sockfd));
    TcpConnectionPtr conn(new TcpConnection(ioLoop,connName,sockfd,localAddr,peerAddr));
    connections_[connName] = conn;
    conn->setConnectionCallback(connectionCallback_);                           //设置各种回调函数
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection,this,_1));    //?
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished,conn));

}


    //以及处理连接断开
void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop,this,conn));
}


void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{

    loop_->assertInLoopThread();

    size_t n = connections_.erase(conn->name());
    (void)n;
    assert(n==1);
    EventLoop* ioloop = conn->getLoop();
    ioloop->queueInLoop(std::bind(&TcpConnection::connectDestroyed,conn));

}


