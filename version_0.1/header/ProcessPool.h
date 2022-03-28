//
// Created by NebulorDang on 2021/12/16.
// 进程池的实现
// 采用图8-11所示的半同步/半异步并发模式的进程池
// 实际上父进程和子进程都维持着自己的事件循环每一个进程都工作在异步模式，所以并非严格的半同步/半异步模式。

#ifndef WEBSERVER_PROCESSPOOL_H
#define WEBSERVER_PROCESSPOOL_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//子进程类,m_pid是子进程的pid,m_pipe_fd是父进程与子进程通信用的管道
class Process
{
public:
    //初始化m_pid为-1，代表没有被使用
    Process() : m_pid(-1){}
public:
    pid_t m_pid;
    int m_pipe_fd[2];
};

//进程池类，定义为模板类是为了代码复用，模板参数是处理逻辑任务的类
template<typename T>
class ProcessPool{
private:
    //将构造函数私有，为确保只能通过create静态函数来创建ProcessPool实例
    ProcessPool(int listen_fd, int process_number = 8);
public:
    //单例模式
    //返回类型是模板参数类型为T的ProcessPool类对象指针
    static ProcessPool<T> * create(int listen_fd, int process_number = 8){
        //如果存在则直接返回
        if(!m_instance){
            m_instance = new ProcessPool<T>(listen_fd, process_number);
        }
        return m_instance;
    }

    ~ProcessPool(){
        delete [] m_sub_process;
    }

    //启动线程池
    void run();

private:
    //建立信号管道
    void setup_sig_pipe();
    void run_parent();
    void run_child();

private:
    //进程池静态实例
    static ProcessPool<T> *m_instance;

private:
    //进程允许的最大子进程数量
    static const int MAX_PROCESS_NUMBER = 16;
    //每个子进程最多能处理的客户数量
    static const int USER_PER_PROCESS = 65536;
    //epoll最多能处理的事件数量
    static const int MAX_EVENT_NUMBER = 10000;
    //进程池的进程总数
    int m_process_number;
    //子进程在池中的序号，从0开始
    int m_idx;
    //每个进程都有一个专用的epoll专用的文件描述符
    int m_epoll_fd;
    //监听socket描述符
    int m_listen_fd;
    //进程通过m_stop决定是否停止运行
    int m_stop;
    //保存所有子进程的描述信息保存了哪些信息
    Process *m_sub_process;
};

template<typename T>
ProcessPool<T> *ProcessPool<T>::m_instance = NULL;

//用于处理信号的管道（信号管道），以实现统一事件源
static int sig_pipe_fd[2];

static int setNonBlocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return new_option;
}

//向epoll中添加描述符，且要设定监听的描述符上的事件
static void addFd(int epoll_fd, int fd){
    epoll_event event;
    event.data.fd = fd;
    //以ET方式触发
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    //此处设定epoll监听的文件描述符为非阻塞，因为是ET模式
    setNonBlocking(fd);
}

static void delFd(int epoll_fd, int fd){
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//信号处理函数，通过信号管道发送到子进程，让子进程处理
static void sig_handler(int sig){
    //保留原来的errno，在函数最后恢复，以保证函数的可重入性
    int save_errno = errno;
    int msg = sig;
    send(sig_pipe_fd[1], (char *)&msg, 1,0);
    errno = save_errno;
}

static void addSig(int sig, void (*handler)(int), bool restart = true){
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    if(restart){
        //由此信号中断的系统调用自动重启
        sa.sa_flags |= SA_RESTART;
    }
    //sigfillset与sigemptyset相反，将所有信号添加到信号集，除了mask之外
    sigfillset(&sa.sa_mask);
    //添加信号
    assert(sigaction(sig, &sa, NULL) != -1);
}

//进程池构造函数。参数listen_fd需要在创建进程池之前创建，否则子进程无法直接
//引用它，参数process_num指定进程池中子进程的数量

template<typename T>
ProcessPool<T>::ProcessPool(int listen_fd, int process_number)
        : m_listen_fd(listen_fd), m_process_number(process_number), m_idx(-1), m_stop(false)
{
    assert((process_number > 0) && process_number <= MAX_PROCESS_NUMBER);

    m_sub_process = new Process[process_number];
    assert(m_sub_process);

    //建立process_number个子进程，并建立他们和父进程之间的管道
    for(int i = 0; i < process_number; i++){
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_pipe_fd);
        assert(ret == 0);

        m_sub_process[i].m_pid = fork();
        //只能是子进程或者父进程
        assert(m_sub_process[i].m_pid >= 0);
        if(m_sub_process[i].m_pid > 0){
            //父进程只写,子进程只读，于是关闭父进程中管道的读端口
            close(m_sub_process[i].m_pipe_fd[0]);
        }else{
            close(m_sub_process[i].m_pipe_fd[1]);
            //每个进程都是fork的于是都拥有一个进程池,于是每个子进程都需要设置m_idx为自己
            m_idx = i;
            //因为是子进程，所以需要跳出循环
            break;
        }
    }
}

//统一事件源,进程收到的信号也由epoll监听
template<typename T>
void ProcessPool<T>::setup_sig_pipe() {
    //创建epoll文件描述符
    m_epoll_fd = epoll_create(5);
    assert(m_epoll_fd != -1);

    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipe_fd);
    assert(ret != -1);

    setNonBlocking(sig_pipe_fd[1]);
    addFd(m_epoll_fd, sig_pipe_fd[0]);

    //设置信号处理函数
    addSig(SIGCHLD, sig_handler);
    addSig(SIGTERM, sig_handler);
    addSig(SIGINT, sig_handler);
    //往一个读端关闭的管道或socket连接中写数据将引发SIGPIPE信号
    addSig(SIGPIPE, SIG_IGN);
}

//父进程的m_idx值为-1，子进程中m_idx值大于等于0，我们据此判断接下来要运行的是父进程代码还是子进程代码
template<typename T>
void ProcessPool<T>::run() {
    if(m_idx == -1){
        run_parent();
    }else{
        run_child();
    }
}

template<typename T>
void ProcessPool<T>::run_child() {
    setup_sig_pipe();

    //每个子进程都通过其在进程池中的序号值m_idx找到与父进程通信的管道
    //所有进程都保留了一份进程池的副本
    int pipe_fd = m_sub_process[m_idx].m_pipe_fd[0];
    //子进程需要需要建管道文件描述符,因为父进程将通过它来通知子进程accept新连接
    addFd(m_epoll_fd, pipe_fd);

    epoll_event events[MAX_EVENT_NUMBER];
    T *users = new T[USER_PER_PROCESS];
    assert(users);
    int number = 0;
    int ret = -1;

    while(!m_stop){
        number = epoll_wait(m_epoll_fd, events, MAX_EVENT_NUMBER, -1);
        if((number < 0) && (errno != EINTR)){
            printf("epoll failure\n");
            break;
        }

        for(int i = 0; i < number; ++i){
            int sock_fd = events[i].data.fd;
            if((sock_fd == pipe_fd) && (events[i].events & EPOLLIN)){
                int client = 0;
                //从父子进程之间的管道读取数据，并将结果保存到变量client中。
                //如果读取成功，则表示新客户连接到来
                ret = recv(sock_fd, (char *)&client, sizeof(client), 0);
                //如果是读取出错且没有提示再尝试一次，或者读取到0字节，那就直接下一次循环
                if(((ret < 0) && (errno != EAGAIN)) || ret == 0){
                    continue;
                }else{
                    struct sockaddr_in client_address;
                    socklen_t client_addr_len = sizeof (client_address);
                    int conn_fd = accept(m_listen_fd, (struct sockaddr *)&client_address, &client_addr_len);
                    if(conn_fd < 0){
                        printf("errno is : %d\n", errno);
                        continue;
                    }

                    addFd(m_epoll_fd, conn_fd);
                    //模板类T必须实现init方法，以初始化一个客户端连接。我们直接使用connfd来
                    //索引逻辑处理对象(T类型的对象)，以提高程序效率
                    //client_address传递给任务类，使其可以给客户端发信息
                    users[conn_fd].init(m_epoll_fd, conn_fd, client_address);
                }
            }
            //下面处理子进程接收到的信号
            else if((sock_fd == sig_pipe_fd[0]) && (events[i].events & EPOLLIN)){
                int sig;
                char signals[1024];
                ret = recv(sock_fd, signals, sizeof (signals), 0);
                if(ret <= 0) {
                    continue;
                }else{
                    for(int j = 0; j < ret; j++){
                        switch (signals[j]) {
                            case SIGCHLD:
                            {
                                pid_t pid;
                                int stat;
                                //第一个参数为-1，表示等待所有子进程，如果收到SIGCHLD且pid大于0说明子进程正常退出。
                                //用while循环可以保证同时有一个以上的子进程发出SIGCHLD信号
                                while((pid = waitpid(-1, &stat, WNOHANG)) > 0){
                                    continue;
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT:
                            {
                                m_stop = true;
                                break;
                            }
                            default:
                            {
                                break;
                            }
                        }
                    }
                }
            }
            //如果是其他刻度数据，那么必然是客户请求到来。调用逻辑处理对象的process方法进行处理
            else if(events[i].events & EPOLLIN){
                users[sock_fd].process();
            }else{
                continue;
            }
        }
    }
    delete[] users;
    users = NULL;
    close(pipe_fd);
    close(m_epoll_fd);
}

template<typename T>
void ProcessPool<T>::run_parent() {
    setup_sig_pipe();
    addFd(m_epoll_fd, m_listen_fd);
    epoll_event events[MAX_EVENT_NUMBER];
    //用于round robin算法
    int sub_process_counter = 0;
    int new_conn = 1;
    int number = 0;
    int ret = -1;

    while(!m_stop){
        number = epoll_wait(m_epoll_fd, events, MAX_EVENT_NUMBER, -1);
        if((number < 0) && (errno != EINTR)){
            printf("epoll failure\n");
            break;
        }

        for(int i = 0; i < number; ++i){
            int sock_fd = events[i].data.fd;
            if(sock_fd == m_listen_fd){
                //如果有新的连接到来，就采用Round Robin方式将其分配给一个子进程
                int j = sub_process_counter;
                do{
                    if(m_sub_process[j].m_pid != -1){
                        break;
                    }
                    j = (j + 1) % m_process_number;
                }while(j != sub_process_counter);

                if(m_sub_process[j].m_pid == -1){
                    m_stop = true;
                    break;
                }
                sub_process_counter = (j + 1) % m_process_number;
                send(m_sub_process[j].m_pipe_fd[1], (char *)&new_conn, sizeof (new_conn), 0);
                printf("send request to child %d\n", j);
            }
            //下面处理父进程接收到信号
            else if((sock_fd == sig_pipe_fd[0]) && (events[i].events & EPOLLIN)){
                int sig;
                char signals[1024];
                ret = recv(sig_pipe_fd[0], signals, sizeof (signals), 0);
                if(ret <= 0){
                    continue;
                }else{
                    for(int j = 0; j < ret; ++j){
                        switch (signals[j]) {
                            case SIGCHLD:
                            {
                                pid_t pid;
                                int stat;
                                while((pid = waitpid(-1, &stat, WNOHANG)) > 0){
                                    for(int k = 0; k < m_process_number; ++k){
                                        //如果进程池中第k个子进程推出了，则主进程关闭相应的通信管道，并设置
                                        //相应的m_pid为-1,以标记该子进程已经退出
                                        if(m_sub_process[k].m_pid == pid){
                                            printf("child %d join\n", k);
                                            close(m_sub_process[k].m_pipe_fd[1]);
                                            m_sub_process[k].m_pid = -1;
                                        }
                                    }
                                }
                                //如果所有子进程都已经退出了，则父进程也退出了
                                m_stop = true;
                                for(int k = 0; k < m_process_number; ++k){
                                    if(m_sub_process[k].m_pid != -1){
                                        m_stop = false;
                                    }
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT:
                            {
                                //如果父进程接收到终止信号，那么就杀死所有子进程。并等待它们全部结束。
                                //通知子进程结束更好的方法是向子进程之间的通信管道发送特殊数据。
                                printf("kill all the child now\n");
                                for(int k = 0; k < m_process_number; ++k){
                                    int pid = m_sub_process[k].m_pid;
                                    if(pid != -1){
                                        kill(pid, SIGTERM);
                                    }
                                }
                                break;
                            }
                            default:
                            {
                                break;
                            }
                        }

                    }
                }
            }
            else{
                continue;
            }
        }
    }
    close(m_epoll_fd);
}

#endif //WEBSERVER_PROCESSPOOL_H
