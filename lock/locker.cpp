#include "locker.h"

/*************信号量的成员函数类外实现**************/
// 构造函数。信号量的创建
sem::sem() {
    if(sem_init(&m_sem, 0, 0) != 0) {
        throw std::exception();   /* 添加头文件："#include <exception>" */
    }
}
sem::sem(int num) {
    if(sem_init(&m_sem, 0, num) != 0) {
        throw std::exception();
    }
}

// 析构函数：信号量的销毁
sem::~sem() {
    sem_destroy(&m_sem);
}

// 信号量的 P V 操作
bool sem::wait() {
    return sem_wait(&m_sem);
}

bool sem::post() {
    return sem_post(&m_sem);
}


/************* 互斥量的成员函数类外实现 **************/
// 构造函数：互斥量的创建
locker::locker() {
    if(pthread_mutex_init(&m_mutex, NULL) != 0) {   //成功返回0
        throw std::exception();
    }
}

// 析构函数：互斥量的销毁
locker::~locker() {
    if(pthread_mutex_destroy(&m_mutex) != 0) {
        throw std::exception();
    }
}

// 上锁
bool locker::lock() {
    return pthread_mutex_lock(&m_mutex);
}

// 解锁
bool locker::unlock() {
    return pthread_mutex_unlock(&m_mutex);
}

// 获取互斥锁
pthread_mutex_t* locker::get() {
    // return &this->m_mutex;  
    return &m_mutex;
}


/************* 条件变量的成员函数类外实现 **************/
// 构造函数：条件变量的创建
cond::cond() {
    if (pthread_cond_init(&m_cond, NULL) != 0) {
        throw std::exception();
    }
}

// 析构函数：条件变量的销毁
cond::~cond() {
    if (pthread_cond_destroy(&m_cond) != 0) {
        throw std::exception();
    }
}

// 阻塞当前线程
bool cond::wait(pthread_mutex_t* m_mutex) {
    int ret = 0;
    pthread_mutex_lock(m_mutex);
    ret = pthread_cond_wait(&m_cond, m_mutex);
    pthread_mutex_unlock(m_mutex);
    return ret == 0;
}
bool cond::timewait(pthread_mutex_t *m_mutex, struct timespec t) {
    int ret = 0;
    pthread_mutex_lock(m_mutex);
    ret = pthread_cond_timedwait(&m_cond, m_mutex, &t); /*允许线程等待一个条件变量被信号通知或者超时发生*/
    pthread_mutex_unlock(m_mutex);
    return ret == 0;
}
/* pthread_cond_timedwait:
这个函数会原子性地解锁由 mutex 指向的互斥量，并等待由 cond 指向的条件变量。
在函数返回之前，互斥量会被重新锁定。
如果条件变量被信号通知，调用线程会自动被唤醒并解锁互斥量。  如果函数成功地等待到了条件变量的信号，则返回值为0。
如果在条件被信号通知之前超时发生，则函数返回一个错误代码表示超时。  如果在指定的超时时间内没有接收到条件变量的信号，则返回值为 ETIMEDOUT，表示超时
*/

// 唤醒一个等待目标条件变量的线程，取决于线程的优先级和调度策略
bool cond::signal() {
    return pthread_cond_signal(&m_cond) == 0;
}

// 唤醒所有等待目标条件变量的线程
bool cond::broadcast() {
    return pthread_cond_broadcast(&m_cond) == 0;
}