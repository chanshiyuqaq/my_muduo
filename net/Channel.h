#ifndef NET_CHANNEL_H
#define NET_CHANNEL_H

#include<muduo/base/Timestamp.h>
#include<muduo/base/noncopyable.h>

#include<functional> //对象包装器
#include<memory>     //智能指针

namespace muduo //大型项目中使用命名空间，避免冲突
{
namespace net
{
class EventLoop; //使用前向声明，避免头文件重复包含

class Channel:noncopyable
{
public:    
    typedef std::function<void()> EventCallback;
    typedef std::function<void(Timestamp)> ReadEventCallback; 
    //这里为什么读事件类型的回调需要一个事件戳参数？

    Channel(EventLoop* loop,int fd);
    ~Channel();

    void handleEvent(Timestamp receiveTime);
    //设置当发生可读，可写，关闭，错误 时的回调函数
    void setReadCallBack(ReadEventCallback cb){readCallBack_ = std::move(cb);}
    void setWriteCallBack(EventCallback cb){writeCallBack_ = std::move(cb);}
    void setCloseCallBack(EventCallback cb){closeCallBack_ = std::move(cb);}
    void setErrorCallBack(EventCallback cb){errorCallBack_ = std::move(cb);}


    //使用智能指针 管理声明周期
    void tie(const std::shared_ptr<void>&); //这里使用const 修饰指针 指针不能变 指针指向的对象可以变

    //常返回函数
    int fd()const{return fd_;}
    int events()const{return events_;}
    int set_revents(int revt){revents_ = revt;}

    //返回 是不是 在监控 某事件类型
    bool isNoneEvent()const{return events_==kNoneEvent;}
    bool isWriting()const{return events_&kWriteEvent;}
    bool isReading()const{return events_&kReadEvent;}

    //设置 监控 某事件类型
    void enableReading(){events_|=kReadEvent;update();}
    void enableWriting(){events_|=kWriteEvent;update();}
    void disableWriting(){events_|=~kWriteEvent;update();}
    void disableReading(){events_|=~kReadEvent;update();}
    void disableAll(){events_ = kNoneEvent;update();}

    int index(){return index_;}
    void set_index(int idx){index_ = idx;}

    string reventsToString()const;
    string eventsToString()const;

    void doNotLogHup(){logHup_ = false;}
    EventLoop* ownerLoop(){return loop_;}

    void remove();

private:

    static string eventsToString(int fd,int ev);
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;


    EventLoop* loop_;
    const int fd_;
    int events_;    //期望
    int revents_;   //epoll或poll返回的
    int index_;     //供poll使用
    
    bool logHup_;
    //为什么

    std::weak_ptr<void> tie_;
    bool tied_;
    bool eventHandling_;
    bool addedToLoop_;

    ReadEventCallback readCallBack_;
    EventCallback writeCallBack_;
    EventCallback closeCallBack_;
    EventCallback errorCallBack_;

};
}

}
#endif // NET_CHANNEL_H