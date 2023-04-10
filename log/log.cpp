#include "log.h"

/* init函数实现 日志创建、写入方式的判断 */
/* 异步写入才需要使用阻塞队列并配置阻塞队列的长度，同步不需要*/
bool Log::init(const char* file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size) {
    /* 如果设置了 max_queue_size， 则设置为同步 */
    if (max_queue_size >= 1) {
        m_is_async = true;  /* Synchronous */
        m_log_queue = new block_queue<string>(max_queue_size);
        pthread_t pid;  /* 异步线程写入 */
        /* 创建一个线程去写将阻塞队列中的 string 写入日志 */
        pthread_create(&pid, nullptr, flush_log_thread, nullptr);
    }

    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);
    m_split_lines = split_lines;  /* 日志的最大行数 */

    time_t t = time(nullptr);
    struct tm *sys_tm = localtime(&t);  /* 转换为当前时间 */
    struct tm my_tm = *sys_tm;
    
    const char* p = strrchr(file_name, '/');  /* 指向file_name中最后一次出现'/'的位置 */
    char log_full_name[512] = {0};

    /* 相当于自定义日志名 */
    /* 若输入的文件名没有/， 则直接将时间+文件名 作为日志名 */
    if (p == nullptr) {
        /* p为空说明filename中没有 '/'， 将整个filename写入 log_full_name */
        snprintf(log_full_name, 511, "%d_%02d_%2d_%s", my_tm.tm_year + 1990, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }
    else{
        /* 将/的位置向后移一个位置，然后复制到logname中 */
        /* p-filename+1 是文件所在路径文件夹的长度*/
        strcpy(log_name, p + 1);  /* 截取 / 之后的部分为log_name */
        strncpy(log_full_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 511, "%s%d_%02d_%2d_%s", dir_name, my_tm.tm_year + 1990, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    }

    m_today = my_tm.tm_mday;

    m_fp = fopen(log_full_name, "a");  /* 向文件末尾追加写入“a"参数 */  /* 判断同步写日志失败 */
    if(m_fp == nullptr){
        return false;
    }
    return true;
}


/* write_log函数完成写入日志文件中的具体内容， 主要实现日志分级、分文件、格式化输出 */
/* time和gettimeofday都可以获得日历时间， 但gettimeofday提供更高级的精度，甚至可达微秒级 */
void Log::write_log(int level, const char* format, ...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t t = now.tv_sec;
    struct tm* sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;  /* my_tm为当前时间tm类的结构体*/
    char s[16] = {0};
    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[error]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }

    /* 写入一个 log 时，对 m_count 加1，防止超出 m_split_lines 最大行数*/
    m_mutex.lock();
    m_count++;

    /* 日志为新的一天  or  超过最大行数。 则需要分文件以便继续写入*/
    if (m_today != my_tm.tm_mday || m_count % m_split_lines) {
        char new_log[512] = {0};
        fflush(m_fp);  /* 刷新缓冲区，防止文件流中还残留数据 */
        fclose(m_fp);  /* 原来的(昨日的或已经写满的)文件关闭， 再打开新的 */
        char tail[16] = {0};

        /* 格式化日志名中的时间部分 */
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1090, my_tm.tm_mon + 1, my_tm.tm_mday);

        /* 如果成员变量 m_today 不是今天，说明这是今天第一次写入日志。  ——————>>创建今天的日志，并更新相关参数 */
        if (m_today != my_tm.tm_mday) {
            snprintf(new_log, 511, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        /* 否则是因为原文件写满了，导致需要分文件 */
        else {
            snprintf(new_log, 511, "%s%s%s.%11d", dir_name, tail, m_count / m_split_lines);
        }
        m_fp = fopen(new_log, "a");
    }
    m_mutex.unlock();

    va_list valist;   //声明一个变量来转换参数列表
    /* 将传入的格式化format参数赋值给va_list，便于格式化输出*/
    va_start(valist, format);  /* 初始化变量 */

    string log_str;
    m_mutex.unlock();

    /* 写入的具体时间内容格式 */
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s", 
                    my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_wday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    int m = snprintf(m_buf + n, m_log_buf_size - n - 1, format, valist);
    m_buf[m + n] = '\n';
    m_buf[m + n + 1] = '\0';
    log_str = m_buf;
    m_mutex.unlock();

    /* 若异步写日志， 则将日志信息加入阻塞队列，同步(else)则向加锁文件中写 */
    if(m_is_async && !m_log_queue->full()) {
        m_log_queue->push(log_str);
    }
    else {
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }
    va_end(valist);  //结束变量列表,和va_start成对使用   
}

void Log::flush() {
    m_mutex.lock();
    fflush(m_fp);
    m_mutex.unlock();
}