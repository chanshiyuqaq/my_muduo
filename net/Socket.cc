#include<net/Socket.h>
#include<net/SocketsOps.h>
#include<net/InetAddress.h>

#include<muduo/base/Logging.h>

#include<netinet/in.h>
#include<netinet/tcp.h>
#include<stdio.h>

using namespace muduo;
using namespace muduo::net;


Socket::~Socket(){sockets::close(sockfd_);}


bool Socket::getTcpInfo(struct tcp_info* tcpi)const
{
    socklen_t len = sizeof(*tcpi);
    memZero(tcpi,len);
    return ::getsockopt(sockfd_,SOL_TCP,TCP_INFO,tcpi,&len) == 0;
}

//将套接字文件描述符里的内容写到tcpi中
bool Socket::getTcpInfoString(char*buf,int len)const
{
    struct tcp_info tcpi;
    bool ok = getTcpInfo(&tcpi);
    if(ok){

snprintf(buf, len, "unrecovered=%u "
             "rto=%u ato=%u snd_mss=%u rcv_mss=%u "
             "lost=%u retrans=%u rtt=%u rttvar=%u "
             "sshthresh=%u cwnd=%u total_retrans=%u",
             tcpi.tcpi_retransmits,  // Number of unrecovered [RTO] timeouts
             tcpi.tcpi_rto,          // Retransmit timeout in usec
             tcpi.tcpi_ato,          // Predicted tick of soft clock in usec
             tcpi.tcpi_snd_mss,
             tcpi.tcpi_rcv_mss,
             tcpi.tcpi_lost,         // Lost packets
             tcpi.tcpi_retrans,      // Retransmitted packets out
             tcpi.tcpi_rtt,          // Smoothed round trip time in usec
             tcpi.tcpi_rttvar,       // Medium deviation
             tcpi.tcpi_snd_ssthresh,
             tcpi.tcpi_snd_cwnd,
             tcpi.tcpi_total_retrans);  // Total retransmits for entire connection


    }
    return ok;
}

//使用传入的地址进行端口的绑定
void Socket::bindAddress(const InetAddress& addr)
{
    sockets::bindOrDie(sockfd_,addr.getSockAddr());
}

void Socket::listen()
{
    sockets::listenOrDie(sockfd_);
}

int Socket::accept(InetAddress*peeraddr)
{
    struct sockaddr_in6 addr;
    memZero(&addr,sizeof addr);
    int connfd = sockets::accept(sockfd_,&addr);
    //这行代码获得   [新的连接的文件描述符connfd,并填充对端的addr]
    if(connfd >=0)
    {
        peeraddr->setSockAddrInet6(addr);
        //[将 addr 传到 peeraddr]
    }
    return connfd;
}

void Socket::shutdownWrite()
{
    sockets::shutdownWrite(sockfd_);
}

void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1:0;
    ::setsockopt(sockfd_,IPPROTO_TCP,TCP_NODELAY,&optval,static_cast<socklen_t>(sizeof optval));

}

void Socket::setReuseAddr(bool on)
{
    int optval = on?1:0;
    ::setsockopt(sockfd_,SOL_SOCKET,SO_REUSEADDR,&optval,static_cast<socklen_t>(sizeof optval));

}

void Socket::setReusePort(bool on)
{
 #ifdef SO_REUSEPORT
  int optval = on ? 1 : 0;
  int ret = ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT,
                         &optval, static_cast<socklen_t>(sizeof optval));
  if (ret < 0 && on)
  {
    LOG_SYSERR << "SO_REUSEPORT failed.";
  }
#else
  if (on)
  {
    LOG_ERROR << "SO_REUSEPORT is not supported.";
  }
#endif   

}

void Socket::setKeepAlive(bool on)
{
 int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE,
               &optval, static_cast<socklen_t>(sizeof optval));

}
