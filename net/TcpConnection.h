#ifndef NET_TCPCONNECTION_H
#define NET_TCPCONNECTION_H

#include<muduo/base/noncopyable.h>
#include<muduo/base/StringPiece.h>
#include<muduo/base/Types.h>

#include<net/Callbacks.h>
#include<net/Buffer.h>
#include<net/InetAddress.h>

#include<atomic>
#include<memory>
#include<boost/any.hpp>

struct tcp_info;

namespace muduo
{

namespace net
{

class Channel;
class EventLoop;
class Socket;


class TcpConnection:noncopyable,public std::enable_shared_from_this<TcpConnection>
{

    public:

    TcpConnection(EventLoop*loop,const string& name,int sockfd,const InetAddress& localAddr,const InetAddress& peerAddr);

    ~TcpConnection();

    EventLoop* getLoop()const{return loop_;}
    const string& name()const{return name_;}
    const InetAddress& localAddress()const{return localAddr_;}
    const InetAddress& peerAddress()const{return peerAddr_;}
    bool connected() const{return state_.load() == kConnected;}
    bool disconnected() const{return state_.load() == kDisconnected;}
    bool getTcpInfo(struct tcp_info*)const;
    string getTcpInfoString()const;

    void send(const void*message,int len);
    void send(const StringPiece&message);
    void send(Buffer* message);
    void shutdown();
    void forceClose();
    void forceCloseWithDelay(double seconds);
    void setTcpNoDelay(bool on);
    void startRead();
    void stopRead();
    bool isReading()const{return reading_.load();}
    //
    void setContext(const boost::any&context){context_ = context;};
    const boost::any&getContext()const{return context_;}
    boost::any*getMutableContext(){return &context_;}
    void setConnectionCallback(const ConnectionCallback& cb){connectionCallback_ = cb;}
    void setMessageCallback(const MessageCallBack& cb){messageCallBack_ = cb;}
    void setWriteCompleteCallback(const WriteCompleteCallback& cb){writeCompleteCallback_ = cb;}
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb){highWaterMarkCallback_ = cb;}
    
    Buffer*inputBuffer()
    {
        return &inputBuffer_;
    }
    Buffer*outputBuffer()
    {
        return &outputBuffer_;
    }

    void setCloseCallback(const CloseCallback& cb){closeCallback_ = cb;}
    void connectEstablished();
    void connectDestroyed();


    private:

    enum StateE:int{kDisconnected,kConnecting,kConnected,kDisconnecting};
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleclose();
    void handleError();

    void sendInLoop(const StringPiece& message);
    void sendInLoop(const void*message,size_t len);
    void shutdownInLoop();
    void forceCloseInLoop();
    
    void setState(StateE s){state_.store(s);}
    const char* stateToString()const;
    void startReadInLoop();
    void stopReadInLoop();

    EventLoop* loop_;
    const string name_;
    std::atomic<StateE> state_;
    std::atomic<bool> reading_;

    //
    std::unique_ptr<Socket>socket_;
    std::unique_ptr<Channel>channel_;
    const InetAddress localAddr_;
    const InetAddress peerAddr_;
    ConnectionCallback connectionCallback_;
    MessageCallBack messageCallBack_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;
    Buffer inputBuffer_;
    Buffer outputBuffer_;
    boost::any context_; 

};

typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;

}
}


#endif //NET_TCPCONNECTION_H