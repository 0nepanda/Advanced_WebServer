#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "block_queue.hpp"  /* 阻塞队列 */
using namespace std;

/* 日志，由服务器自动创建，并记录运行状态，错误信息，访问数据的文件。 */

/* 日志类中的方法都不会被其他程序直接调用，下面的四个宏定义提供其他程序的调用方法 */
// 这四个宏定义在其他文件中使用，主要用于不同类型的日志输出
#define LOG_DEBUG(format,...) Log::get_instance()->write_log(0, format, __VA_ARGS__)
#define LOG_INFO(format,...) Log::get_instance()->write_log(1, format, __VA_ARGS__)
#define LOG_WARN(format,...) Log::get_instance()->write_log(2, format, __VA_ARGS__)
#define LOG_ERROR(format,...) Log::get_instance()->write_log(3, format, ##__VA_ARGS__)

/* 宏定义的实现：
加入##后，如可变参数的个数为0，则“##”会将前面多余的“...”去掉，否则会编译错误。使得程序更加健壮
*/

class Log {
public:
    /* 获取单一实例的接口 */
    /* 经典的线程安全懒汉模式，使用双检测法加锁*/
    /* C++11之后，使用局部变量懒汉模式不用加锁 */
    static Log* get_instance() {
        static Log instance;
        return & instance;
    }

    /* 异步写日志的公有方法， 进而在类的内部 调用异步写日志的私有方法*/
    static void* flush_log_thread(void* args) {
        Log::get_instance()->async_write_log();
        return nullptr;
    }

    /* 可选择的参数： 日志大小、日志缓冲区大小、最大行数、最长日志条例 */
    bool init(const char* file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

    void write_log(int level, const char* format, ...);  /* level:日志分级 */

    void flush(void);

private:
    /* 私有化构造函数， 不被其他程序调用 */ 
    Log() {
        m_count = 0;
        m_is_async = false;
    }
    virtual ~Log() {
        if (m_fp != nullptr) {
            fclose(m_fp);
        }
    }

    /* 从阻塞队列中异步写入日志文件 */
    void* async_write_log() {
        string single_log;
        /* 从阻塞队列中取出一个string类日志，写入文件 */
        while (m_log_queue->pop(single_log))
        {
            m_mutex.lock();
            fputs(single_log.c_str(), m_fp);   /*将c++的string转为c的字符串数组*/
            m_mutex.unlock();
        }
        return nullptr;
    }

private:
    char dir_name[128];  /* 路径名 */ 
    char log_name[128];  /* log文件名 */
    int m_split_lines;  /* 日志最大行数 */
    int m_log_buf_size;  /* 日志缓冲区大小 */
    long long m_count;  /* 日志行数纪录 */
    int m_today;  /*将日志按天分类，记录当前是哪一天*/
    FILE* m_fp;  /* 打开 log 的文件指针*/
    char* m_buf;
    block_queue<string>* m_log_queue;  /* 阻塞队列 */
    bool m_is_async;  /* 是否为同步标志位 */
    locker m_mutex;
    int m_close_log;
};

#endif