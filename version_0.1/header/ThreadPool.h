//
// Created by NebulorDang on 2021/12/18.
// 线程池
//

#ifndef WEBSERVER_THREADPOOL_H
#define WEBSERVER_THREADPOOL_H
#include <list>
#include <stdio.h>
#include "Locker.h"
#include "TimeHeap.h"

template<typename T>
//线程池，模板参数T是任务类
class ThreadPool{
public:
//参数thread_number是线程池中线程的数量，max_requests是请求队列中最多
//允许的、等待处理的请求的数量
ThreadPool(int thread_number = 4, int max_requests = 100000);
~ThreadPool();
//向请求队列中添加任务
bool append(T *request);

private:
    //工作线程运行的函数，它不断从工作队列中去除任务并执行
    static void *worker(void *arg);
    void run();

private:
    //线程池中的线程数量
    int m_thread_number;
    //请求队列中允许的最大请求数量
    int m_max_requests;
    //线程池(线程指针数组)
    pthread_t *m_threads;
    //请求队列
    std::list<T *> m_work_queue;
    //保护请求队列的互斥锁
    Locker m_queue_locker;
    //是否有任务需要处理
    Sem m_queue_stat;
    //是否结束线程
    bool m_stop;

private:

};

template<typename T>
ThreadPool<T>::ThreadPool(int thread_number, int max_requests): m_thread_number(thread_number),
m_max_requests(max_requests), m_stop(false), m_threads(NULL){
    if(m_thread_number <= 0 || m_max_requests <= 0){
        throw std::exception();
    }

    //这里只是new了数组,没有调用pthread_t的构造函数
    m_threads = new pthread_t[m_thread_number];

    //创建thread_number个线程，并将它们都设置为脱离线程
    for(int i = 0; i < m_thread_number; i++){
        printf("create the %d-th thead\n", i);
        if(pthread_create(m_threads + i, NULL, worker, this) != 0){
            delete[] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i])){
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
ThreadPool<T>::~ThreadPool<T>() {
    delete[] m_threads;
    m_stop = true;
}

template<typename T>
bool ThreadPool<T>::append(T *request) {
    //操作工作队列一定要加锁，因为被所有线程共享
    m_queue_locker.lock();
    if(m_work_queue.size() > m_max_requests){
        m_queue_locker.unlock();
        return false;
    }

    m_work_queue.push_back(request);
    m_queue_locker.unlock();
//    m_queue_stat.post();
    return true;
}

template <typename T>
void* ThreadPool<T>::worker(void *arg){
    ThreadPool<T> *pool = (ThreadPool<T> *)arg;
    pool->run();
    //线程函数一般可以返回void
    //这里返回线程池对象，方便进行可能以后要加的操作
    return pool;
}

template <typename T>
void ThreadPool<T>::run(){
    while(!m_stop){
        //等待信号量不用加锁
//        m_queue_stat.wait();
        m_queue_locker.lock();
        if(m_work_queue.empty()){
            m_queue_locker.unlock();
            continue;
        }
        T* request = m_work_queue.front();
        m_work_queue.pop_front();
        m_queue_locker.unlock();
        if(!request){
            continue;
        }
        //调用任务类的处理函数
        request->process();
    }
}

#endif //WEBSERVER_THREADPOOL_H
