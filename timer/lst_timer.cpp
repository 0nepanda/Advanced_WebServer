#include "lst_timer.h"
#include "../http/http_conn.h"

sort_timer_lst::sort_timer_lst() {
    head = nullptr;
    tail = nullptr;
}

/* 销毁定时器容器(链表) */
sort_timer_lst::~sort_timer_lst() {
    util_timer* tmp = head;
    while (tmp) {
        head = head->next;
        delete tmp;
        tmp = head;
    }
}

/* 添加定时器到升序链表 */
void sort_timer_lst::add_timer(util_timer* timer) {
    if (!timer) {
        return;
    }
    /* 如果此时升序链表内没有定时器 */
    if (!head) {
        head = tail = nullptr;
        return;
    }
    /* 若添加的定时器超时时间比头节点超时时间还小，则其成为链头 */
    if (timer->expire < head->expire) {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    /* 否则调用重载版本, 按序添加到中部 */
    add_timer(timer, head);
}

/* 当某个定时任务发生改变时，调整该指定的定时器在链表中的位置 */
/* 只考虑定时器被延长的情况 */
void sort_timer_lst::adjust_timer(util_timer* timer) {
    if (!timer) {
        return;
    }
    util_timer* tmp = timer->next;
    if (!tmp || (timer->expire <= tmp->expire)) {  /* 如果timer是队尾，或timer延长后仍小于其后继 */
        return;
    }
    if (head == timer) {  /* timer为头节点 */  /* 拿出去再放回 */
        head = head->next;
        head->prev = nullptr;
        timer->next = nullptr;
        add_timer(timer, head);
    }
    else {  /* 非头非尾，且延长后大于后继 */
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, head);
    }
}

void sort_timer_lst::del_timer(util_timer* timer) {
    if (!timer) {
        return;
    }
    else if ((timer == head) && (timer == tail)) {  /* 仅有一个节点 */
        delete timer;
        head = nullptr;
        tail = nullptr;
        return;
    }
    else if (timer == head) {
        head = head->next;
        head->prev = nullptr;
        delete timer;
        return;
    }
    else if (timer == tail) {
        tail = tail->prev;
        tail->next = nullptr;
        delete timer;
        return;
    }
    else {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
    }
}

/* 定时器到期处理函数 */
void sort_timer_lst::tick() {  /* 被Utils的time_handler调用 */
    if (!head) {
        return;
    }

    time_t cur = time(nullptr);  /* 获取当前时间 */
    util_timer* tmp = head;
    while (tmp) {
        /* 当前定时器内设的超时时间 大于 当前时间*/
        if (cur < tmp->expire) {
            break;
        }
        /* 否则说明有定时器到期 */
        tmp->cb_func(tmp->user_data);
        head = head->next;
        if (head) {
            head->prev = nullptr;
        }
        delete tmp;
        tmp = head;
    }
}

/* 私有成员函数， 被公有 add_timer 和 adjust_timer 调用，主要用于调整链表内部结构 */
void sort_timer_lst::add_timer(util_timer* timer, util_timer* lst_head) {
    /* 被调用时，是按序插入到链表的中部。则需要找到合适位置：之前比它小，之后比它大 */
    util_timer* prev = lst_head;
    util_timer* tmp = lst_head->next;
    while (tmp) {
        if (timer->expire < tmp->expire) {  /* 逻辑上取反：完成才进if去break。 不是将思路局限于不完成时怎么样 */
            prev->next = timer;
            timer->prev = prev;
            timer->next = tmp;
            tmp->prev = timer;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    /* 原升序链表中没有比要插入的timer更大，则timer去队尾 */
    if (!tmp) {
        prev->next = timer;
        timer->prev = prev;  /* 双向链表 */
        timer->next = nullptr;  /* 尾置空 */
        this->tail = timer;
    }
}


/****************************** 使用定时器类 *****************************/
void Utils::init(int timeslot) {
    m_TIMESLOT = timeslot;
}

/* 将文件描述符设置为非阻塞 */
int Utils::setNonBlocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

/* 向内核事件表注册读事件， ET模式， 选择开启 EPOLLONESHOT */
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRGIMode) {
    epoll_event event;
    event.data.fd = fd;

    if (TRGIMode == 1) {
        event.events = EPOLLIN | EPOLLET | EPOLLHUP;
    }
    else {
        event.events = EPOLLIN | EPOLLHUP;
    }
    if (one_shot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setNonBlocking(fd);  /* 将读事件设置为非阻塞？ */
}

/* 信号处理函数 */
void Utils::sig_handler(int sig) {
    /* 为了保证函数的可重入性，保留原来的 errno */
    /* 函数的可重入性：
    重入一般可以理解为一个函数在同时多次调用
    可重入的函数必须满足以下三个条件：
    （1）可以在执行的过程中可以被打断；
    （2）被打断之后，在该函数一次调用执行完之前，可以再次被调用（或进入，reentered)。
    （3）再次调用执行完之后，被打断的上次调用可以继续恢复执行，并正确执行。
    
    可重入函数可以在任意时刻被中断，稍后再继续运行，不会丢失数据。
    不可重入（non-reentrant）函数不能由两个任务(及以上)所共享，除非能确保函数的互斥（或者使用信号量，或者在代码的关键部分禁用中断）。
    */
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char*)&msg, 1, 0);
    errno = save_errno;
}

/* 设置信号函数 */
void Utils::addsig(int sig, void(handler)(int), bool restart) {  /* WebServer中调用时填写Utils.sig_handler */
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;  
    if (restart) {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);  /* 将参数set信号集初始化， 然后把所有的信号加入到此信号集里 */
    assert(sigaction(sig, &sa, nullptr) != -1);
}

/* 定时处理任务 */
void Utils::timer_handler() {  /* 被谁调用？ */
    m_timer_lst.tick(); 
    alarm(m_TIMESLOT);  /* 设置信号传送闹钟，即用来设置信号SIGALARM在经过参数TIMESLOT秒数后发送给 目前进程(主循环？) */
}

void Utils::show_error(int connfd, const char* info) {
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

/* 静态static成员类外初始化 */
int *Utils::u_pipefd = 0;  
int Utils::u_epollfd = 0;  /* 回调函数的epoll_ctl中调用 */

class Utils;

void cb_func(client_data* user_data) {
    /* 该系统调用对文件描述符epfd引用的epoll实例执行控制操作。它要求对目标文件描述符fd执行op操作。 */
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;  
}