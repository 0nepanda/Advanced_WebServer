#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <cstdio>
#include <list>
#include <exception>
#include <pthread.h>

#include "../lock/locker.h"
#include "../sql_conn_pool/sql_connection_pool.h"


/* 线程池： 
    空间换时间，浪费服务器的硬件资源，换取运行效率。
    池是一组资源的集合，这组资源在服务器启动之初就被完全创建并完成初始化。 这成为静态资源。
    当服务器进入正式运行阶段，开始处理客户请求的时候，如果他需要相关的资源，可以直接从池中获取，无需动态分配。 
*/
template <typename T>
class threadpool {
public:
    threadpool(int actor_model, connection_pool* connPool, int thread_number=8, int max_requsets = 10000);
    ~threadpool();
    bool append(T* requset, int state);
    bool append_p(T* requset);

private:
    /* 工作线程入口*/ 
    static void* worker(void* arg);  /* 工作线程运行的函数，不断从工作队列取出任务并执行 */  /* Proactor or Reactor */
    void run();
    /* 静态成员变量：
        将类的成员变量声明为static，则为静态成员变量。
        与一般的成员变量不同，无论建立多少对象，都只有一个静态成员变量的拷贝(同一个)，静态成员属于一个类，所有对象共享。
        静态变量在编译阶段就分配了空间，对象还没创建时就已经分配了空间，放到全局静态区。
            静态成员变量：
                最好是类内声明，类外初始化。(以免类名方位静态成员访问不到)（如lst_timer.cpp中Utils::u_epollfd = 0）;
                无论共有还是私有，静态成员都可以在类外定义，但私有成员仍有访问权限。
                非静态成员类外不能初始化.
                静态成员数据是共享的
       静态成员函数：
        静态成员函数可以直接访问静态成员变量，不能直接访问普通成员变量。但可以通过参数传递的方式访问。
        普通成员函数可以访问普通成员变量，也可以访问静态成员变量。
        静态成员函数没有this指针。 非静态数据成员被对象单独维护，但静态成员函数为共享函数，无法区分属于哪个对象。
        因此不能直接访问普通静态变量成员，也没有this指针。  
    */

private:
    int m_thread_number;            /* 线程池的线程数 */
    int m_max_requsets;             /* 请求队列中允许的最大请求数 */
    pthread_t* m_threads;           /* 描述线程池的数组，其大小为 m_thread_number */
    std::list<T*> m_workqueue;      /* 请求队列 */    /* 是线程间共享的, 操作它要上锁*/
    locker m_queuelocker;           /* 用来保护请求队列的互斥锁 */
    sem m_queuestate;               /* 是否有任务需要处理 */
    connection_pool* m_connPool;    /* 数据库  地址？ */
    int m_actor_model;              /* 模型切换 */
};

/* 构造函数： 初值化列表 */
template <typename T>
threadpool<T>::threadpool(int actor_model, connection_pool* connPool, int thread_number, int max_requests)
                         : m_actor_model(actor_model), m_thread_number(thread_number), m_max_requsets(max_requests) {
    if (thread_number <= 0|| max_requests <= 0)
    {
        throw std::exception();
    }

    m_threads = new pthread_t[thread_number];
    if (!m_threads) {
        throw std::exception();
    }
    
    /* 创建 thread_number 个线程， 并将他们都设置为脱离线程 */
    for (int i = 0; i < thread_number; i++) {
        if (pthread_create(m_threads + i, nullptr, worker, this) != 0) {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i])) {  /* 只有在pthread_join函数返回时，或者是设置在分离属性的情况下，一个线程结束会立即释放它所占用的资源。 */
            delete[] m_threads;
            throw std::exception();
        }
    }
    /* 补充知识： pthread_create()  &&  pthread_detach()
        一、pthread_create(新创建的线程ID指向的内存单元, 线程属性默认为NULL, 新创建的线程函数入口地址， 重新申请一块内存存入需要传递的参数再将这个地址作为arg传入):
            避免直接在传递的参数中传递发生改变的量：即使是只再创造一个单线程，也可能在线程未获取传递参数时，线程获取的变量值已经被主线程进行了修改。
        二、pthread_detatch().
            使用时注意防止内存泄漏：在默认情况下通过pthread_create函数创建的线程是非分离属性的
            即：由pthread_create函数的第二个参数决定，在非分离的情况下，当一个线程结束的时候，它所占用的系统资源并没有完全真正的释放，也没有真正终止。
        三、this指针  worker()
            pthread_create()函数的第三个参数，为函数指针，指向的 线程处理函数的地址 。
            该函数，要求为静态函数。如果 处理线程函数 为类成员函数时，需要将其设置为静态成员函数。
            参三指向的线程处理函数参数类型为(void*)，若线程函数为类成员函数，则this指针会作为默认的参数被传进函数。
            从而和线程函数参数(void*)不能匹配，不能通过编译。
            静态成员函数就没有这个问题，其没有this指针。
    */
}

template <typename T>
threadpool<T>::~threadpool() {
    delete m_threads;
}

template <typename T>
bool threadpool<T>::append(T* requset, int state) {
    /* 操作工作队列是一定要加锁，因为它被所有线程共享 */
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requsets) {  /* 请求队列已满，不许插入 */
        m_queuelocker.unlock();
        return false;
    }
    requset->m_state = state;  /* 判断读写位 */
    m_workqueue.push_back(requset);
    m_queuelocker.unlock();
    m_queuestate.post();  /* V 操作：标识(请求队列上)有无任务需要处理 */
    return true;
}

template <typename T>
bool threadpool<T>::append_p(T* requset) {
    /* 操作请求队列时需要加锁，因为其被所有线程共享 */
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requsets) {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(requset);
    m_queuelocker.unlock();
    m_queuestate.post();
    return true;
}

template <typename T>
void* threadpool<T>::worker(void* arg) {  /* 在pthread_create时完成创建线程以后，就开始运行相关的线程函数*/
    threadpool* pool = (threadpool*) arg;
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run() {
    while (true) {
        /* 从请求队列中取出一个 http 连接任务 */
        m_queuestate.wait();
        m_queuelocker.lock();
        if (m_workqueue.empty()){
            m_queuelocker.unlock();
            continue;  /* 回到while继续判断，即等待生产者生产 */
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request){  /* 感觉是逻辑上写重了 */
            continue;
        }

        /* reactor 模型，读写在工作线程中执行 */
        if (m_actor_model == 1) {
            /* 读 */
            if (request->m_state == 0) {
                if (request->read()) {    /* 读缓存 ？*/
                    request->improv = 1;  /* ????????????? */
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                }
                else {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            /* 写 */
            else {
                if (request->write()) {
                    request->improv = 1;
                }
                else {
                    request->improv = 1;
                }
            }
            /* Proactor */
        }
        else {
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process();
        }
    }
}


#endif