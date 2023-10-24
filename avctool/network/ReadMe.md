# Socket模块需求分析
## Socket用来干什么
### 实现UDP协议
#### UDP客户端
#### UDP服务端

### 实现TCP协议
#### TCP客户端
#### TCP服务端
##### Acceptor
    Tcp服务端，需要使用Socket实现Acceptor，用于监听ip:port以及接受Tcp链接
#### TCP链接


## Socket发送逻辑
### 发送情形
    1）需要指定目标地址的sendto
    2）不需要指定目标地址的send
