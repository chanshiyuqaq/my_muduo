
#include <net/EventLoop.h>
#include <net/EventLoopThread.h>
#include <net/EventLoopThreadPool.h>



#include<stdio.h>


using namespace muduo;
using namespace muduo::net;


EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop,const string&nameArg)
:
baseLoop_(baseLoop),
name_(nameArg),
started_(false),
numThreads_(0),
next_(0)
{}

EventLoopThreadPool::~EventLoopThreadPool(){}


//线程池相关代码。。。。
