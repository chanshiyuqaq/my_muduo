#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#include<muduo/base/noncopyable.h>

struct tcp_info;
//前向定义

namespace muduo
{
namespace net
{
class InetAddress;

class Socket:noncopyable
{
    public:

    //在构造函数前写 explicit ,禁用该构造函数的隐式类型转换能力，
    //仅允许显式构造 / 类型转换，
    explicit Socket(int sockfd):sockfd_(sockfd)
    {}
    ~Socket();

    int fd()const{return sockfd_;}
    bool getTcpInfo(struct tcp_info*)const;
    bool getTcpInfoString(char*buf,int len)const;
    //?
    void bindAddress(const InetAddress& localaddr);
    void listen();

    int accept(InetAddress* peeraddr);

    void shutdownWrite();
    void setTcpNoDelay(bool on);
    
    //重用地址/重用端口
    void setReuseAddr(bool on);
    void setReusePort(bool on);

    //
    void setKeepAlive(bool on);

    private:
    const int sockfd_;

};
}
}


#endif //NET_SOCKET_H