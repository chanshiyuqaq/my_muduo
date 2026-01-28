#include<muduo/base/Logging.h>
#include<muduo/base/Mutex.h>

#include<net/EventLoop.h>
#include<net/Channel.h>
#include<net/Poller.h>
#include<net/SocketsOps.h>

//timequeue 没有包含




#include<algorithm>

#include<signal.h>
#include<sys/eventfd.h>
#include<unistd.h>


using namespace muduo;
using namespace muduo::net;






EventLoop::EventLoop():
looping_(false),

{}