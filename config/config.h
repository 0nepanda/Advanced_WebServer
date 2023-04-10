#ifndef CONFIG_H
#define CONFIG_H

#include "../WebServer/WebServer.h"

using namespace std;

class Config {
public:
    Config();
    ~Config() {};   /* 不加{} 且没在cpp中实现的话 会报错：对‘Config::~Config()’未定义的引用*/

    void parse_arg(int argc, char* argv[]);

    int port;               /* 端口号 */
    int logWrite;           /* 日志写入方式 */
    int trigMode;           /* 触发组合方式 */
    int listenTrigMode;     /* listenfd 触发模式 */
    int connTrigMode;       /* connfd 触发方式 */
    int opt_linger;         /* 优雅的关闭连接 */
    int sql_num;            /* 数据库连接池的数量 */
    int thread_num;         /* 线程池内的线程数量 */
    int close_log;          /* 是否关闭日志 */
    int actor_model;        /* 并发模型选择 */
};

#endif