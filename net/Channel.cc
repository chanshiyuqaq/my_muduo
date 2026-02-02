#include<net/Channel.h>
#include<net/EventLoop.h>
#include<muduo/base/Logging.h>

#include<sstream>
#include<poll.h>


using namespace muduo;
using namespace muduo::net;


//定义表示事件类型的值

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop*loop,int fd__):
loop_(loop),
fd_(fd__),
events_(0),
revents_(0),
index_(-1),
logHup_(true),
tied_(false),
eventHandling_(false),
addedToLoop_(false)
{ }

Channel::~Channel(){
    assert(!eventHandling_);
    assert(!addedToLoop_);
    if(loop_->isInLoopThread()){assert(!loop_->hasChannel(this));}
    //意思是 当执行channel的析构的时候，ta一定是不在任何的loop里面的

}

void Channel::tie(const std::shared_ptr<void>& obj){tie_ = obj;tied_ = true;}
    //这里的tie是一个弱指针，表示绑定到某个东西上


void Channel::update(){addedToLoop_ = true; loop_->updateChannel(this);}

void Channel::remove(){
    assert(isNoneEvent()); 
    //这里要求显式调用 disableAll()
    addedToLoop_ = false;
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    std::shared_ptr<void>guard;
    if(tied_){
        guard = tie_.lock();
        if(guard){handleEventWithGuard(receiveTime);}
        //这里判断 只有升级成功才能继续handle
        //如果升级失败，说明
    }
    else{
        //此分支也能继续执行，因为有些channel不需要tie_到别的线程
        handleEventWithGuard(receiveTime);
    }
}


void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    eventHandling_ = true;
    LOG_TRACE << reventsToString();

    //处理写回调
    if(revents_&POLLOUT){if(writeCallBack_)writeCallBack_();}
    //处理读回调
    if(revents_&(POLLIN|POLLPRI|POLLRDHUP)){if(readCallBack_)readCallBack_(receiveTime);}
    //处理关闭回调
    if((revents_&POLLHUP)&&!(revents_&POLLIN)){
        if(logHup_){  LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLHUP";}
        if(closeCallBack_){closeCallBack_();}
    }
    //处理错误回调
    if(revents_&(POLLERR | POLLNVAL)){if(errorCallBack_){errorCallBack_();}}
    //写入日志
    if (revents_ & POLLNVAL){LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLNVAL";}

    eventHandling_ = false;

}



//用字符串的方式返回revents/events的状态
string Channel::reventsToString()const{return eventsToString(fd_,revents_);}
string Channel::eventsToString()const{return eventsToString(fd_,events_);}

string Channel::eventsToString(int fd,int ev){
    std::ostringstream oss;
    oss<<fd<<":";
    if(ev&POLLIN)oss<<"IN ";
    if(ev&POLLPRI)oss<<"PRI ";
    if(ev&POLLOUT)oss<<"OUT ";
    if(ev&POLLHUP)oss<<"HUP ";
    if(ev&POLLRDHUP)oss<<"RDHUP ";
    if(ev&POLLERR)oss<<"ERR";
    if(ev&POLLNVAL)oss<<"NVAL ";
    return oss.str();
}