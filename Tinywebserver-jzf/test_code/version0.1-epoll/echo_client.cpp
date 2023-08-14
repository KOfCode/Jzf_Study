#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>

int main(int argc, char *argv[])
{

    if (argc <= 2)
    {
        printf("need ipaddress and port n.o\n");
        return 1;
    }
    int error;
    char *ip = argv[1];
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    error = inet_pton(AF_INET, ip, &address.sin_addr);
    address.sin_port = htons(atoi(argv[2]));
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listenfd != -1);
    error = connect(listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(error != -1);
    printf("connected.....\n");

    char buffer[1024] = {0};
    int str_len = 0;
    int recv_len = 0;
    while (1)
    {
        fputs("client says:\n", stdout);
        fgets(buffer, 1024, stdin);
        if (!strcmp(buffer, "q\n") || !strcmp(buffer, "Q\n"))
            break;
        str_len = send(listenfd, buffer, strlen(buffer), 0);
        recv_len = recv(listenfd, buffer, str_len, 0);
        buffer[str_len] = 0;
        printf("Message from server : %s\n", buffer);
    }
    close(listenfd);
    return 0;
}
