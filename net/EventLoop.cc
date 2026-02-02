#include<muduo/base/Logging.h>
#include<muduo/base/Mutex.h>

#include<net/EventLoop.h>
#include<net/Channel.h>
#include<net/Poller.h>
#include<net/SocketsOps.h>
#include<net/TimerQueue.h>
#include<net/TimerId.h>



#include<algorithm>

#include<signal.h>
#include<sys/eventfd.h>
#include<unistd.h>


using namespace muduo;
using namespace muduo::net;

namespace 
{
    __thread EventLoop* t_loopInThisThread = 0;
    const int kPollTimeMs = 10000;
    int createEventfd()
    {
        int evtfd = ::eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC);
        if(evtfd<0)
        {
            LOG_SYSERR << "Failed in eventfd";
            abort();
        }
        return evtfd;
    }
    #pragma GCC diagnostic ignored "-Wold-style-cast"
    class IgnoreSigPipe
    {
        public:
        IgnoreSigPipe(){::signal(SIGPIPE,SIG_IGN);}
    };
    #pragma GCC diagnostic error "-Wold-style-cast"

    IgnoreSigPipe initObj;
}


EventLoop* EventLoop::getEventLoopOfCurrentThread(){return t_loopInThisThread;}

EventLoop::EventLoop():
looping_(false),
quit_(false),
eventHandling_(false),
callingPendingFunctors_(false),
iteration_(0),
threadId_(CurrentThread::tid()),
poller_(Poller::newDefaultPoller(this)),
timerQueue_(new TimerQueue(this)),
wakeupFd_(createEventfd()),
wakeupChannel_(new Channel(this,wakeupFd_)),
currentActiveChannel_(NULL)

{

LOG_DEBUG << "EventLoop created"<< this << "in thread"<< threadId_;
if(t_loopInThisThread){

    LOG_FATAL <<"Another EventLoop"<<t_loopInThisThread <<"exists in this thread"<<threadId_;
}
else{t_loopInThisThread = this;}

wakeupChannel_->setReadCallBack(std::bind(&EventLoop::handleRead, this));
wakeupChannel_->enableReading();

}


EventLoop::~EventLoop(){

    LOG_DEBUG << "EventLoop"<<this<<"of thread"<<threadId_ << "destrucrts in thread "<< CurrentThread::tid();

    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = NULL;

}


void EventLoop::loop()
{
    assert(!looping_);
    assertInLoopThread();
    looping_ = true;
    quit_ = false;

    while(!quit_){

        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs,&activeChannels_);
        ++iteration_;
        if(Logger::logLevel()<=Logger::TRACE){printActiveChannels();}
        eventHandling_ = true;
        //开始处理事件
        for(Channel*channel : activeChannels_){
            currentActiveChannel_ = channel;
            currentActiveChannel_->handleEvent(pollReturnTime_);
        }
        currentActiveChannel_ = NULL;
        eventHandling_ = false;
        doPendingFunctors();
    }
    LOG_TRACE<<"EventLoop "<<this<<"stop looping";
    looping_ = false;
}

void EventLoop::quit()
{
    quit_ = true;
    if(!isInLoopThread()){wakeup();}
}

void EventLoop::runInLoop(Functor cb){

    if(isInLoopThread()){cb();}
    else{

        queueInLoop(std::move(cb));
        //解释一下移动语义？
        //
    }
}

void EventLoop::queueInLoop(Functor cb)
{
    {
        MutexLockGuard lock(mutex_);
        pendingFucntors_.push_back(std::move(cb));
    }
    if(!isInLoopThread()||callingPendingFunctors_)
    {
        wakeup();
    }

}


size_t EventLoop::queueSize()const
{
    MutexLockGuard lock(mutex_);
    return pendingFucntors_.size();
}


TimerId EventLoop::runAt(Timestamp time,TimerCallback cb)
{
    return timerQueue_->addTimer(std::move(cb),time,0.0);
}
TimerId EventLoop::runAfter(double delay,TimerCallback cb)
{
    Timestamp time(addTime(Timestamp::now(), delay));
    return runAt(time,std::move(cb));
}

TimerId EventLoop::runEvery(double interval,TimerCallback cb)
{
    Timestamp time(addTime(Timestamp::now(),interval));
    return timerQueue_->addTimer(std::move(cb),time,interval);
}

void EventLoop::cancel(TimerId timeId)
{
    return timerQueue_->cancel(timeId);
}

void EventLoop::updateChannel(Channel* channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    if(eventHandling_){
        assert(currentActiveChannel_ == channel ||          //
            //这里为什么？ 是合理的，因为遍历的active , 删除的是channelists
            std::find(activeChannels_.begin(),activeChannels_.end(),channel)==activeChannels_.end());
    }
    poller_->removeChannel(channel);

}

bool EventLoop::hasChannel(Channel*channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    return poller_->hasChannel(channel);
}

void EventLoop::abortNotInLoopThread(){
    
    LOG_FATAL<<"event loop"<<this<<"was created om"<<threadId_<<"current thread:"<<CurrentThread::tid();

}
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = sockets::write(wakeupFd_,&one,sizeof one);
    if(n!=sizeof one)
    {
        LOG_ERROR <<" wakeup writes"<<n<<"bytes instead of 8";
    }
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = sockets::write(wakeupFd_,&one,sizeof one);
    if (n != sizeof one)
    {
    LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
    }
}

void EventLoop::doPendingFunctors()
{
    std::vector<Functor>functors;
    callingPendingFunctors_ = true;
    {
        MutexLockGuard lock(mutex_);
        functors.swap(pendingFucntors_);
    }
    for(const Functor& functor : functors)
    {
        functor();
    }
    callingPendingFunctors_ = false;
}

void EventLoop::printActiveChannels()const
{
    for(const Channel*channel : activeChannels_){
        LOG_TRACE<<"{"<<channel->reventsToString()<<"}";
    }

}