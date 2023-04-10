#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>

#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

/* 使用局部变量懒汉模式创建连接池 */
class connection_pool {
public:
    MYSQL* GetConnection();               /* 获取数据库连接 */   /* 初始化 */
    bool ReleaseConnection(MYSQL* conn);  /* 释放连接 */
    int GetFreeConn();                    /* 获取连接 */
    void DestroyPool();                   /* 销毁所有连接*/  /* 销毁连接池 */

    /* 局部静态变量单例模式 */
    static connection_pool* GetInstance();

    void init(string Url, string User, string Password, string DataBaseName, int Port, int maxConn, int close_flg);

public:
    string m_url;                        /* 主机地址 */
    string m_Port;                       /* 数据库端口号，数据库也相当于是个服务器 */
    string m_User;                       /* 登录数据库的用户名*/
    string m_PassWord;                   /* 登录数据库的密码*/
    string m_DataBaseName;               /* 使用的数据库名 */
    int m_close_log;                     /* 日志开关 */

private:
    connection_pool();
    ~connection_pool();

    int m_MaxConn;                       /* 最大连接数 */
    int m_CurConn;                       /* 当前已使用的连接数 */
    int m_FreeConn;                      /* 当前空闲的连接数 */
    locker lock;
    list<MYSQL*> connList;               /* 连接池 */
    sem reserver;
};

/* RAII机制释放数据库连接池 */
class connectionRAII {
public:
    /* 双指针对 MYSQL* con修改 */
    connectionRAII(MYSQL** con, connection_pool* connpool);
    ~connectionRAII();
private:
    MYSQL* conRAII;
    connection_pool* poolRAII;
};

#endif
