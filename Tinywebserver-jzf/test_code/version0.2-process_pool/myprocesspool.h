#ifndef MYPROCESSPOOL_H
#define MYPROCESSPOOL_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

class process
{
    int m_pid;               // 子进程的pid
    int m_pipefd[2];         // 与父进程通信的管道，子进程关0，父进程关1
    process() : m_pid(-1){}; // 将mpid初始化为-1用于区分父子进程
};

template <typename T>
class myprocesspool
{
private:
    // 构造函数私有
    myprocesspool(int listenfd, int process_number = 8);

public:
    // 三个公有的成员函数
    static myprocesspool<T> *creat(int listenfd, int process_number = 8)
    {
        if (!m_instance)
        {
            m_instance = new myprocesspool<T>(listenfd, process_number);
            return m_instance;
        }
    }
    ~myprocesspool()
    {
        delete[] m_sub_process;
    }
    // 启动进程池
    void run();

private:
    // 私有成员函数
    // 统一信号源
    void setup_sig_pipe();
    // 启动父进程
    void run_parent();
    // 启动子进程
    void run_child();

private:
    static const int MAX_PROCESS_NUM = 16;
    static const int MAX_EPOLL_NUM = 10000;
    static const int MAX_CONNECT_NUM = 10000;
    // 私有成员变量
    int m_process_number;                // 进程总数
    int m_idx;                           // 子进程序号
    int m_listenfd;                      // 子进程监听fd
    int m_epollfd;                       // 子进程epollfd
    int m_stop;                          // 子进程停止
    process *m_sub_process;              // 子进程信息
    static myprocesspool<T> *m_instance; // 进程池实例
};

template <typename T>
myprocesspool<T> *myprocesspool<T>::m_instance = nullptr;

static int sig_pipe[2];

// 设置非阻塞文件描述符
static int setnonblocking(int fd)
{
    // 记录旧的
    int oldopt = fcntl(fd, F_GETFL);
    // 改新的
    int newopt = oldopt | O_NONBLOCK;
    fcntl(fd, F_SETFL, newopt);
    // 返回旧的
    return oldopt;
}

// 向epoll事件表中加入一个文件描述符
static void addfd(int epollfd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    // 设置为ET
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

static void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 将信号都加入到管道中，以便于之后加入epoll事件表
static void sig_handler(int sig)
{
    int olderrno = errno;
    int msg = sig;
    send(sig_pipe[1], &msg, sizeof(msg), 0);
    errno = olderrno; // 保存errno因为他可能会改变
}

// 注册信号处理函数
static void add_sig(int sig, void(handler)(int), bool restart = true)
{
    struct sigaction sa;
    // 重置sa中内容
    memset(&sa, '\0', sizeof(sa));
    // 将sa的handler设置为自定义的
    sa.sa_handler = handler;
    // 因信号中断的系统调用会重启
    if (restart)
    {
        sa_handler |= SA_RESTART;
    }
    // 阻塞其他信号
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) == -1);
}

template <typename T>
myprocesspool<T>::myprocesspool(int listenfd, int process_number = 8) : m_listenfd(listenfd), m_process_number(process_number), m_idx(-1), m_stop(false)
{
    // 创建子进程的信息数组
    m_sub_process = new process[process_number];
    for (int i = 0; i < process_number; i++)
    {
        // 建立父子进程通讯管道
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_pipefd);
        assert(ret != -1);
        // fork子进程
        m_sub_process[i].m_pid = fork();
        if (m_sub_process[i].m_pid > 0)
        {
            // 父进程关闭写端
            close(m_sub_process[i].m_pipefd[1]);
            continue;
        }
        else
        {
            // 子进程关闭读端
            close(m_sub_process[i].m_pipefd[0]);
            // 更新子进程的序号
            m_idx = i;
            break;
        }
    }
}

// 统一信号源
template <typename T>
void myprocesspool<T>::setup_sig_pipe()
{
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    // 将信号管道建立起来
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipe);
    assert(ret != -1);
    setnonblocking(sig_pipe[1]);
    // 将管道读端加入epoll时间表
    addfd(m_epollfd, sig_pipe[0]);
    // 设置信号处理函数
    add_sig(SIGCHLD, sig_handler); /*子进程终止给发进程发送的信号*/
    addsig(SIGTERM, sig_handler);  /*软件终止信号 */
    addsig(SIGINT, sig_handler);   /*中断进程信号*/
    addsig(SIGPIPE, sig_handler);  /*向一个没有读进程的管道写数据 子进程已关闭，父进程还往与子进程关联的管道写数据会触发*/
}

// 运行进程池，根据midx判断是运行子进程还是父进程
template <typename T>
void myprocesspool<T>::run()
{
    if (m_idx = -1)
    {
        run_parent();
    }
    else
    {
        run_parent();
    }
}

template <typename T>
void myprocesspool<T>::run_parent()
{
    setup_sig_pipe();
    addfd(m_epollfd, m_listenfd);
    epoll_event events[MAX_EPOLL_NUM];
    int last_process = 0;
    int has_conn = 1;
    while (!m_stop)
    {
        // 获取有多少事件就绪了
        int number = epoll_wait(m_epollfd, events, MAX_EPOLL_NUM, -1);
        if (number < 0 && errno != EINTR)
        {
            printf("epoll failure\n");
            break;
        }
        for (int i = 0; i < number; i++)
        {
            // 获取每一个事件
            int sockfd = events[i].data.fd;
            // 如果是链接请求
            if (sockfd == m_listenfd && (m_epollfd & EPOLLIN))
            {
                // 轮询把请求交给子进程
                int selected_process = last_process;
                do
                {
                    if (m_sub_process[selected_process].m_pid != -1)
                        break;
                    selected_process = (selected_process + 1) % m_process_number;
                } while (selected_process != last_process);
                if (m_sub_process[selected_process].m_pid == -1)
                {
                    m_stop = 1;
                    break;
                }
                last_process = selected_process;
                // 告诉子进程有连接请求
                int ret = send(m_sub_process[selected_process].m_pipefd[0], &has_conn, 1, 0);
                assert(ret != -1);
                printf("send request to child %d\n", selected_process);
            }
            else if (sockfd == sig_pipe[0] && (m_epollfd & EPOLLIN))
            {
                // 处理各种信号
                char signals[1024];
                int ret = recv(sockfd, signals, sizeof(signals), 0);
                if (ret <= 0)
                {
                    continue;
                }
                else
                {
                    for (int i = 0; i < ret; ++i)
                    {
                        switch (signals[i])
                        {
                        case SIGCHLD:
                        {
                            pid_t pid;
                            int stat;
                            while (pid = waitpid(-1, &stat, WNOHANG))
                            {
                                for (int i = 0; i < m_process_number; i++)
                                {
                                    if (m_sub_process[i].m_pid == pid)
                                    {
                                        printf("child %d quit\n", pid);
                                        close(m_sub_process[i].m_pipefd[0]);
                                        m_sub_process[i].m_pid = -1;
                                    }
                                }
                            }
                            /*如果所有子进程都退出了，父进程也退出*/
                            m_stop = true;
                            for (int i = 0; i < m_process_number; ++i)
                            {
                                if (m_sub_process[i].m_pid != -1)
                                {
                                    m_stop = false;
                                    break;
                                }
                            }
                            // 处理信号
                            break;
                        }
                        case SIGTERM:
                        case SIGINT:
                        {
                            // 处理信号
                            for (int i = 0; i < m_process_number; i++)
                            {
                                int pid = m_sub_process[i].m_pid;
                                if (pid != -1)
                                {
                                    kill(pid, SIGTERM);
                                }
                            }
                            break;
                        }
                        default:
                            break;
                        }
                    }
                }
            }
        }
    }
    close(m_epollfd);
}

template <typename T>
void myprocesspool<T>::run_child()
{
    setup_sig_pipe();
    int pipefd = m_sub_process[m_idx].m_pipefd[1];
    addfd(m_epollfd, pipefd);
    epoll_event events[MAX_EPOLL_NUM];
    T *users = new T[MAX_CONNECT_NUM];
    while (!m_stop)
    {
        int number = epoll_wait(m_epollfd, events, MAX_EPOLL_NUM, -1);
        if (number < 0 && errno != EINTR)
        {
            printf("epoll failure\n");
            break;
        }
        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;
            // 如果收到连接请求则创建客户连接
            if (sockfd == pipefd && (m_epollfd & EPOLLIN))
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(sockfd, (sockaddr *)&client_address, client_addrlength);
                if (connfd < 0)
                {
                    printf("accept failure, errno is:%d\n", errno);
                    continue;
                }
                addfd(m_epollfd, connfd);
                // 用户需要自定义这个初始化连接的函数
                users[connfd].init(client_address, connfd, m_epollfd);
            }
            else if (sockfd == sig_pipe[0] && (m_epollfd & EPOLLIN))
            {
                char sig[1024];
                int ret = recv(sockfd, sig, sizeof(sig), 0);
                if (ret < 0)
                    continue;
                for (int i = 0; i < ret; i++)
                {
                    switch (sig[i])
                    {
                    case SIGCHLD:
                    {
                        // 子进程应该不会收到这个信号
                        break;
                    }
                    case SIGTERM:
                    case SIGINT:
                    {
                        // 处理信号
                        m_stop = 1;
                        break;
                    }
                    default:
                        break;
                    }
                }
            }
            else if (events[i].events & EPOLLIN)
            {
                printf("recv client request\n");
                users[sockfd].process();
            }
        }
    }
}

#endif