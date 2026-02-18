#ifndef NET_EVENTLOOPTHREADPOOL_H
#define NET_EVENTLOOPTHREADPOOL_H

#include <muduo/base/Types.h>
#include <vector>
#include <memory>
#include <muduo/base/noncopyable.h>
#include <functional>

namespace muduo
{
namespace net
{

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool: noncopyable
{

    public:
    typedef std::function<void(EventLoop*)>ThreadInitCallback;
    //构造函数
    EventLoopThreadPool(EventLoop* baseLoop,const string& nameArg);
    ~EventLoopThreadPool();
    void setThreadNum(int numThreads){numThreads_ = numThreads;}
    //从线程池里初始化一个线程时候调用的函数
    void start(const ThreadInitCallback& cb = ThreadInitCallback());
    EventLoop* getNextLoop();
    EventLoop* getLoopForHash(size_t hashCode);
    std::vector<EventLoop*>getAllLoop();
    bool started()const{return started_;}
    const string& name()const{return name_;}


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