#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

/* 信号量 */
class sem {  
public:
    sem();
    sem(int value);
    ~sem();
    bool wait();  /* P操作 */
    bool post();  /* V操作 */
private:
    sem_t m_sem;  /* 添加头文件："#include <semaphore.h>" */
};

/* 互斥锁 */
class locker {
public:
    locker();
    ~locker();
    bool lock();
    bool unlock();
    pthread_mutex_t* get();  /* 获取互斥锁 */
private:
    pthread_mutex_t m_mutex;  /* 添加头文件："#include <pthread.h>" */
};

/* 条件变量 */
class cond {
public:
    cond();
    ~cond();
    bool wait(pthread_mutex_t* m_mutex);  /* 保证操作原子性-->>加锁 */
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t);
    bool signal();
    bool broadcast();
private:
    pthread_cond_t m_cond;
};

#endif