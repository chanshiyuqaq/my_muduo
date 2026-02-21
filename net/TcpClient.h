#ifndef NET_TCPCLIENT_H
#define NET_TCPCLIENT_H

#include<muduo/base/Mutex.h>
#include<net/TcpConnection.h>
#include<net/Callbacks.h>

namespace muduo
{

namespace net
{

class Connector;
typedef std::shared_ptr<Connector> ConnectorPtr;

class TcpClient:noncopyable
{
    public:
    TcpClient(EventLoop* loop,const InetAddress& serverAddr,const string& nameArg);
    ~TcpClient();

    void connect();
    void disconnect();
    void stop();

    TcpConnectionPtr connection() const
    {
        MutexLockGuard lock(mutex_);
        return connection_;
    }
    EventLoop* getLoop() const{return loop_;}
    bool retry() const{return retry_;}
    void enableRetry(){retry_ = true;}
    const string& name() const{return name_;}

    void setConnectionCallback(ConnectionCallback cb){connectionCallback_ = std::move(cb);}
    void setMessageCallback(MessageCallBack cb){messageCallBack_ = std::move(cb);}
    void setWriteCompleteCallback(WriteCompleteCallback cb){writeCompleteCallback_ = std::move(cb);}
    

    private:
    void newConnection(int sockfd);
    void removeConnection(const TcpConnectionPtr& conn);

    EventLoop* loop_;
    ConnectorPtr connector_;
    const string name_;
    ConnectionCallback connectionCallback_;
    MessageCallBack messageCallBack_;
    WriteCompleteCallback writeCompleteCallback_;
    bool retry_;
    bool connect_;
    int nextConnId_;
    mutable MutexLock mutex_;
    TcpConnectionPtr connection_ GUARDED_BY(mutex_);

};

}



}

#endif //NET_TCPCLIENT_H