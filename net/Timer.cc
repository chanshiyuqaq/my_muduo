#include "Timer.h" 

namespace muduo
{
namespace net
{

// 静态变量初始化
AtomicInt64 Timer::s_numCreated_;

// 成员函数实现
void Timer::restart(Timestamp now)
{
    if (repeat_)
    {
        expiration_ = addTime(now, interval_);
    }
    else
    {
        expiration_ = Timestamp::invalid();
    }
}

} // namespace net
} // namespace muduo
