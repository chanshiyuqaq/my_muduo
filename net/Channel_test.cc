#include <net/EventLoop.h>
#include <net/InetAddress.h>
#include <net/Acceptor.h>
#include <net/SocketsOps.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

void newConnection(int sockfd, const InetAddress& peerAddr)
{
  printf("newConnection(): accepted a new connection from %s\n",
         peerAddr.toIp().c_str());
  ::write(sockfd, "How are you?\n", 13);
  sockets::close(sockfd);
}

int main()
{
  printf("main(): pid = %d\n", getpid());

  InetAddress listenAddr(9981);
  EventLoop loop;

  Acceptor acceptor(&loop, listenAddr,false);
  acceptor.setNewConnectionCallback(newConnection);
  acceptor.listen();

  loop.loop();
}