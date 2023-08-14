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
#include "myprocesspool.h"
#include "echo.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        puts("need more paras\n");
        return -1;
    }
    const char *ip = argv[1];
    sockaddr_in sa;
    memset(sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &address.sin_addr);
    sa.sin_port = htons(atoi(argv[2]));
    int error;
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd != -1);
    error = bind(listenfd, (sockaddr *)&sa, sizeof(sa));
    assert(error != -1);
    error = listen(listenfd, 5);
    assert(error != -1);
    myprocesspool<echo> *pool = processpool<echo>::create(listenfd, 8);
    pool->run();

    close(listenfd);

    return 0;
}