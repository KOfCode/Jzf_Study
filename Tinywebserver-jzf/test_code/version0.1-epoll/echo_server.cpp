#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#define MAX_EVENT_NUM 1024
#define BUFFER_SIZE 10

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

int addfd(int epollfd, int fd, bool enable_et)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if (enable_et)
    {
        event.events = event.events | EPOLLET;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void use_lt(epoll_event *events, int nums, int epollfd, int listenfd)
{
    char buf[BUFFER_SIZE];
    for (int i = 0; i < nums; i++)
    {
        int sockfd = events[i].data.fd;
        if (sockfd == listenfd)
        {
            struct sockaddr_in client_address;
            socklen_t client_addrlength = sizeof(client_address);
            int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            addfd(epollfd, connfd, false);
        }
        else if (events[i].events & EPOLLIN)
        {
            printf("event trigger once!\n");
            memset(buf, '\0', BUFFER_SIZE);
            int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
            if (ret <= 0)
            {
                close(sockfd);
                continue;
            }
            printf("get %d bytes of content: %s\n", ret, buf);
            send(sockfd, buf, ret, 0);
        }
        else
        {
            printf("strange ?\n");
        }
    }
}

void use_et(epoll_event *events, int nums, int epollfd, int listenfd)
{
    char buf[BUFFER_SIZE];
    for (int i = 0; i < nums; i++)
    {
        int sockfd = events[i].data.fd;
        if (sockfd == listenfd)
        {
            struct sockaddr_in client_address;
            socklen_t client_addrlength = sizeof(client_address);
            int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength);
            addfd(epollfd, connfd, true);
        }
        else if (events[i].events & EPOLLIN)
        {
            printf("event trigger once!\n");
            while (1)
            {
                memset(buf, '\0', BUFFER_SIZE);
                int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
                if (ret < 0)
                {
                    if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                    {
                        printf("read later...\n");
                        break;
                    }
                    close(sockfd);
                    break;
                }
                else if (ret == 0)
                {
                    close(sockfd);
                }
                else
                {
                    printf("get %d bytes of content: %s\n", ret, buf);
                    send(sockfd, buf, ret, 0);
                }
            }
        }
        else
        {
            printf("strange ?\n");
        }
    }
}

int main(int argc, char *argv[])
{

    int error;
    if (argc <= 2)
    {
        printf("need ipaddress and port n.o\n");
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd != -1);
    error = bind(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(error != -1);
    error = listen(listenfd, 5);
    assert(error != -1);

    epoll_event events[MAX_EVENT_NUM];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    bool enable_et;
    if (USE_ET == 1)
    {
        enable_et = true;
    }
    else
    {
        enable_et = false;
    }

    addfd(epollfd, listenfd, enable_et);

    char buffer[1024] = {0};
    int recv_size = 0;
    assert(recv_size != -1);
    char send_buffer[1024] = {0};
    while (1)
    {
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUM, -1);
        if (ret < 0)
        {
            printf("epoll failed\n");
            break;
        }
        if (enable_et)
        {
            use_et(events, ret, epollfd, listenfd);
        }
        else
        {
            use_lt(events, ret, epollfd, listenfd);
        }
    }

    close(listenfd);
    return 0;
}
