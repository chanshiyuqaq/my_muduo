cmake_minimum_required(VERSION 3.0)
project(my_muduo_net)

# 1. 设置 C++ 标准
# 使用 C++14 以支持更现代的特性，并匹配 muduo 的代码风格
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-g -Wall")

# 2. 定义路径变量
set(MUDUO_PATH "/home/wyk/桌面/muduo-master")
set(MUDUO_LIB_PATH "${MUDUO_PATH}/build/lib")

# 3. 头文件搜索路径
include_directories(
    ${PROJECT_SOURCE_DIR}           # 找到你的 net/Channel.h 等
    ${MUDUO_PATH}                   # 找到 muduo/base/Logging.h 等
)

# 4. 库文件搜索路径
link_directories(${MUDUO_LIB_PATH})

# 5. 收集源文件
# 收集 poller 目录
file(GLOB POLLER_SOURCES "${PROJECT_SOURCE_DIR}/net/poller/*.cc")

# 收集 net 目录，但排除掉所有可能包含 main 的测试文件
file(GLOB NET_LIB_SOURCES 
    "${PROJECT_SOURCE_DIR}/net/Channel.cc"
    "${PROJECT_SOURCE_DIR}/net/EventLoop.cc"
    "${PROJECT_SOURCE_DIR}/net/Poller.cc"
    "${PROJECT_SOURCE_DIR}/net/Timer.cc"
    "${PROJECT_SOURCE_DIR}/net/TimerQueue.cc"
    "${PROJECT_SOURCE_DIR}/net/SocketsOps.cc"
    "${PROJECT_SOURCE_DIR}/net/Socket.cc"
    "${PROJECT_SOURCE_DIR}/net/InetAddress.cc"
    "${PROJECT_SOURCE_DIR}/net/Acceptor.cc"
    
)

# 6. 生成可执行文件
# 明确指定 Channel_test.cc 作为入口
add_executable(timer_test 
    "${PROJECT_SOURCE_DIR}/net/Channel_test.cc"
    ${NET_LIB_SOURCES}
    ${POLLER_SOURCES}
)

# 7. 链接库
target_link_libraries(timer_test
    muduo_base    # 必须链接，因为你用了 Logging.h 和 Timestamp
    pthread       
    rt            
)
