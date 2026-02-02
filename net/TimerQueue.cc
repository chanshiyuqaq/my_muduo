#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif


#include <net/TimerQueue.h>
#include<net/Timer.h>
#include<net/TimerId.h>
#include<net/EventLoop.h>
#include<muduo/base/Logging.h>

#include<sys/timerfd.h>
#include<unistd.h>


namespace muduo
{

namespace net
{

namespace detail
{

//创建定时器的文件描述符
int createTimerfd()
{
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC,TFD_NONBLOCK|TFD_CLOEXEC);
    if (timerfd<0)
    {LOG_SYSFATAL<<"failed in timerfd_create";}
    return timerfd;
}

//计算事件
struct timespec howMuchTimeFromNow(Timestamp when)
{
    int64_t microseconds = when.microSecondsSinceEpoch()-Timestamp::now().microSecondsSinceEpoch();
    if(microseconds < 100){microseconds = 100;}
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(microseconds/Timestamp::kMicroSecondsPerSecond);//取整数得到秒
    ts.tv_nsec = static_cast<long>(microseconds%Timestamp::kMicroSecondsPerSecond*1000);//得到纳秒
    return ts;
};

void readTimerfd(int timerfd , Timestamp now){
    //这里的读到底在读什么？
    //消耗超时信号，因为是定时器文件，读的是超时信号
    //n代表这里读出来的数据长度
    //howmany 读出来的东西放在这里
    uint64_t howmany;
    ssize_t n = ::read(timerfd,&howmany,sizeof howmany);
    LOG_TRACE << "handle read" <<howmany << "at" <<now.toString();
    if(n!=sizeof howmany)
    {
        LOG_ERROR << "TimerQueue handle read" << n << "instead of 8 ";
    }
}

void resetTimerfd(int timerfd,Timestamp expiration)
{
    //expiration 是超时时间
    struct itimerspec newValue;
    struct itimerspec oldValue;
    memZero(&newValue,sizeof newValue);
    memZero(&oldValue,sizeof oldValue);
    newValue.it_value = howMuchTimeFromNow(expiration);
    //这里讲其设置为 newValue 的it_value
    int ret = ::timerfd_settime(timerfd,0,&newValue,&oldValue);
    if(ret){LOG_SYSERR<<"timerfd_settime()";}
}

}
}
}

using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::detail;


TimerQueue::TimerQueue(EventLoop* loop):
loop_(loop),
timerfd_(createTimerfd()),
timerfdChannel_(loop,timerfd_),
timers_(),
callingExpiredTimers_(false)
{
    timerfdChannel_.setReadCallBack(std::bind(&TimerQueue::handleRead,this));
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    ::close(timerfd_);
}


TimerId TimerQueue::addTimer(TimerCallback cb, Timestamp when, double interval)
{
    std::unique_ptr<Timer> ptr_timer(new Timer(std::move(cb), when, interval));
    TimerId for_return(ptr_timer.get(), ptr_timer->sequence());
    
    Timer* raw_timer = ptr_timer.release();
    loop_->runInLoop([this, raw_timer]() {
        // 在 Loop 线程中重新接管所有权
        this->addTimerInLoop(std::unique_ptr<Timer>(raw_timer));
    });
    return for_return;
}

void TimerQueue::cancel(TimerId timerId)
{   
    loop_->runInLoop(std::bind(&TimerQueue::cancelInLoop,this,timerId));
}


//在loop中 添加定时器

void TimerQueue::addTimerInLoop(std::unique_ptr<Timer> ptrr)
{
    loop_->assertInLoopThread();
    Timestamp expiration = ptrr->expiration();       // 先记录时间点
    bool earliestChanged = insert(std::move(ptrr));  // 移动所有权
    
    if(earliestChanged){
        resetTimerfd(timerfd_, expiration);         // 使用记录好的时间
    }
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    
    //构造一个活跃 定时器 ， 用传入的timerId
    ActiveTimer timer(timerId.timer_,timerId.sequence_);
    
    ActiveTimerSet::iterator it(activeTimers_.find(timer));
    //从列表中找到
    //对于对象本身的删除 由智能指针来做
    //这里的删除 指的是在容器的删除
    
    //在activeTimers中找到了
    if(it!=activeTimers_.end())
    {
        Timer* ptr = it->first;
        Timestamp exp = ptr->expiration();
        //从 timers_ 中移除（主仓库）
        // 由于可能有多个定时器在同一微秒到期，我们需要在相同时间的范围内找指针匹配的!!!!
        auto range = timers_.lower_bound(std::make_pair(exp, std::unique_ptr<Timer>(nullptr)));
        while (range != timers_.end() && range->first == exp)
        {
        if (range->second.get() == ptr)
        {
            timers_.erase(range); 
            break;
        }
        ++range;
        }
        activeTimers_.erase(it);
    }
    else if(callingExpiredTimers_)
    {
        cancelingTimers_.insert(timer);
    }
    assert(timers_.size() == activeTimers_.size());

}


//当用户调用cancel的时候，会先判断在不在active中
//在的话直接删
//不在的话就是已经过期了
//将定时器指针放入cancelingTimers_中
//reset处理cancelingTimers_ ，将其中的周期性的放回timers_



void TimerQueue::handleRead()
{
    loop_->assertInLoopThread();
    Timestamp now(Timestamp::now());
    readTimerfd(timerfd_,now);
    //读取超时警告

    std::vector<Entry> expired = getExpired(now);   //这里有问题！复制了unique_ptr//没问题，内部会使用移动语义
    callingExpiredTimers_ = true;
    
    cancelingTimers_.clear();
    //这里为什么clear?
    //避免被上一次影响！


    for(const Entry&it:expired)
    {
        it.second->run();
    }
    callingExpiredTimers_ = false;
    reset(expired,now);

}


std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
    assert(timers_.size() == activeTimers_.size());
    std::vector<Entry> expired;

    // 修正哨兵对象的构造
    // unique_ptr 构造函数是 explicit 的，不能直接用原始指针强转
    // 我们只需要 Timestamp 用于比较，unique_ptr 部分给 nullptr 即可
    Entry sentry(now, std::unique_ptr<Timer>(nullptr));

    TimerList::iterator end = timers_.lower_bound(sentry);

    // 确保 end 之后的时间确实比 now 大（或者 end 到了末尾）
    assert(end == timers_.end() || now < end->first);

  
    for (auto it = timers_.begin(); it != end; ) {
    expired.push_back(std::move(const_cast<Entry&>(*it))); // 强制移动  // 这种写法最稳妥，强制转换 const 属性以允许 move
    timers_.erase(it++);
    }


   
    timers_.erase(timers_.begin(), end); // 从定时器仓库中移除（此时仓库里的 unique_ptr 已失效）

    // 修正索引表的删除
    for (const Entry& it : expired)
    {
        ActiveTimer timer(it.second.get(), it.second->sequence());
        size_t n = activeTimers_.erase(timer);
        assert(n == 1); (void)n;
    }

    assert(timers_.size() == activeTimers_.size());
    return expired;
}


bool TimerQueue::insert(std::unique_ptr<Timer> timer)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    
    bool earliestChanged = false;
    Timestamp when = timer->expiration();
    auto it = timers_.begin();
    
    // 检查新插入的定时器是否是所有定时器中最先到期的
    if (it == timers_.end() || when < it->first)
    {
        earliestChanged = true;
    }

    // 在 move 掉 unique_ptr 之前，先记下它的原始指针和序列号
    Timer* rawPtr = timer.get();
    int64_t seq = rawPtr->sequence();

    //unique_ptr move到 timers_ 
    auto result = timers_.insert(Entry(when, std::move(timer)));
    assert(result.second); (void)result;

    //插入active
    auto result2 = activeTimers_.insert(ActiveTimer(rawPtr, seq));
    assert(result2.second); (void)result2;

    assert(timers_.size() == activeTimers_.size());
    return earliestChanged;
}

void TimerQueue::reset(std::vector<Entry>& expired, Timestamp now)
{
    Timestamp nextExpire;

    for (Entry& it : expired)
    {
        // 构造 ActiveTimer 用于查找
        ActiveTimer timer(it.second.get(), it.second->sequence());
        
        if (it.second->repeat() && cancelingTimers_.find(timer) == cancelingTimers_.end())
        {
            //重新放回容器
            it.second->restart(now);
            insert(std::move(it.second));
        }
        else
        {
            //expired被析构后自然被析构
        }
    }

    // 更新下一次内核触发时间
    if (!timers_.empty())
    {
        nextExpire = timers_.begin()->second->expiration();
    }

    if (nextExpire.valid())
    {
        resetTimerfd(timerfd_, nextExpire);
    }
}





