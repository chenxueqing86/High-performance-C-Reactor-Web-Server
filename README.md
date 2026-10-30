#Reactor 网络库

# High-performance-C-Reactor-Web-Server
基于Reactor模型的C++高性能Web服务器

## ✨ 功能特性
- Reactor 事件驱动模型，epoll ET边缘触发
- 多Reactor + 线程池
- HTTP解析，支持GET/POST
- 异步日志模块
- 静态资源访问
- 连接超时定时器

## 编译运行
一个基于C++14 和 epoll 的单线程 Reactor 网络库，支持 TCP 长连接、长度头协议、定时器、跨线程任务投递、并附带 HTTP 静态响应示例。

##特性
- 事件驱动 （epoll + Reactor）
- 非阻塞 IO，正确处理 EAGAIN
- 自动扩容的 Buffer
- 长度头协议（4字节网络序 + 数据体），解决粘包/拆包
- 定时器（基于timefd）:`runAfter` / `runEvery`
- 跨线程任务投递（eventfd + 任务队列）
- HTTP静态相应（支持压测）

## 编译与运行
```bash
mkdir build && cd build
cmake ..
make
./examples/echo_http_server
```

压测：
```bash
wrk -t 2 -c 10 -d 5s http://127.0.0.1:8889/

二、架构设计
详见 docs/design.md

三、性能
在 WSL环境下，单线程QPS 约2200（响应 Hello,World!）.原生Linux 估计性能更高
