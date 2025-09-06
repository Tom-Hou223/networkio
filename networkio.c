#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/select.h>
#include  <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>




void* client_thread(void* arg)
{
    int clientfd = *(int*)arg;

    while(1)
    {
        char buf[1024] = { 0 };
        int count = recv(clientfd, buf, 1024, 0);
        printf("recv: %s\n", buf);//阻塞在这里
        if(count == 0)
        {
            printf("client disconnect:%d\n", count);
            close(clientfd);
            break;
        }

        send(clientfd, buf, strlen(buf), 0);
        printf("send: %d\n", count);
    }

}



//主线程
int main()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//0.0.0.0本地网卡
    servaddr.sin_port = htons(2000);

    if(bind(sockfd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr)) == -1)
    {
        printf("bind failed: %s\n", strerror(errno));
        return -1;
    }

    listen(sockfd, 10);
    printf("listening finished:%d\n", sockfd);


    struct sockaddr_in clientaddr;
    socklen_t clientlen = sizeof(clientaddr);

#if 0
    //都是阻塞式串行执行
    while(1)
    {
        printf("accepting...\n");
        int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &clientlen);
        printf("accept finished:%d\n", clientfd);

        pthread_t thid;
        pthread_create(&thid, NULL, client_thread, &clientfd);
    }
#elif 0
    //select实现多路复用
    fd_set readfds, rset;//位图
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);
    int maxfd = sockfd;

    while(1)
    {
        rset = readfds;//要传入内核的bitmap
        int nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if(FD_ISSET(sockfd, &rset))
        {
            int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &clientlen);
            printf("accept finished:%d\n", clientfd);

            FD_SET(clientfd, &readfds);

            clientfd > maxfd ? (maxfd = clientfd) : (0);//更新最大文件描述符
        
        }

        //recv
        int i;
        for(i = sockfd + 1; i <= maxfd; i++)
        {
            if(FD_ISSET(i, &rset))
            {
                char buffer[1024] = { 0 };
               int count = recv(i, buffer, 1024, 0);
               printf("RECV: %s\n", buffer);
               if(count == 0)
               {
                   printf("client disconnect:%d\n", count);
                   close(i);
                   FD_CLR(i, &readfds);
                   continue;
               }

                send(i, buffer, 1024, 0);
                printf("SEND: %d\n", count);
            }
        }

    }
#elif 0
    //poll的出现为了解决select的文件描述符上限问题，参数多
    struct pollfd fds[1024] = { 0 };
    fds[sockfd].fd = sockfd;//对应位图
    fds[sockfd].events = POLLIN;//关注可写

    int maxfd = sockfd;

    while(1)
    {
        int nready = poll(fds, 1024, -1);

        if(fds[sockfd].revents & POLLIN)
        {
            int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &clientlen);
            printf("accept finished:%d\n", clientfd);

            fds[clientfd].fd = clientfd;
            fds[clientfd].events = POLLIN;

            if(clientfd > maxfd)//更新最大文件描述符
            {
                maxfd = clientfd;
            }
        }

        int i;
        for(i = sockfd + 1; i <= maxfd; i++)
        {
            if(fds[i].revents & POLLIN)
            {
                char buffer[1024] = { 0 };

                int count = recv(i, buffer, 1024, 0);
                printf("RECV: %s\n", buffer);

                if(count == 0)
                {
                    printf("client disconnect:%d\n", count);
                    close(i);

                    fds[i].fd = -1;
                    fds[i].events = 0;
                    continue;
                }
                send(i, buffer, 1024, 0);
                printf("SEND: %s,are you ok?\n", buffer);
            }
        }

    }



#else
    //epoll
    int epollfd = epoll_create(1);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sockfd;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev);

    while(1)
    {
        struct epoll_event events[1024] = { 0 };
        int nready = epoll_wait(epollfd, events, 1024, -1);



        int i;
        for(i = 0; i < nready; i++)
        {
            int connfd = events[i].data.fd;
            if(connfd == sockfd)
            {
                int clientfd = accept(sockfd, (struct sockaddr*)&clientaddr, &clientlen);
                printf("accept finished:%d\n", clientfd);

                ev.events = EPOLLIN;
                ev.data.fd = clientfd;
                epoll_ctl(epollfd, EPOLL_CTL_ADD, clientfd, &ev);

            }
            else if(events[i].events & EPOLLIN)
            {
                char buffer[1024] = { 0 };
                int count = recv(connfd, buffer, 1024, 0);
                printf("RECV: %s\n", buffer);
                if(count == 0)
                {
                    printf("client disconnect:%d\n", count);
                    close(connfd);
                    epoll_ctl(epollfd, EPOLL_CTL_DEL, connfd, NULL);
                    continue;
                }
                send(connfd, buffer, 1024, 0);
                printf("SEND: %s,are you ok?\n", buffer);
            }
        }
    }
#endif

    getchar();
    exit(1);
    return 0;
}