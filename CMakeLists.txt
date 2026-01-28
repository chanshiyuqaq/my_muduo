cmake_minimum_required(VERSION 3.0)
project(my_muduo_net)

# 1. 设置 C++ 标准和编译选项
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-g -Wall")

# 2. 定义路径变量
set(MUDUO_PATH "/home/wyk/桌面/muduo-master")
set(MUDUO_LIB_PATH "${MUDUO_PATH}/build/lib")

# 3. 头文件搜索路径
include_directories(
    ${PROJECT_SOURCE_DIR}           # 代码根目录
    ${MUDUO_PATH}                   # 原始 muduo 目录
)

# 4. 库文件搜索路径
link_directories(${MUDUO_LIB_PATH})

# 5. 自动收集源文件
aux_source_directory(./net SRC_LIST)

# 6. 生成可执行文件
add_executable(net_test ${SRC_LIST})

# 7. 链接必要的库
target_link_libraries(net_test
    muduo_base    # 链接已经编译好的 base 库
    pthread       # 线程库
    rt            # 实时库（Timestamp 等需要）
)

