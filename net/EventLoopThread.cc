#include <net/EventLoopThread.h>
#include <net/EventLoop.h>

using namespace muduo;
using namespace muduo::net;


//构造函数
EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,const string& name)
:
loop_(NULL),
exiting_(false),
thread_(std::bind(&EventLoopThread::threadFunc,this),name),
mutex_(),
cond_(mutex_),
callback_(cb)
{}

EventLoopThread::~EventLoopThread()
{
exiting_ = true;
if(loop_ !=NULL)        //线程退出
{
    loop_->quit();
    thread_.join();
}

}


EventLoop*EventLoopThread::startLoop()
{
    assert(!thread_.started());
    thread_.start();
    EventLoop*loop = NULL;
    {

        MutexLockGuard lock(mutex_);
        while(loop_==NULL)
        {
            cond_.wait();
        }
        loop = loop_;
    }
    return loop;

}



void EventLoopThread::threadFunc()
{
    EventLoop loop;
    if(callback_)
    {
        callback_(&loop);
    }
    {
        MutexLockGuard lock(mutex_);
        loop_ = &loop;
        cond_.notify();
    }
    loop.loop();
    MutexLockGuard lock(mutex_);
    loop_ = NULL;
}


