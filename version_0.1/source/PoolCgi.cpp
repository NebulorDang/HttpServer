//
// Created by NebulorDang on 2021/12/17.
// 进程池实现Cgi服务器
//

#include "ProcessPool.h"
class CgiConn{
public:
    CgiConn(){}
    ~CgiConn(){}

    //初始化客户连接，清空读缓冲区
    void init(int epoll_fd, int sock_fd, const sockaddr_in &client_addr){
        m_epoll_fd = epoll_fd;
        m_sock_fd = sock_fd;
        m_address = client_addr;
        memset(m_buf, 0, BUFFER_SIZE);
        m_read_index = 0;
    }

    void process(){
        int idx = 0;
        int ret = -1;
        //循环读取和分析客户数据
        while(true){
            idx = m_read_index;
            ret = recv(m_sock_fd, m_buf + idx, BUFFER_SIZE - 1 - idx, 0);
            //如果读操作发生错误，则关闭客户连接，但是如果暂时无数据可读，则退出循环
            if(ret < 0){
                if(errno != EAGAIN){
                    delFd(m_epoll_fd, m_sock_fd);
                }
                break;
            }
            //如果对方关闭连接，则服务器也关闭连接
            else if(ret == 0){
                delFd(m_epoll_fd, m_sock_fd);
                break;
            }else{
                m_read_index += ret;
                printf("user content is %s\n", m_buf);
                //如果遇到字符"\r\n",则开始处理客户请求
                for(; idx < m_read_index; idx++){
                    if((idx >= 1) && (m_buf[idx-1] == '\r') && (m_buf[idx] == '\n')){
                        break;
                    }
                }
                //如果没有遇到字符"\r\n"，则需要读取更多客户数据
                //判定条件idx等于读缓冲中的最后一个字符的下一个位置
                if(idx == m_read_index){
                    continue;
                }

                //如果读取到"\r\n"
                m_buf[idx-1] = '\0';

                char *file_name = m_buf;
                //判断客户要运行的cgi程序是否存在
                if(access(file_name, F_OK) == -1){
                    delFd(m_epoll_fd, m_sock_fd);
                    break;
                }
                //创建子进程来执行CGI程序
                ret = fork();
                if(ret == -1){
                    delFd(m_epoll_fd, m_sock_fd);
                    break;
                }
                else if(ret > 0){
                    //父进程只需要关闭连接
                    delFd(m_epoll_fd, m_sock_fd);
                    break;
                }else{
                    //子进程将标准输出重定向到m_sock_fd，并执行CGI程序
                    close(STDOUT_FILENO);
                    dup(m_sock_fd);
                    execl(m_buf, m_buf, NULL);
                    exit(0);
                }
            }
        }
    }

private:
    static const int BUFFER_SIZE = 1024;
    static int m_epoll_fd;
    int m_sock_fd;
    sockaddr_in m_address;
    char m_buf[BUFFER_SIZE];
    //标记读缓冲区已经读入客户数据的最后一个字节的下一个位置
    int m_read_index;
};

int CgiConn::m_epoll_fd = -1;

int main_PoolCgi(int argc, char *argv[]){
    if(argc < 2){
        //去掉路径输出文件名
        printf("usage: %s ip_address port_number\n", basename(argv[0]));
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listen_fd > 0);

    int ret = 0;
    struct sockaddr_in address;
    memset(&address, 0, sizeof (address));
    address.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(port);

    ret = bind(listen_fd, (struct sockaddr *)&address, sizeof(address));
    assert(ret != -1);

    ret = listen(listen_fd, 5);
    assert(ret != -1);

    ProcessPool<CgiConn> *pool = ProcessPool<CgiConn>::create(listen_fd);
    if(pool){
        pool->run();
        delete pool;
    }

    close(listen_fd);
    return 0;
}