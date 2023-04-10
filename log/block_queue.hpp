/***************************************************************/
/* 循环数组实现的阻塞队列，m_back = (m_back + 1) % m_max_size; */
/* 线程安全，每个操作前都要先加互斥锁，操作完后，再解锁*/
/***************************************************************/

#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <iostream>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "../lock/locker.h"
using namespace std;

template <typename T>
class block_queue {
public:
    block_queue(int max_size);
    ~block_queue();
    void clear();  /* 清空阻塞队列 */
    bool full();  /* 判断阻塞队列是否满了 */
    bool empty();  /* 判断阻塞队列是否为空 */
    bool front(T& value);  /* 返回队首元素 */
    bool back(T& value);  /* 返回队尾元素 */
    int size();  /* 返回队列的当前长度 */
    int max_size();  /* 返回队列的最大长度 */
    bool push(const T& item);  /* 向队列的尾部添加元素 */
    bool pop(T& item);  /* 弹出队首元素 */
    bool pop(T& item, int ms_timeout);  /* 重载一个处理超时的版本 */
private:
    locker m_mutex;
    cond m_cond;

    T* m_array;
    int m_size;
    int m_max_size;
    int m_front;
    int m_back;   /* 指向队列中的位置 */
};

// 构造函数：创建循环数组
template <typename T>
block_queue<T>::block_queue(int max_size) {
    if (max_size <= 0) {
        exit(-1);
    }
    m_max_size = max_size;
    m_array = new T[m_max_size];
    m_size = 0;
    m_front = -1;
    m_back = -1;
}

// 析构函数：销毁循环数组
template <typename T>
block_queue<T>::~block_queue() {
    m_mutex.lock();
    if (m_array != nullptr) {
        delete[] m_array;
        m_array = nullptr;
    }
    m_mutex.unlock();
}

template <typename T>
void block_queue<T>::clear() {
    m_mutex.lock();
    m_size = 0;
    m_front = -1;
    m_back = -1;
    m_mutex.unlock();
}

template <typename T>
bool block_queue<T>::full() {
    m_mutex.lock();
    if (m_size >= m_max_size) {
        m_mutex.unlock();
        return true;
    }
}

template <typename T>
bool block_queue<T>::empty() {
    m_mutex.lock();
    if(m_size == 0) {
        m_mutex.unlock();
        return true;
    }
    m_mutex.unlock();
    return false;
}

template <typename T>
bool block_queue<T>::front(T& value) {  /*把阻塞队列的队头元素以地址传递的方式赋给value*/
    m_mutex.lock();
    if(m_size == 0) {
        m_mutex.unlock();
        return false;
    }
    value = m_array[m_front];
    m_mutex.unlock();
    return true;
}

template <typename T>
bool block_queue<T>::back(T& value) {
    m_mutex.lock();
    if (m_size == 0) {
        m_mutex.unlock();
        return false;
    }
    value = m_array[m_back];
    m_mutex.unlock();
    return true;
}

template <typename T>
int block_queue<T>::size() {
    int temp = 0; 

    m_mutex.lock();
    temp = m_size;
    m_mutex.unlock();
    return temp;
}

template <typename T>
int block_queue<T>::max_size() {
    int temp = 0;

    m_mutex.lock();
    temp = m_max_size;
    m_mutex.unlock();

    return temp;
}


/* 往队列添加元素，需要先唤醒所有的线程 */
/* 当有元素被push到队列， 相当于生产者生茶一个元素*/
/* 若当前没有线程等待条件变量，则唤醒线程无意义 */
template <typename T>
bool block_queue<T>::push(const T& item) {  /* 插到队尾 */
    m_mutex.lock();
    if(m_size >= m_max_size) {
        m_cond.broadcast();
        m_mutex.unlock();
        return false;
    }
    /* 循环数组实现模拟生产 */
    m_back = (m_back + 1) % m_max_size;
    m_array[m_back] = item;
    m_size++;
    m_cond.broadcast();
    m_mutex.unlock();
    return true;
}

template <typename T>
bool block_queue<T>::pop(T& item) {
    m_mutex.lock();
    /* 多个消费者时，这里只能使用while而不是if: 有可能线程A的wait返回后，资源已经被其他线程B抢占了 */
    /* 所以要“一直”可用 */
    while (m_size <= 0) {
        /* 确保没有“资源(生产的物品)”时，所有线程一直阻塞 */
        if(!m_cond.wait(m_mutex.get())){  /* wait(&m_mutex)可行吗？ */
            m_mutex.unlock();  /* 不阻塞或阻塞失败，则返回false */
            return false;
        }  
    }
    m_front = (m_front + 1) % m_max_size;
    item = m_array[m_front];
    m_size--;
    m_mutex.unlock();
    return true;
}

/* 重载pop的超时版本 */ /* 本项目没有用到 */  /*在pthread_wait的基础上加了等待时间，只在指定时间内能抢到互斥锁即可 */
template <typename T>
bool block_queue<T>::pop(T& item, int ms_timeout) {
    struct timespec t = {0, 0};
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    m_mutex.lock();
    if (m_size <= 0) {   /* 在到达t时刻后，条件变量没有信号 */
        t.tv_sec = now.tv_sec + ms_timeout / 1000;
        t.tv_nsec = (ms_timeout % 1000) * 1000;
        if(!m_cond.timewait(m_mutex.get(), t)) {  /* 这个timewait是为了阻塞当前进程 */ /*其核心函数pthread_cond_timedwait允许线程等待一个条件变量被信号通知或者超时发生*/
            m_mutex.unlock();  /* cond下编写的timewait是0==0返回1*/ 
            return false;  /*也就是说在超时时间内，没有成功等到条件变量的信号。而是接收到超时的错误码TIMEDOUT */
        }
    }

    if (m_size <= 0) {  /* 过了t时间后, 依然没有生产者生产 */
        m_mutex.unlock();
        return false;
    }

    m_front = (m_front + 1) % m_max_size;
    item = m_array[m_front];
    m_size--;
    m_mutex.unlock();
    return true;
}

#endif