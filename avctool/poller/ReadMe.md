# EventPoller是什么
## 提供接口轮询是否发生I/O事件，通常是网络I/O
    例如使用select或则epoll_wait轮询
## 由于select或epoll_wait通常与时间（唤醒超时有关）因此也尝尝提供定时器功能

# EventPoller需求设计
## 网络I/O事件
## 定时器事件

## 如何初始化
## 如何退出
## 如何交互

