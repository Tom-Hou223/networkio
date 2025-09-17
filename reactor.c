#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>


#define BUFFER_LENGTH 20
#define CONNECTION_SIZE 1048576
#define MAX_PORT 20
#define TIME_SUB_MS(tv1, tv2)  ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)

typedef int(*RCALLBACK)(int fd);
int accept_cb(int fd);
int recv_cb(int fd);
int send_cb(int fd);
int set_event(int fd, int event, int flag);
int init_server(unsigned short port);
int event_register(int fd, int event);

struct timeval begin;
int epfd = 0;

struct conn {
    int fd;
    char rbuffer[BUFFER_LENGTH];
    int rlength;

    char wbuffer[BUFFER_LENGTH];
    int wlength;

    //EPOLLIN
    union {
        RCALLBACK recv_callback;
        RCALLBACK accept_callback;
    }r_action;
    //EPOLLOUT
    RCALLBACK send_callback;
};

/*连接池*/
struct conn conn_list[CONNECTION_SIZE] = { 0 };


int accept_cb(int fd)
{
    struct sockaddr_in clientaddr;
    socklen_t clientlen = sizeof(clientaddr);

    int clientfd = accept(fd, (struct sockaddr*)&clientaddr, &clientlen);
    //printf("accept finished:%d\n", clientfd);

    if(clientfd < 0)
    {
        printf("accept failed: %d --> %s\n", errno, strerror(errno));
        return -1;
    }

    event_register(clientfd, EPOLLIN );

    if((clientfd % 1000) == 0)
    {
        struct timeval current;
        gettimeofday(&current, NULL);

        int time_used = TIME_SUB_MS(current, begin);
        memcpy(&begin, &current, sizeof(struct timeval));
        printf("accept count:%d, time:%d\n", clientfd, time_used);
    }

    return clientfd;
}

int recv_cb(int fd)
{
    memset(conn_list[fd].rbuffer, 0, BUFFER_LENGTH);
    int count = recv(fd, conn_list[fd].rbuffer, BUFFER_LENGTH, 0);
    if(count == 0)
    {
        printf("client disconnect:%d\n", fd);
        close(fd);
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);// unfinished

        return 0;
    }
    conn_list[fd].rlength = count;
    //printf("recv data:%s\n", conn_list[fd].rbuffer);

#if 1 //echo
    conn_list[fd].wlength = conn_list[fd].rlength;
    memcpy(conn_list[fd].wbuffer, conn_list[fd].rbuffer, conn_list[fd].wlength);
    printf("[%d]RECV: %s\n",conn_list[fd].rlength ,conn_list[fd].rbuffer);
#endif


    //set_event(fd, EPOLLOUT, 0);
    

    return count;
}

int send_cb(int fd)
{
    int count = send(fd, conn_list[fd].wbuffer, conn_list[fd].wlength, 0);

    set_event(fd, EPOLLIN, 0);

    return count;
}


int set_event(int fd, int event, int flag)
{
    if(flag)//non-zero add
    {
        struct epoll_event ev;
        ev.events = event;
        ev.data.fd = fd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    }
    else//zero modify
    {
        struct epoll_event ev;
        ev.events = event;
        ev.data.fd = fd;
        epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
    }
    return 0;
}

int event_register(int fd, int event)
{
    if(fd < 0)
    {
        return -1;
    }
    conn_list[fd].fd = fd;
    conn_list[fd].r_action.recv_callback = recv_cb;
    conn_list[fd].send_callback = send_cb;

    memset(conn_list[fd].rbuffer, 0, BUFFER_LENGTH);
    conn_list[fd].rlength = 0;

    memset(conn_list[fd].wbuffer, 0, BUFFER_LENGTH);
    conn_list[fd].wlength = 0;

    set_event(fd, event, 1);

}



int init_server(unsigned short port)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//0.0.0.0本地网卡
    servaddr.sin_port = htons(port);

    if(bind(sockfd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr)) == -1)
    {
        printf("bind failed: %s\n", strerror(errno));
        return -1;
    }

    listen(sockfd, 10);
    //printf("listening finished:%d\n", sockfd);

    return sockfd;
}


int main()
{
    unsigned short port = 2000;


    epfd = epoll_create(1);



    int i;
    for(i = 0; i < MAX_PORT; i++)
    {
        int sockfd = init_server(port + i);
        conn_list[sockfd].fd = sockfd;
        conn_list[sockfd].r_action.accept_callback = accept_cb;
        set_event(sockfd, EPOLLIN, 1);
    }


    gettimeofday(&begin, NULL);

    while(1)
    {
        struct epoll_event events[1024] = { 0 };
        int nready = epoll_wait(epfd, events, 1024, -1);
        for(int i = 0; i < nready; i++)
        {
            int connfd = events[i].data.fd;
            if(events[i].events & EPOLLIN)
            {
                conn_list[connfd].r_action.recv_callback(connfd);
            }
            if(events[i].events & EPOLLOUT)
            {
                conn_list[connfd].send_callback(connfd);
            }

        }
    }
    return 0;
}