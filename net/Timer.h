#ifndef NET_TIMER_H
#define NET_TIMER_H

#include<muduo/base/Atomic.h>
#include<muduo/base/Timestamp.h>
#include<muduo/base/noncopyable.h>
#include<net/Callbacks.h>

namespace muduo
{
namespace net
{

class Timer:noncopyable
{
public:
    Timer(TimerCallback cb,Timestamp when,double interval)
    :
    callback_(std::move(cb)),
    expiration_(when),
    interval_(interval),
    repeat_(interval > 0.0),
    sequence_(s_numCreated_.incrementAndGet())
    {}

    void run() const{callback_();}
    Timestamp expiration()const{return expiration_;}
    int64_t sequence()const{return sequence_;}

    void restart(Timestamp now);
    bool repeat() const { return repeat_; }
    static int64_t numCreated(){return s_numCreated_.get();}

private:

    const TimerCallback callback_;  //一个定时器有一个回调
    Timestamp expiration_;      //时间戳类型的过期事件
    const double interval_;
    const bool repeat_;
    const int64_t sequence_;
    static AtomicInt64 s_numCreated_;
};


}

}







#endif //NET_TIMER_H