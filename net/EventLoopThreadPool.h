#ifndef NET_EVENTLOOPTHREADPOOL_H
#define NET_EVENTLOOPTHREADPOOL_H

#include <muduo/base/Types.h>
#include <vector>
#include <memory>
#include <muduo/base/noncopyable.h>

namespace muduo
{
namespace net
{

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool: noncopyable
{

    public:

    //还有Tcp server .cpp


    private:
    EventLoop* baseLoop_;
    string name_;
    bool started_;
    bool numThreads_;
    int next_;
    std::vector<std::unique_ptr<EventLoopThread>>threads_;
    std::vector<EventLoop*>loops_;

};



}
}




#endif //NET_EVENTLOOPTHREADPOOL_H