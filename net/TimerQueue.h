#ifndef NET_TIMERQUEUE_H
#define NET_TIMERQUEUE_H

#include<set>
#include<vector>

#include<muduo/base/Mutex.h>
#include<muduo/base/Timestamp.h>
#include<net/Channel.h>
#include<net/Callbacks.h>
#include<memory>


namespace muduo
{

namespace net
{

//前向定义类

class EventLoop;
class Timer;
class TimerId;


class TimerQueue : noncopyable
{

public:
    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    TimerId addTimer(TimerCallback cb,Timestamp when,double interval);
    void cancel(TimerId timerId);
    typedef std::pair<Timestamp,std::unique_ptr<Timer>>Entry;
    typedef std::set<Entry>TimerList;
    typedef std::pair<Timer*,int64_t>ActiveTimer;
    //这里使用pair是防止重复
    typedef std::set<ActiveTimer>ActiveTimerSet;
private:
  

    void addTimerInLoop(std::unique_ptr<Timer> timer);
    void cancelInLoop(TimerId TimerId);
    void handleRead();
    std::vector<Entry> getExpired(Timestamp now);//传进来当前时间，得到一个过期定时器的数组
    void reset(std::vector<Entry>& expired,Timestamp now);
    //?将过期的定时器做什么处理？
    //答：查看是不是周期性的定时器
    bool insert(std::unique_ptr<Timer>timer);


    //成员变量
    EventLoop*loop_;
    const int timerfd_;
    Channel timerfdChannel_;
    TimerList timers_;

    ActiveTimerSet activeTimers_;
    bool callingExpiredTimers_;
    ActiveTimerSet cancelingTimers_;

    //具体删除细节？
};

}
}


#endif //NET_TIMERQUEUE_H


