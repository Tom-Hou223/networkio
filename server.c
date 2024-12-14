#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void* client_pthread(void* arg)
{
    int clientfd = *(int*)arg;

    // 这里的循环是为了让同一个客户端收发多次数据
    while (1) {
        char buffer[1024] = { 0 };
        int count = recv(clientfd, buffer, 1024, 0);
        printf("RECV: %s\n", buffer);
        if (count == 0) { // disconnect
            printf("client disconnect: %d\n", clientfd);
            close(clientfd);
            break;
        }
        count = send(clientfd, buffer, count, 0);
        printf("SEND: %d\n", count);
    }
}

int main()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(2000);
    if (bind(sockfd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr)) == -1) {
        printf("bind failed: %s\n", strerror(errno));
        return -1;
    }
    listen(sockfd, 10);
    printf("listen finished: %d\n", sockfd);

    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);
#if 0
    printf("accept\n");
    int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);
    printf("accept finished\n");

    char buffer[1024] = { 0 };
    int count = recv(clientfd, buffer, 1024, 0);
    printf("RECV: %s\n", buffer);

    send(clientfd, buffer, count, 0);
    printf("SEND: %d\n", count);

#elif 0
    while (1) {
        printf("accept\n");
        int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);
        printf("accept finished\n");

        char buffer[1024] = { 0 };
        int count = recv(clientfd, buffer, 1024, 0);
        printf("RECV: %s\n", buffer);

        send(clientfd, buffer, count, 0);
        printf("SEND: %d\n", count);
    }

#else
    while (1) {
        printf("accept\n");
        int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &len);
        printf("accept finished: %d\n", clientfd);

        pthread_t thid;
        pthread_create(&thid, NULL, client_pthread, &clientfd);
    }
#endif
    getchar();
    printf("exit\n");

    return 0;
}