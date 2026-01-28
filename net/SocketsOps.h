#ifndef NET_SOCKETSOPS_H
#define NET_SOCKETSOPS_H

#include<arpa/inet.h>

namespace muduo
{

namespace net
{

namespace sockets
{

    //对Linux原生Socket API的封装
    int createNonblockingOrDie(sa_family_t family);
    int connect(int sockfd,const struct sockaddr* addr);
    void bindOrDie(int sockfd,const struct sockaddr* addr);
    void listenOrDie(int socket);
    int accept(int sockfd,const struct sockaddr_in6* addr);
    void close(int sockfd);
    void shutdownWrite(int sockfd);

    ssize_t read(int sockfd,void*buf,size_t count);
    ssize_t readv(int sockfd,const struct iovec*iov,int iovcnt);
    ssize_t write(int sockfd,const void* buf,size_t count);

    void toIpPort(char*buf,size_t size,const struct sockaddr*addr);
    void toIp(char*buf,size_t size,const struct sockaddr*addr);

    void fromIpPort(const char*ip,uint16_t port,struct sockaddr_in*addr);
    void fromIpPort(const char*ip,uint16_t port,struct sockaddr_in6*addr);

    int getSocketError(int sockfd);

    //传进来sockaddr_in/in6 ->sockaddr
    const struct sockaddr* sockaddr_cast(const struct sockaddr_in*addr);
    const struct sockaddr* sockaddr_cast(const struct sockaddr_in6*addr);
    struct sockaddr* sockaddr_cast(struct sockaddr_in6*addr);
    //传进来 sockaddr -> sockaddr_in/in6
    const struct sockaddr_in* sockaddr_in_cast(const struct sockaddr*addr);
    const struct sockaddr_in6* sockaddr_in6_cast(const struct sockaddr*addr);

    //传进去 sockfd 返回 地址
    struct sockaddr_in6 getLocalAddr(int sockfd);
    struct sockaddr_in6 getPeerAddr(int sockfd);

    bool isSelfConnected(int sockfd);


}
}
}

#endif //NET_SOCKETSOPS_H