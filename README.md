# Tiny HttpServer


## Introduction  

本项目为C++编写的轻型HTTP服务器，解析了HTTP请求，并返回静态资源。

## Envoirment  
* OS: Ubuntu 20.04
* Complier: gcc 9.3.0

## Build

```shell
mkdir build
cd build
cmake ..
make
```

## Usage

```shell
./WebServer ip_address port
```

## Technical points
* 整体框架采用半同步/半反应堆线程池，主线程采用异步模式向任务队列添加任务，多个工作线程进行锁竞争从任务队列中消费http连接，并进行具体的读写业务

* 采用主从状态机解析请求头，主状态机调用从状态机，从状态机分析请求行交由主状态机决策http请求状态
* IO模型采用epoll边缘触发模式的多路复用，非阻塞式IO
* 实现小根堆的定时器关闭超时连接请求
*  在请求文件时采用mmap内存映射方式，避免程序陷入内核态再从内核态向用户态拷贝文件

## Model

并发模型为半同步/半反应堆+非阻塞IO+线程池，详细介绍请参考《Linux高性能服务器编程》

![half_sync_half_reactive.png](https://github.com/NebulorDang/HttpServer/blob/master/citeImages/half_sync_half_reactive_2.png?raw=true)

![half_sync_half_reactive.png](https://github.com/NebulorDang/HttpServer/blob/master/citeImages/half_sync_half_reactive.png?raw=true)

![half_sync_half_reactive.png](https://github.com/NebulorDang/HttpServer/blob/master/citeImages/half_sync_half_reactive_3.png?raw=true)

## HTTP server benchmarking tool

[Webbench](https://github.com/NebulorDang/HttpServer/tree/master/WebBench)添加Keep-Alive选项和测试功能

## Others

- version_0.1中实现了进程池cgi服务器

![processPool.png](https://github.com/NebulorDang/HttpServer/blob/master/citeImages/processPool.png?raw=true)

![processPool_2.png](https://github.com/NebulorDang/HttpServer/blob/master/citeImages/processPool_2.png?raw=true)

## Reference

### [《Linux高性能服务器编程》游双著](https://book.douban.com/subject/24722611/)

![Linux高性能服务器编程](https://img1.doubanio.com/view/subject/s/public/s27327287.jpg)

### [《Linux多线程服务端编程》陈硕著](https://book.douban.com/subject/20471211/)

![Linux多线程服务端编程](https://img1.doubanio.com/view/subject/s/public/s24522799.jpg)
