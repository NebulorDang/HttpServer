//
// Created by NebulorDang on 2021/12/21.
// 线程池实现的主函数
//

#include "Locker.h"
#include "ThreadPool.h"
#include "HttpConnection.h"
#include "TimeHeap.h"

#define MAX_FD 65535
#define MAX_EVENT_NUMBER 10000

extern int addFd(int epoll_fd, int fd, bool one_shot);
extern int delFd(int epoll_fd, int fd);

Locker timeHeapLock;
TimeHeap timeHeap(100);

void addSig(int sig, void (*handler)(int), bool restart = true){
    struct sigaction sa;
    memset(&sa, 0, sizeof (sa));
    sa.sa_handler = handler;
    if(restart){
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void show_error(int conn_fd, const char *info){
    printf("%s", info);
    send(conn_fd, info, strlen(info), 0);
    close(conn_fd);
}

void handle_expired_conn(){
    timeHeapLock.lock();
    while (!timeHeap.empty()){
        Timer* timer = timeHeap.top();
        if(!timer->isvalid()){
            //过期了
            if(timer->conn){
                timer->conn->close_conn();
            }
            timeHeap.pop_timer();
        }else{
            break;
        }
    }
    timeHeapLock.unlock();
}

int main(int argc, char *argv[]){
    if(argc < 2){
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    //忽略SIGPIPE信号
    addSig(SIGPIPE, SIG_IGN);

    //创建线程池
    ThreadPool<HttpConnection> *pool = NULL;
    try{
        pool = new ThreadPool<HttpConnection>();
    }catch (...){
        return 1;
    }

    //预先为每个可能的客户连接分配一个HttpConnection对象,最大65535个客户连接
    HttpConnection *users = new HttpConnection[MAX_FD];
    assert(users);
    int user_count = 0;

    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listen_fd >= 0);
    struct linger tmp = {1, 0};
    setsockopt(listen_fd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));

    int ret = 0;
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    ret = bind(listen_fd, (struct sockaddr*)&address, sizeof (address));
    assert(ret >= 0);

    ret = listen(listen_fd, 5);
    assert(ret >= 0);

    epoll_event events[MAX_EVENT_NUMBER];
    int epoll_fd = epoll_create(5);
    assert(epoll_fd != -1);
    addFd(epoll_fd, listen_fd, false);
    HttpConnection::m_epoll_fd = epoll_fd;

    while(true){
        int number = epoll_wait(epoll_fd, events, MAX_EVENT_NUMBER, -1);
        if((number < 0) && errno != EINTR){
            printf("epoll failure\n");
            break;
        }

        for(int i = 0; i < number; ++i){
            int sock_fd = events[i].data.fd;
            //如果是监听描述符号则会初始化客户端连接，此时要在这个连接被EPOLLIN之前添加计时器
            if(sock_fd == listen_fd){
                struct sockaddr_in client_address;
                socklen_t client_addr_len = sizeof (client_address);
                int conn_fd = accept(listen_fd, (sockaddr*)&client_address, &client_addr_len);
                if(conn_fd < 0){
                    printf("errno is : %d\n", errno);
                    continue;
                }
                if(HttpConnection::m_user_count >= MAX_FD){
                    show_error(conn_fd, "Internal server busy");
                    continue;
                }
                //新建定时器
                Timer* timer = new Timer(100000, &users[conn_fd]);
                //将timer指针保存在HttpConnection中，方便通过HttpConnection直接获取它对应的timer
                users[conn_fd].setTimer(timer);
                //放入事件堆，开始计时
                users[conn_fd].startTimer(timer);

                //初始化客户连接
                users[conn_fd].init(conn_fd, client_address);
                //新增时间信息
            }else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                //如果有异常，直接关闭客户连接
                users[sock_fd].close_conn();
            }else if(events[i].events & EPOLLIN){
                //根据读的结果，决定是将任务加到线程池还是关闭连接
                if(users[sock_fd].read()){
                    //在放入线程池之前先要解绑HttpConnection与timer
                    //防止在读取时由于超时而中途关闭连接
                    //但是在HttpConnection重置连接时又需要重新绑定
                    users[sock_fd].separateTimer();
                    pool->append(users + sock_fd);
                }else{
                    users[sock_fd].close_conn();
                }
            }else if(events[i].events & EPOLLOUT){
                //根据写的结果，决定是否关闭连接
                if(!users[sock_fd].write()){
                    users[sock_fd].close_conn();
                }
            }else{

            }
        }
        handle_expired_conn();
    }
    close(epoll_fd);
    close(listen_fd);
    delete [] users;
    delete pool;
    return 0;
}