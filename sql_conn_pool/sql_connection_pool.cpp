#include "sql_connection_pool.h"

connection_pool::connection_pool() {
    m_CurConn = 0;
    m_FreeConn = 0;
}

connection_pool* connection_pool::GetInstance() {
    static connection_pool connpool;
    return & connpool;
}

/* 初始化构造 */
void connection_pool::init(string Url, string User, string PassWord, string DataBaseName, int Port, int MaxConn, int close_log) {
    m_url = Url;
    m_User = m_User;
    m_PassWord = PassWord;
    m_DataBaseName = DataBaseName;
    m_Port = Port;
    m_close_log = close_log;

    /* 创建MaxConn条数据库连接 */
    for (int i = 0; i < MaxConn; i++) {
        /* 分配或初始化与mysql_real_connect()相适应的MYSQL对象。用mysql_init()函数 */
        /* 如果mysql是NULL指针，该函数将分配、初始化、并返回新对象。
        否则，将初始化对象，并返回对象的地址。
        如果mysql_init()分配了新的对象，当调用mysql_close()来关闭连接时。将释放该对象。返回值*/
        MYSQL* con = nullptr;
        con = mysql_init(con);

        if (con == nullptr) {
            LOG_ERROR("%s", "MySQL Error");
            exit(1);
        }

        /* 连接数据库引擎，通过函数mysql_real_connect()尝试与运行在主机上的MySQL数据库引擎建立连接。 */
        con = mysql_real_connect(con, Url.c_str(), User.c_str(), PassWord.c_str(), DataBaseName.c_str(), Port, NULL, 0);
        if (con == nullptr) {
            LOG_ERROR("%s", "MySQL Error");
            exit(1);
        }

        /* 更新连接池和空闲连接数量 */
        connList.push_back(con);  /* connList: MYSQL*类型的List */
        m_FreeConn++;
    }

    /* 将信号量初始化为最大连接次数 */
    reserver = sem(m_FreeConn);
    m_MaxConn = m_FreeConn;
}

/* 获取、释放连接
当线程数量大于数据库连接数量时，使用信号量进行同步：每次取出连接，信号量原子减1。
每次释放连接，信号量原子加1。 若连接池内没有连接了，则阻塞等待。
另外，由于多线程操作连接池，会造成竞争。 因此这里使用互斥锁同步，具体的同步机制使用lock.h封装好的locker类。
*/

/* 当有请求时，从数据库连接池中返回一个可用的连接，更新使用和空闲连接数 */
MYSQL* connection_pool::GetConnection() {
    MYSQL* con = nullptr;

    if (connList.size() == 0) {
        return nullptr;
    }

    // 取出连接，信号量原子减1，为0则阻塞等待
    reserver.wait();  /* list 是临界资源，以连接池中的资源个数为"SV"？线程间互斥访问 */
                        /* 与之对应的，当归还的时候才 V 操作 */
    lock.lock();  /* 锁住对list元素的可能操作 */  /* 锁，是保证操作的原子性 */  /* 获取、释放的原子性 */

    con = connList.front();
    connList.pop_front();
    m_FreeConn--;
    m_CurConn++;

    lock.unlock();
    return con;
}

/* 释放当前使用的连接 */
bool connection_pool::ReleaseConnection(MYSQL* con) {
    if (con == nullptr) {
        return false;
    }

    lock.lock();

    connList.push_back(con);
    m_CurConn--;
    m_FreeConn++;

    lock.unlock();

    reserver.post();   /* V 操作 ： 释放连接原子加1 */
    return true;
}

/* 销毁数据库连接池 */  /* 通过迭代器遍历连接池链表，关闭对应数据库连接池，清空链表并重置空闲连接和现有的连接数量 */
void connection_pool::DestroyPool() {
    lock.lock();
    if (connList.size() > 0) {
        list<MYSQL*>::iterator it;
        for (it = connList.begin(); it != connList.end(); it++) {
            MYSQL* con = *it;
            mysql_close(con);
        }
        m_CurConn = 0;
        m_FreeConn = 0;
    }
    lock.unlock();
}

/* 当前空闲连接数 */
int connection_pool::GetFreeConn() {
    return this->m_CurConn;
} 

/* 使用 RAII机制 销毁连接池 */  /* 即析构函数内销毁资源 */
connection_pool::~connection_pool() {
    DestroyPool();
}

/* RAII 机制释放数据库连接：将数据库连接的 获取与释放 通过RAII机制封装， 避免手动释放 */
/* 不直接调用获取和释放连接的接口，将其封装起来，通过RAII机制进行获取和释放 */
connectionRAII::connectionRAII(MYSQL** sql, connection_pool* connpool) {
    *sql = connpool->GetConnection();  /* 连接池中的一个连接 */
    conRAII = *sql;
    poolRAII = connpool;
}

connectionRAII::~connectionRAII() {
    poolRAII->ReleaseConnection(conRAII);
}