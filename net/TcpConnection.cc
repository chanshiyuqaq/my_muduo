#include<net/TcpConnection.h>
#include<net/Channel.h>
#include<net/EventLoop.h>
#include<net/Socket.h>
#include<net/SocketsOps.h>
#include<net/TimerId.h>

#include<muduo/base/Logging.h>
#include<muduo/base/WeakCallback.h>

#include<errno.h>

using namespace muduo;
using namespace muduo::net;


void muduo::net::defaultConnectionCallback(const TcpConnectionPtr& conn)
{
    LOG_TRACE << conn->localAddress().toIpPort()<<" -> "
    <<conn->peerAddress().toIpPort()<<" is "
    <<(conn->connected()?"UP":"DOWN");

}


void muduo::net::defaultMessageCallback(const TcpConnectionPtr&,Buffer*buf,Timestamp)
{
    buf->retrieveAll();
}

TcpConnection::TcpConnection(EventLoop*loop,const string& nameArg,int sockfd,const InetAddress&localAddr,const InetAddress&peerAddr)
:loop_(CHECK_NOTNULL(loop)),
name_(nameArg),
state_(kConnecting),
reading_(true),
socket_(new Socket(sockfd)),
channel_(new Channel(loop,sockfd)),
localAddr_(localAddr),
peerAddr_(peerAddr),
highWaterMark_(64*1024*1024)
{

//给内部channel_设置各种回调函数
    channel_->setReadCallBack(std::bind(&TcpConnection::handleRead,this,_1));
    channel_->setWriteCallBack(std::bind(&TcpConnection::handleWrite,this));
    channel_->setCloseCallBack(std::bind(&TcpConnection::handleclose,this));    
    channel_->setErrorCallBack(std::bind(&TcpConnection::handleError,this));
    LOG_DEBUG << "TcpConnection::ctor[" << name_ << "] at "<<this <<" fd=" << sockfd;
    socket_->setKeepAlive(true);
}   

TcpConnection::~TcpConnection()
{
      LOG_DEBUG << "TcpConnection::dtor[" <<  name_ << "] at " << this
            << " fd=" << channel_->fd()
            << " state=" << stateToString();
    assert(state_ == kDisconnected);
}



//对连接内部的socket进行函数的封装
bool TcpConnection::getTcpInfo(struct tcp_info*tcpi)const
{
    return socket_->getTcpInfo(tcpi);
}

string TcpConnection::getTcpInfoString()const
{
    char buf[1024];
    buf[0] = '\0';
    //为什么？//标记为合法空串
    socket_->getTcpInfoString(buf,sizeof buf);
    return buf;
}

void TcpConnection::send(const void* data,int len)
{
    send(StringPiece(static_cast<const char*>(data),len));  //这里调用另一个send函数
}

void TcpConnection::send(const StringPiece& message)
{
    if(state_ == kConnected)
    {
        if(loop_->isInLoopThread())
        {
            sendInLoop(message);
        }
        else
        {
            void (TcpConnection::*fp)(const StringPiece& message) = &TcpConnection::sendInLoop;
            //代码声明了一个叫 fp 的变量，这个变量专门用来存放
            //TcpConnection 类中那个“接收 StringPiece 参数且不返回值”的成员函数的地址
            //类成员函数指针
            
            loop_->runInLoop(std::bind(fp,shared_from_this(),message.as_string()));
                                //绑定  函数指针  调用者       函数参数
        
        }
    }
}


void TcpConnection::send(Buffer* buf)
{
    if(state_ == kConnected)
    {
        if(loop_->isInLoopThread())
        {
            sendInLoop(buf->peek(),buf->readableBytes());
            buf->retrieveAll();
        }
    }
    else
    {
        void(TcpConnection::*fp)(const StringPiece& message) = &TcpConnection::sendInLoop;
        loop_->runInLoop(std::bind(fp,this,buf->retrieveAllAsString()));
    }

}

void TcpConnection::sendInLoop(const StringPiece& message)
{
    sendInLoop(message.data(),message.size());
}

void TcpConnection::sendInLoop(const void* data,size_t len)

{
    loop_->assertInLoopThread();
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;
    if(state_ == kDisconnected)
    {
        LOG_WARN << "disconnected give up writing";
        return;
    }
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {   
        //当前channel_不在写东西
        //无可读数据
        nwrote = sockets::write(channel_->fd(),data,len);
        if(nwrote >= 0)
        {
            remaining = len - nwrote;
            if(remaining == 0 && writeCompleteCallback_)
            {
                loop_->queueInLoop(std::bind(writeCompleteCallback_,shared_from_this()));
            }
        }
        else
        {
            nwrote = 0;
            if(errno !=EWOULDBLOCK)
            {
                LOG_SYSERR << "TCPConnection::sendInLoop";
                if(errno == EPIPE || errno == ECONNRESET)
                {
                    faultError = true;
                }
            }
        }

    }
    assert(remaining<=len);

    if(!faultError && remaining > 0)
    {
        //还有剩余的

        //?
        size_t oldLen = outputBuffer_.readableBytes();
        //缓冲区已经积压得数据

        //只在首次的时候触发回调函数
        
        if(oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_,shared_from_this(),oldLen+remaining));
            //设置了回调函数
            //用于流量控制
        }
        //放到缓冲区
        outputBuffer_.append(static_cast<const char*>(data)+nwrote,remaining);
        
        if(!channel_->isWriting())
        {
            channel_->enableWriting();
        }
    }


}


void TcpConnection::shutdown()
{
    if(state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop,shared_from_this()));
    }

}

void TcpConnection::shutdownInLoop()
{
    loop_->assertInLoopThread();
    if(!channel_->isWriting())
    {
        socket_->shutdownWrite();
    }
    //连接状态已经变了
    //在写完回调中会检查，并关闭
}

void TcpConnection::forceClose()
{
    if(state_ == kConnected || state_ == kDisconnecting)
    {
        setState(kDisconnecting);
        loop_->queueInLoop(std::bind(&TcpConnection::forceCloseInLoop,shared_from_this()));
        //? 使用run in loop 会立即调用 queue in loop会延迟调用
        //   业务逻辑和 IO 逻辑已经完全退出了调用栈
    }

}

void TcpConnection::forceCloseWithDelay(double seconds)
{
    if(state_ == kConnected || state_ == kDisconnecting)
    {
        setState(kDisconnecting);
        loop_->runAfter(seconds,makeWeakCallback(shared_from_this(),&TcpConnection::forceClose));
        //这里make weakCallBack?不延长连接的生命期！
        //如果提升成功：说明对象还活着，那就执行 forceClose()。
        //如果提升失败：说明对象在延迟期间已经被销毁了
    }


}

void TcpConnection::forceCloseInLoop()
{
    loop_->assertInLoopThread();
    if(state_ == kConnected || state_ == kDisconnecting)
    {
        handleclose();
    }
}

const char* TcpConnection::stateToString()const
{
  switch (state_)
  {
    case kDisconnected:
      return "kDisconnected";
    case kConnecting:
      return "kConnecting";
    case kConnected:
      return "kConnected";
    case kDisconnecting:
      return "kDisconnecting";
    default:
      return "unknown state";
  }
}

void TcpConnection::setTcpNoDelay(bool on)
{
    socket_->setTcpNoDelay(on);
}

void TcpConnection::startRead()
{
    loop_->runInLoop(std::bind(&TcpConnection::startReadInLoop,this));
    //为什么这里不用shared from this?
    //这里调用是安全的！
    //立即执行不涉及回调的时候的对象生命期管理问题
}


void TcpConnection::startReadInLoop()
{
    loop_->assertInLoopThread();
    if(!reading_ || !channel_->isReading())
    {
        //如果不在读就开始读
        channel_->enableReading();
        reading_ = true;
    }
}

void TcpConnection::stopRead()
{
    loop_->runInLoop(std::bind(&TcpConnection::stopReadInLoop,this));
}

void TcpConnection::stopReadInLoop()
{
    loop_->assertInLoopThread();
    if(reading_ || channel_->isReading())
    {
        channel_->disableReading();
        reading_ = false;
    }
}

//连接建立完成
void TcpConnection::connectEstablished()
{   
    loop_->assertInLoopThread();
    assert(state_ == kConnecting);
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();

    connectionCallback_(shared_from_this());
}

//连接销毁

void TcpConnection::connectDestroyed()
{
    loop_->assertInLoopThread();
    if(state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }
    channel_->remove();

}

//处理各种情况

void TcpConnection::handleRead(Timestamp receiveTime)
{
    loop_->assertInLoopThread();
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(),&savedErrno);
    if(n>0)
    {
        messageCallBack_(shared_from_this(),&inputBuffer_,receiveTime);
    }
    else if(n == 0)
    {
        handleclose();
    }
    else
    {
        errno = savedErrno;
        LOG_SYSERR << "TcpConnection handle read";
        handleError();
    }
    //？Muduo 默认使用 Epoll 的 Level Triggered (LT) 模式。
    //LT 的特性：只要内核缓冲区里还有数据没读完，Epoll 就会在下一次调用 epoll_wait 时再次提醒。
    //??如果使用poll?
}



void TcpConnection::handleWrite()
{
    loop_->assertInLoopThread();
    if(channel_->isWriting())
    {
        ssize_t n = sockets::write(channel_->fd(),outputBuffer_.peek(),outputBuffer_.readableBytes());
        if(n > 0)
        {   
            // 移动读取指数，标记这 n 字节已经发完了
            outputBuffer_.retrieve(n);
            if(outputBuffer_.readableBytes() == 0)// 数据终于全部发完(应用层缓冲区里面可读的为0)
            {
                //已经没有能向内核缓冲区里写的东西了，等待内核缓冲区有空位 ➔ Reactor 触发 handleWrite
                channel_->disableWriting();
                
                if(writeCompleteCallback_)
                {
                    loop_->queueInLoop(std::bind(writeCompleteCallback_,shared_from_this()));
                }
                if(state_ == kDisconnecting)
                {
                    //在loop中shutdonw
                    shutdownInLoop();
                }
            }

        }
        else
        {
            LOG_SYSERR << "TcpConnection::handleWrite";
        }
    }
    else
    {
        LOG_TRACE << "Connection fd = " << channel_->fd()
        << " is down, no more writing";
    }

}

void TcpConnection::handleclose()
{
    loop_->assertInLoopThread();

    assert(state_ == kConnected || state_ == kDisconnecting);
    setState(kDisconnected);
    channel_->disableAll();
    TcpConnectionPtr  guardThis(shared_from_this());
    connectionCallback_(guardThis);
    closeCallback_(guardThis);
}


void TcpConnection::handleError()
{
  int err = sockets::getSocketError(channel_->fd());
  LOG_ERROR << "TcpConnection::handleError [" << name_
            << "] - SO_ERROR = " << err << " " << strerror_tl(err);
}