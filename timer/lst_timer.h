#ifndef LST_TIMER
#define LST_TIMER

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <time.h>

#include "../log/log.h"

class util_timer;  /* tiqianshengming */

/* 连接资源：客户端数据 */
class client_data {
public:
    sockaddr_in address;         /* 客户端 socket 地址 */
    int sockfd;                  /* 客户端 socket 文件描述符 */
    util_timer* timer;           /* 客户资源类中有定时器：------------>> */
};

/* 定时器类 */
class util_timer {
public:
    util_timer() : prev(nullptr), next(nullptr) {}
public:
    time_t expire;               /* 超时时间 */
    void (*cb_func)(client_data*);  /* 回调函数 cb_func是个指针 */
    client_data* user_data;      /* 连接资源 */
    util_timer* prev;            /* 前向定时器 */
    util_timer* next;            /* 后继定时器 */
};

/* 定时器容器类 */
class sort_timer_lst {
public:
    sort_timer_lst();
    ~sort_timer_lst();

    void add_timer(util_timer* timer);
    void adjust_timer(util_timer* timer);    /* 当定时器任务发生变化时，调整定时器在链表中的位置 */
    void del_timer(util_timer* timer);
    void tick();
private:
    /* 私有成员函数， 被公有 add_timer 和 adjust_timer 调用，主要用于调整链表内部结构 */
    void add_timer(util_timer* timer, util_timer* lst_head);

    util_timer* head;           /* 链表头节点 */
    util_timer* tail;           /* 链表尾节点*/
};

/* 使用定时器类 */
class Utils {
public:
    Utils() {};
    ~Utils() {};

    void init(int timeslot);

    /* 对文件描述符设置为非阻塞 */
    int setNonBlocking(int fd);

    /* 将内核事件表注册可读事件，ET模式，选择开启EPOLLONESHOT */
    void addfd(int epollfd, int fd, bool one_shot, int TRGIMode);

    /* 信号处理函数 */
    static void sig_handler(int sig);

    /* 设置信号函数*/
    void addsig(int sig, void(handler)(int), bool restart = true);

    /* 定时处理任务，重新定义时不断触发SIGALARM信号 */
    void timer_handler();

    void show_error(int connfd, const char* info);

public:
    static int* u_pipefd;
    sort_timer_lst m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
};

void cb_func(client_data* user_data);

#endif