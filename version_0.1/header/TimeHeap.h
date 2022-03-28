//
// Created by 36967 on 2022/3/11.
// 最小堆实现定时器
/* 主线程和工作线程都需要使用定时器堆，
 * 主线程在第一次addFd时添加定时器，
 * 工作线程在process_read()判定为NO_REQUEST时modFd之前添加定时器*/
//

#ifndef WEBSERVER_TIMEHEAP_H
#define WEBSERVER_TIMEHEAP_H

#include <iostream>
#include <netinet/in.h>
#include <time.h>
using std::exception;

#define BUFFER_SIZE 64

class HttpConnection;
class Timer; //前向声明
//绑定socket和定时器
//struct client_data{
//    sockaddr_in address;
//    int sock_fd;
//    char buf[BUFFER_SIZE];
//    Timer* timer;
//};

//定时器类
class Timer{
public:
    Timer(int delay, HttpConnection* httpConnection) : conn(httpConnection) {
        //time(NULL)返回的是1970到当前的秒数
        expire = time(nullptr) + delay;
    }

public:
//    void (*cb_func)();//定时器回调函数
    time_t expire; //定时器生效的绝对时间
    HttpConnection* conn;
    bool isvalid(){
        time_t cur = time(nullptr);
        //判断定时器是否有效
        //当前时间小于过期时间则有效
        return cur < this->expire;
    }
};

class TimeHeap{
public:
    //构造函数1，初始化为一个大小为cap的空堆
    TimeHeap(int capacity) throw(std::exception) : capacity(capacity), cur_size(0){
        array = new Timer* [capacity]; //创建堆数组
        if(!array){
            throw std::exception();
        }
        for(int i = 0; i < capacity; ++i){
            array[i] = nullptr;
        }
    }
    //构造函数2，用已有数组来初始化堆
    TimeHeap(Timer** init_array, int size, int capacity)
        throw(std::exception) : capacity(capacity), cur_size(size)
    {
        if(capacity < size)
        {
            throw std::exception();
        }
        array = new Timer* [capacity]; //创建堆数组
        if(!array){
            throw std::exception();
        }
        for(int i = 0; i < capacity; ++i){
            array[i] = nullptr;
        }
        if(size != 0){
            //初始化堆数组
            for(int i = 0; i < size; ++i){
                array[i] = init_array[i];
            }
            //(index-1)/2表示的是index位置的节点的父节点
            //(cur_size-1)/2表示最后一个节点的下一个空位的父节点，即倒数第一个非叶子节点
            //初始化时要对最后一个非叶子节点执行下滤操作，这一步就是堆排序
            for(int i = (cur_size-1) / 2; i >= 0; --i){
                //对数组中的第[(cur_size-1)/2]到0个元素执行下滤操作；
                percolate_down(i);
            }
        }
    }

    //销毁时间堆
    ~TimeHeap(){
        for(int i = 0; i < cur_size; ++i){
            delete array[i];
        }
        //因为存储的是类对象的指针，所以上一条先删除对象
        //这一条再删除array中的全部指针
        delete [] array;
    }

public:
    //添加目标定时器timer
    void add_timer(Timer* timer) throw (std::exception){
        if(!timer){
            return;
        }
        if(cur_size >= capacity){
            //如果当前堆数组容量不够，则将其扩大1倍
            resize();
        }
        //新插入了一个元素，当前堆大小加1，hole是新建空穴的位置
        int hole = cur_size++;
        int parent = 0;
        //插入时
        //对从空穴到根节点的路径上所有节点执行上滤操作
        for(; hole > 0; hole = parent){
            parent = (hole-1) / 2;
            if(array[parent]->expire <= timer->expire){
                break;
            }
            array[hole] = array[parent];
        }
        array[hole] = timer;
    }

    //删除目标定时timer
    void del_timer(Timer* timer){
        if(!timer){
            return;
        }
        //仅仅将目标定时器的回调函数设置为空，即所谓的延迟销毁。这将节省真正删除该定时器造成的开销，
        //但是这样做容易使堆数组膨胀
        //timer->cb_func = nullptr;
        timer->conn = nullptr;
    }
    //获得堆顶部的定时器
    Timer* top() const
    {
        if(empty()){
            return nullptr;
        }
        return array[0];
    }
    //删除堆顶部的计时器
    void pop_timer(){
        if(empty()){
            return;
        }
        if(array[0]){
            delete array[0];
            //将原来的堆顶元素替换为堆数组中最后一个元素
            array[0] = array[--cur_size];
            //对新的对丁元素执行下滤操作
            percolate_down(0);
        }
    }

/*
    为解决HttpConnection与TimerHeap之间的循环引用，
    array[0]->conn->close_conn();无法调用
    只能弹出堆顶元素后到上层主线程中去判断是否超过期限
*/

    //心跳函数
//    void tick(){
//        Timer* tmp = array[0];
//        //循环处理堆中到期的定时器
//        time_t cur = time(nullptr);
//        while(!empty()){
//            if(!tmp){
//                break;
//            }
//            //如果堆定时器没到期，则退出循环
//            if(tmp->expire > cur){
//                break;
//            }
//            //否则执行堆顶定时器中的任务
//            if(array[0]->cb_func){
//                array[0]->cb_func();
//            }
//            if(array[0]->conn){
//                array[0]->conn->close_conn();
//            }
//            //将堆顶元素删除，同时生成新的堆定时器(array[0])
//            pop_timer();
//            tmp = array[0];
//        }
//    }

    bool empty() const {return cur_size == 0;}

private:
    //最小堆的下滤操作，确保堆数组中以第hole个节点作为根的子树拥有最小堆性质
    void percolate_down(int hole){
        Timer* temp = array[hole];
        int child = 0;
        //hole*2+1为左子节点
        for(; ((hole*2+1) <= (cur_size-1)); hole=child){
            child = hole*2+1;
            //child+1为右子节点
            //这一步的目的是是选出左右中小的那一个
            if((child < (cur_size-1)) && (array[child+1]->expire < array[child]->expire)){
                ++child;
            }
            //将小的子节点放到父亲节点上去，子节点变成了空的
            if(array[child]->expire < temp->expire){
                array[hole] = array[child];
            }else{
                //如果已经形成小顶堆那么就可以结束了留出一个节点来给hole插入
                break;
            }
        }
        //将节点插入空穴
        array[hole] = temp;
    }

    //将堆数组容量扩大一倍
    void resize() throw (std::exception){
        Timer** temp = new Timer* [2 * capacity];
        for(int i = 0; i < 2*capacity; ++i){
            temp[i] = nullptr;
        }
        if(!temp){
            throw std::exception();
        }
        capacity = 2 * capacity;
        for(int i = 0; i < cur_size; ++i){
            temp[i] = array[i];
        }
        delete [] array;
        array = temp;
    }

private:
    Timer** array; //堆数组
    int capacity; //堆数组的容量
    int cur_size; //堆数组当前包含的元素的个数
};
#endif //WEBSERVER_TIMEHEAP_H
