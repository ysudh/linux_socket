/*************************************************************************
	> File Name: client.c
	> Author: donghao
	> Mail: 参考：https://www.cnblogs.com/Anker/archive/2013/08/17/3263780.html
    > description:利用epoll实现 C/S模式中的client
	> Created Time: 2017年12月19日 星期二 19时04分52秒
 ************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <net/if.h> 
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h> 
#include <fcntl.h>
#include <assert.h>
#include <sys/epoll.h>

/* epoll 监听STDIN_FILENO STDOUT_FILENO sockfd三个描述符*/

#define MAXSIZE 1024
#define IPADDRESS "127.0.0.1"
#define SERV_PORT 8787
#define FDSIZE 1024
#define EPOLLEVENTS 20

static void handle_connection(int sockfd);
static void handle_events(int epollfd,struct epoll_event *events,int num,int sockfd,char *buf);
static void do_write(int epollfd,int fd,char *buf);
static void do_read(int epollfd,int fd,int sockfd,char *buf);
static void add_event(int epollfd,int fd,int state);
static void delete_event(int epollfd,int fd,int state);
static void modify_event(int epollfd,int fd,int state);
static int socket_init(const char *ip,int port);

int main(int argc,char **argv)
{
    int clientfd;
    clientfd = socket_init(IPADDRESS,SERV_PORT);
    
    handle_connection(clientfd);
    close(clientfd);
    return 0;
}

static int socket_init(const char *ip,int port)
{
    int sockfd;
    int re;
    struct sockaddr_in servaddr;
    sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd<0)
    {
        printf("socket error errno = %d(%s)\n",errno,strerror(errno));
        return -1;
    }
    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT); //主机字节序--->网络字节序
    inet_pton(AF_INET,IPADDRESS,&servaddr.sin_addr); 

    re = connect(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr));
    if(re<0)
    {
        printf("connect error errno = %d(%s)\n",errno,strerror(errno));
        return -1;
    }
    return sockfd;
}
static void add_event(int epollfd,int fd,int state)
{
    struct epoll_event ev;
    ev.events = state; //表示对应的文件描述符的状态（读、写...)
    ev.data.fd = fd;   //监听套接字
    /*events(应用-->内核)告知内核需要监听什么事*/
    if(epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&ev)==-1)
    {
        perror("epoll_ctl:EPOLL_CTL_ADD");
        exit(EXIT_FAILURE);
    }
}
static void delete_event(int epollfd,int fd,int state)
{
    struct epoll_event ev;
    ev.events = state;
    ev.data.fd = fd;
    if(epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,&ev)==-1)
    {
        perror("epoll_ctl:EPOLL_CTL_DEL");
        exit(EXIT_FAILURE);        
    }
}
static void modify_event(int epollfd,int fd,int state)
{
    struct epoll_event ev;
    ev.events = state;
    ev.data.fd = fd;
    if(epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&ev)==-1)
    {
        perror("epoll_ctl:EPOLL_CTL_MOD");
        exit(EXIT_FAILURE);        
    }
}
static void handle_connection(int sockfd)
{
    int epollfd;
    int nfds;
    char sockreadbuf[MAXSIZE] = {0}; 
    char sockwrite[MAXSIZE] = {0}//
    char stdinbuf
    char stdoutbuf
    struct epoll_event events[EPOLLEVENTS];
    epollfd = epoll_create(FDSIZE); //FDSIZE >0无意义
    if(epollfd == -1)
    {
        perror("epoll_create");
        exit(EXIT_FAILURE);
    }
    /*STDIN 读事件*/
    add_event(epollfd,STDIN_FILENO,EPOLLIN);
    /*添加sockfd写事件*/
    add_event(epollfd,sockfd,EPOLLOUT);
    while(1)
    {
        /*events(内核-->应用程序)用来从内核得到事件的集合；返回需要处理的事件数目*/
        nfds = epoll_wait(epollfd,events,EPOLLEVENTS,-1); 
        if(nfds == -1)
        {
           perror("epoll_wait");
           exit(EXIT_FAILURE);
        }
        handle_events(epollfd,events,nfds,sockfd,buf);
    }
    /*当创建好epoll句柄后，它就是会占用一个fd值，在linux下如果查看/proc/进程id/fd/，
      是能够看到这个fd的，所以在使用完epoll后，必须调用close()关闭，否则可能导致fd被耗尽。*/
    close(epollfd);
}
static void handle_events(int epollfd,struct epoll_event *events,int num,int sockfd,char *buf)
{   
    int fd;
    int i=0;
    for(i=0;i<num;i++)
    {
        /*判断返回的事件是读还是写*/
        fd = events[i].data.fd;
        if(events[i].events & EPOLLIN)
            do_read(epollfd,fd,sockfd,buf);
        if(events[i].events & EPOLLOUT)
            do_write(epollfd,fd,buf);
    }
}
static void do_read(int epollfd,int fd,int sockfd,char *buf)
{
    int nread;
    nread=read(fd,buf,MAXSIZE);
    if(nread == -1)
    {
        perror("read error");
        close(fd);
    }
    else if(nread == 0)
    {
        fprintf(stderr,"server close\n");
        close(fd);
    }
    else
    {
        if(fd == STDIN_FILENO)
           // add_event(epollfd,sockfd,EPOLLOUT);/*添加sockfd写事件*/
        else 
        {
           // delete_event(epollfd,sockfd,EPOLLIN); /*删除sockfd上的读事件*/
            add_event(epollfd,STDOUT_FILENO,EPOLLOUT);/*添加STDOUT 写事件，打印到屏幕*/
        }
    }

}
static void do_write(int epollfd,int fd,char *buf)
{
    int nwrite;
    nwrite = write(fd,buf,strlen(buf));
    if(nwrite ==-1)
    {
        perror("read error");
        close(fd);
    }
    else
    {
        if(fd == STDOUT_FILENO)
            delete_event(epollfd,fd,EPOLLOUT);
        else
            modify_event(epollfd,fd,EPOLLIN);
    }
    memset(buf,0,MAXSIZE);
}
