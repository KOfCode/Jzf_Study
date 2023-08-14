#ifndef ECHO_H
#define ECHO_H

#include "myprocesspool.h"
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

class echo
{
private:
    static int epollfd;
    int sockfd;
    sockaddr_in client_addr;
    char buf[1024] = {0};

public:
    echo() {}
    ~echo() {}
    void init(sockaddr_in _client_addr, int _sockfd, int _epollfd)
    {
        client_addr = _client_addr;
        sockfd = _sockfd;
        epollfd = _epollfd;
    }
    void process()
    {
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
            }
            else if (ret == 0)
            {
                removefd(epollfd, sockfd);
            }
            else
            {
                printf("get %d bytes of content: %s\n", ret, buf);
                send(sockfd, buf, ret, 0);
            }
        }
        return;
    }
};

#endif