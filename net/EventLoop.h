#ifndef NET_EVENTLOOP_H
#define NET_EVENTLOOP_H


#include<atomic>
#include<functional>
#include<vector>

#include<boost/any.hpp>
//为什么

#include<muduo/base/Mutex.h>
#include<muduo/base/CurrentThread.h>
#include<muduo/base/Timestamp.h>



namespace muduo
{
namespace net
{

class Channel;
class Poller;
class TimerQueue;

class EventLoop : noncopyable
{

    public:
    typedef std::function<void()> Functor;
    EventLoop();
    ~EventLoop();
    void loop();
    void quit();
    //常量返回的函数
    Timestamp pollReturnTime()const{return pollReturnTime_;}
    int64_t iteration() const{return iteration_;}

    void runInLoop(Functor cb);
    //确保线程安全的一种实现，在目标线程内运行对应的函数
    void queueInLoop(Functor cb);
    size_t queueSize()const;

    //与定时器相关的函数

    //与Channel相关的函数
    void wakeup();
    void updateChannel(Channel*Channel);
    void removeChannel(Channel*Channel);
    bool hasChannel(Channel*Channel);

    bool isInLoopThread()const{return threadId_ == CurrentThread::tid();}
    bool eventHandling() const{return eventHandling_;}
    void assertInLoopThread(){
        if(!isInLoopThread()){abortNotInLoopThread();}
    }


    void setContext(const boost::any& context){context_ = context;}
    const boost::any& getContext()const{return context_;}
    boost::any*getMutableContext(){return &context_;}
    //为什么

    static EventLoop* getEventLoopOfCurrentThread();

    private:

    void abortNotInLoopThread();
    void handleRead();
    //为什么
    void doPendingFunctors();
    void printActiveChannels() const; 

    typedef std::vector<Channel*>ChannelList;

    //成员变量：
    
    //如下表示状态的：
    bool looping_;
    std::atomic<bool>quit_;
    bool eventHandling_;
    bool callingPendingFunctors_;

    //如下是具体数值的：
    int64_t iteration_;
    const pid_t threadId_;
    Timestamp pollReturnTime_;

    //指针指向timerqueue 与 poller
    //因为需要调用

    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;
    Channel* currentActiveChannel_;
    //只存一个活跃的channel
    boost::any context_;
    ChannelList activeChannels_;

    mutable MutexLock mutex_;
    std::vector<Functor> pendingFucntors_ GUARDED_BY(mutex_);
    

};

}
}




#endif  // NET_EVENTLOOP_H


