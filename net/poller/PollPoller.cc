#include<net/poller/PollPoller.h>
#include<net/Channel.h>
#include<muduo/base/Logging.h>
#include<muduo/base/Types.h>

#include<assert.h>
#include<errno.h>
#include<poll.h>

using namespace muduo;
using namespace muduo::net;

PollPoller::PollPoller(EventLoop*loop):Poller(loop){}
//调用基类的初始化构造

PollPoller::~PollPoller() = default;

Timestamp PollPoller::poll(int timeoutMs,ChannelList*activeChannels)
{
    int numEvents = ::poll(&*pollfds_.begin(),pollfds_.size(),timeoutMs);
    int savedErrno = errno;
    Timestamp now(Timestamp::now());
    if(numEvents>0){
        LOG_TRACE<<numEvents<<" events happened";
        fillActiveChannels(numEvents,activeChannels);
    }
    else if(numEvents == 0){
        LOG_TRACE<<" nothing happened";
    }
    else{
        if(savedErrno!=EINTR){
            errno = savedErrno; //在error.h中定义的
            LOG_SYSERR<<" PollPoller::poll()";
        }
    }
    return now;
}
//返回的是一个时间戳类型 ， 考虑为什么要返回一个时间类型？
//这里返回的时间是poll执行完的时间


void PollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels)const{

    for(PollFdList::const_iterator pfd = pollfds_.begin();pfd!=pollfds_.end() && numEvents>0;++pfd){
        if(pfd->revents>0)  //当前文件描述符有事件
        {
            --numEvents;
            ChannelMap::const_iterator ch = channels_.find(pfd->fd);    //map里面 键fd值channel
            assert(ch!=channels_.end());
            Channel*channel = ch->second;
            assert(channel->fd()==pfd->fd);
            channel->set_revents(pfd->revents);
            //将文件描符对应的事件 填入 channel中
            activeChannels->push_back(channel);
            //经过这个过程得到了active channel
        }
    }

}

void PollPoller::updateChannel(Channel*channel){
    Poller::assertInloopThread();
    LOG_TRACE<<"fd="<<channel->fd()<<"events = "<<channel->events();
    if(channel->index()<0){
        //这是一个新的channel,将其对应的文件描述符放入我关心的pollfds_中
        assert(channels_.find(channel->fd())==channels_.end());
        struct pollfd pfd;
        pfd.fd = channel->fd();  
        pfd.events = static_cast<short>(channel->events());
        pfd.revents = 0;
        pollfds_.push_back(pfd);
        int idx = static_cast<int>(pollfds_.size())-1;
        channel->set_index(idx);
        channels_[pfd.fd] = channel;
    }
    else{
        assert(channels_.find(channel->fd())!=channels_.end());
        assert(channels_[channel->fd()] == channel);
        int idx = channel->index();
        //取出来
        assert(idx>=0 && idx<static_cast<int>(pollfds_.size()));
        struct pollfd& pfd = pollfds_[idx];
        //为什么？这里的索引会不会变 , 通过交换 ， 删除删除改，实现即使删除了也能正常存储
        assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd()-1);
        pfd.fd = channel->fd();  
        pfd.events = static_cast<short>(channel->events());
        pfd.revents = 0;
        if(channel->isNoneEvent()){pfd.fd = -channel->fd()-1;}
        //停止监控一个 Channel（例如调用了 channel->disableAll()），
        //但又不想立刻从 pollfds_ 这个 std::vector 中删除该元素时，
        //最快的方法就是把它的 fd 改为负数
    }
}


void PollPoller::removeChannel(Channel*channel){

    Poller::assertInloopThread();
    LOG_TRACE<<"fd="<<channel->fd();
    assert(channels_.find(channel->fd())!=channels_.end());
    assert(channels_[channel->fd()] == channel);
    assert(channel->isNoneEvent());

    int idx = channel->index();
    assert(idx>=0 && idx< static_cast<int>(pollfds_.size()));
    const struct pollfd& pfd = pollfds_[idx];
    (void)pfd;
    
    assert(pfd.fd == (-channel->fd()-1) && pfd.events == channel->events());
    size_t n = channels_.erase(channel->fd());
    //将map映射关系删除
    assert(n==1); (void)n;
    if(implicit_cast<size_t>(idx) == pollfds_.size()-1){pollfds_.pop_back();}
    //在尾部的话直接popback删除
    else{
        int channelAtEnd = pollfds_.back().fd;
        iter_swap(pollfds_.begin()+idx,pollfds_.end()-1);
        if(channelAtEnd<0){channelAtEnd = -channelAtEnd-1;}
        channels_[channelAtEnd]->set_index(idx);    //要去除的idx变成你的idx了，要去除的直接popback()
        pollfds_.pop_back();
    }

}


