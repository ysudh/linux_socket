/*************************************************************************
	> File Name: server.c
	> Author: donghao
	> Mail: 利用epoll实现 C/S模式中的slient
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
/* 最大缓存区大小 */
#define MAXBUFF 128
#define IPADDR "127.0.0.1"
#define PORT 8787
#define EPOLLEVENTS 100
#define LISTEN 10
/* epoll最大监听数 */
#define FDSIZE 100
/* ET模式 */
#define EPOLL_ET 1
/* LT模式 */
#define EPOLL_LT 0
/* 文件描述符设置阻塞 */
#define FD_BLOCK 0
/* 文件描述符设置非阻塞 */
#define FD_NONBLOCK 1
/*epoll 事件的触发条件 */

//static函数的作用域是本源文件，可以它想象为面向对象中的private函数就可以了
//建立监听的
static int create_server_proc(const char *ip, int port);
//链接一个client
static int accept_client_proc(int epollfd, int listenfd, int epoll_type, int block_type);

/*接收 发送 IO处理 epoll默认时LT模式*/
static void write_client_msg_LT(int epollfd, int fd, char *buf);
static void recv_client_msg_LT(int epollfd, int fd, char *buf);
/*接收 发送 IO处理 epoll默认时ET模式*/
static void write_client_msg_ET(int epollfd, int fd, char *buf);
static void recv_client_msg_ET(int epollfd, int fd, char *buf);

/*处理接入的client*/
static void handle_client_proc(int epollfd, struct epoll_event *events, int num,
                               int listenfd, char *buf, int epoll_type, int block_type);
static int do_epoll(int fd);

/*epoll的事件注册,*/
static void add_event(int epollfd, int fd, int epoll_type, int block_type, int state);
/*删除事件,*/
static void del_event(int epollfd, int fd, int state);
/*改变事件,*/
static void modify_event(int epollfd, int fd, int state);
/*设置 非阻塞*/
int set_nonblock(int fd);

char buf[MAXBUFF] = {0}; //服务器写缓存取
typedef struct data
{
    int clifd;
    char data[MAXBUFF];
} cli_data; //缓存读client的信息
cli_data *clidata;

char clifd_group[FDSIZE]={0}; //标志server连接clifd的结合.0/1:断开/连接

int main(int argc, char **argv)
{
    int srvfd;
    srvfd = create_server_proc(IPADDR, PORT);
    do_epoll(srvfd);
    return 0;
}

static int create_server_proc(const char *ip, int port)
{
    int fd;
    struct sockaddr_in seraddr;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        //perror("socket error!\n");
        printf("socket error errno = %d(%s)\n", errno, strerror(errno));
        return -1;
    }
    int reuse = 1;
    /*一个端口释放后会等待两分钟之后才能再被使用，SO_REUSEADDR是让端口释放后立即就可以被再次使用。*/
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
    {
        return -1;
    }
    memset(&seraddr, 0, sizeof(seraddr));
    seraddr.sin_family = AF_INET;
    //seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    inet_pton(AF_INET, ip, &seraddr.sin_addr);
    seraddr.sin_port = htons(port);
    if (bind(fd, (struct sockaddr *)&seraddr, sizeof(seraddr)) < 0)
    {
        perror("bind error!\n");
        return -1;
    }
    listen(fd, LISTEN);

    printf("本地接收地址 = %s:%d\n", "127.0.0.1", port);
    return fd;
}
static int accept_client_proc(int epollfd, int listenfd, int epoll_type, int block_type)
{
    struct sockaddr_in cliaddr;
    socklen_t cliaddrlen;
    cliaddrlen = sizeof(cliaddr);
    int clifd = -1;

    clifd = accept(listenfd, (struct sockaddr *)&cliaddr, &cliaddrlen);
    if (clifd == -1)
    {
        printf("accept error errno = %d(%s)\n", errno, strerror(errno));
        return -1;
    }
    printf("accept a new client(%d) :%s:%d\n", clifd, inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);
    //添加一个客户描述符和事件
    add_event(epollfd, clifd, epoll_type, block_type, EPOLLIN);
    clifd_group[clifd] = '1';
}
static void add_event(int epollfd, int fd, int epoll_type, int block_type, int state)
{
    struct epoll_event ev;
    ev.events = state; //EPOLLOUT or EPOLLIN or ...
    ev.data.fd = fd;

    /* 如果是ET模式，设置EPOLLET */
    if (epoll_type == EPOLL_ET)
        ev.events |= EPOLLET;

    /* 设置是否阻塞 */
    if (block_type == FD_NONBLOCK)
        set_nonblock(fd);
    /*epoll的事件注册函数,不同与select()在监听事件时告知内核监听事件的类型(R/W/E),而是先注册要监听的事件类型,*/
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
}
static void del_event(int epollfd, int fd, int state)
{
    struct epoll_event ev;
    ev.events = state;
    ev.data.fd = fd;
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &ev);
}
static void modify_event(int epollfd, int fd, int state)
{
    struct epoll_event ev;
    ev.events = state;
    ev.data.fd = fd;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &ev);
}

static int do_epoll(int listenfd)
{
    int epollfd;
    struct epoll_event events[EPOLLEVENTS]; //事件表
    int nfds;
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    /*创建一个epoll描述符*/
    epollfd = epoll_create(FDSIZE); //FDSIZE无意思
    /*添加listenfd事件 非阻塞LT模式 */
    add_event(epollfd, listenfd, EPOLL_LT, FD_NONBLOCK, EPOLLIN);
    /*添加listenfd事件 非阻塞ET模式 高并发下处出现客户端连接不上 */
    //add_event(epollfd,listenfd,EPOLL_LT,FD_NONBLOCK,EPOLLIN);
    while (1)
    {
        /*等待事件的产生,类似select,events用来从内核得到事件的集合；返回需要处理的事件数目*/
        nfds = epoll_wait(epollfd, events, EPOLLEVENTS, -1);
        if (nfds < 0)
        {
            printf("epoll_wait error errno = %d(%s)", errno, strerror(errno));
            return -1;
        }
        /* connfd:非阻塞的LT模式 */
        handle_client_proc(epollfd, events, nfds, listenfd, buf, EPOLL_LT, FD_NONBLOCK);
    }
    close(epollfd);
}
static void handle_client_proc(int epollfd, struct epoll_event *events, int num,
                               int listenfd, char *buf, int epoll_type, int block_type)
{
    int i = 0;
    int fd;
    /*检测事件表哪个事件的那种行为发生变化,*/
    for (i = 0; i < num; i++)
    {
        fd = events[i].data.fd;
        /*有new connection ,并把新的连接加入监听队列*/
        if ((fd == listenfd) && (events[i].events & EPOLLIN))
            /* connfd:非阻塞的LT模式 */
            accept_client_proc(epollfd, listenfd, epoll_type, block_type);
        /*接收数据,读*/
        else if (events[i].events & EPOLLIN)
        {
            if (epoll_type == EPOLL_LT)
            {
                printf("EPOLL_LT read event\n");
                recv_client_msg_LT(epollfd, fd, buf);
            }
            else if (epoll_type == EPOLL_ET)
            {
                printf("EPOLL_ET read event\n");
                recv_client_msg_ET(epollfd, fd, buf);
            }
        }
        else if (events[i].events & EPOLLOUT)
        {
            if (epoll_type == EPOLL_LT)
            {
                printf("EPOLL_LT write event\n");
                write_client_msg_LT(epollfd, fd, buf);
            }
            else if (epoll_type == EPOLL_ET)
            {
                printf("EPOLL_ET write event\n");
                write_client_msg_LT(epollfd, fd, buf);
            }
        }
    }
}
static void write_client_msg_LT(int epollfd, int fd, char *buf)
{
    /*  write事件，水平模式下，socketfd 未设置阻塞，就会不断触发write，所有
     *  write 事件是不能一直监听的，需要写的时候才监听，写一次，然后改变成监听读
     */
    //assert(buf); //假设有buf,若条件为假,终止
    int nwrite;

    //char sendbuf[MAXBUFF]={'1','2','3','1','2','3','1','2','3','1','2','3','\0'};
    //nwrite = write(fd,sendbuf,strlen(sendbuf));

    char *sendbuf;
    sendbuf = malloc(MAXBUFF * sizeof(char));
    memset(sendbuf, 0, MAXBUFF);
    strncpy(sendbuf, buf, sizeof(buf));
    int i=0;
    for(i=0;i<FDSIZE;i++)
    {
        if(clifd_group[i] == '1')
        {
            int clifd = i;
            if(i==fd) //对于fd 不可一直监听EPOLLOUT，否则会一直写
            {
                 modify_event(epollfd,fd, EPOLLIN); 
                 continue;
            }
            nwrite = write(clifd, sendbuf, sizeof(sendbuf)); //回调 出现问题 不能超过大于8bytes
            if (nwrite == -1)
            {
                perror("write error!\n");
                close(clifd);
                del_event(epollfd, clifd, EPOLLOUT);  
                clifd_group[clifd] = '0';
            }
            else
                modify_event(epollfd, clifd, EPOLLIN);
            printf("write to client(%d) %dbytes:%s\n", clifd, nwrite, sendbuf);
        }
        
    }
    printf("write over！！！！\n");
    memset(buf, 0, MAXBUFF);
    
}
/*采用LT触发时，只要还有数据留在buffer中（一次未读完）,server就会继续得到通知,不断触发读*/
static void recv_client_msg_LT(int epollfd, int fd, char *buf)
{
    int nread;
    nread = read(fd, buf, MAXBUFF); //recv
    if (nread == -1)
    {
        perror("read error!\n");
        close(fd);
        del_event(epollfd, fd, EPOLLIN);
        clifd_group[fd] = '0';
    }
    else if (nread == 0)
    {
        //fprintf(stderr,"client close\n");
        printf("%d client close\n", fd);
        close(fd);
        del_event(epollfd, fd, EPOLLIN);
        clifd_group[fd] = '0';
    }
    else
    {
        printf("read from client(%d) %dbytes msg:%s\n", fd, nread, buf);
        /*修改描述符对应的事件，由读改为写--->写事件就会触发*/
        modify_event(epollfd, fd, EPOLLOUT);
    }
    printf("read over！！！\n");
}
static void write_client_msg_ET(int epollfd, int fd, char *buf)
{
    // assert(buf);//假设有buf,若条件为假,终止
    int nwrite;
    //nwrite = write(fd,buf,sizeof(buf));//回调
    nwrite = write(fd, buf, strlen(buf)); //send

    if (nwrite == -1)
    {
        perror("write error!\n");
        close(fd);
        del_event(epollfd, fd, EPOLLOUT);
    }
    else
        modify_event(epollfd, fd, EPOLLIN);

    printf(" write to client(%d) %dbytes:%s", fd, nwrite, buf);
    printf(" write over！！！！\n");
    memset(buf, 0, MAXBUFF);
}
/*采用ET触发时，只是触发一次读 所以触发一次后需要不断读取所有数据直到读完EAGAIN为止。
 *否则剩下的数据只有在下次对端有写入时才能一起取出来了。 while(1){read();}*/
static void recv_client_msg_ET(int epollfd, int fd, char *buf)
{
    int nread;
    nread = read(fd, buf, MAXBUFF);
    if (nread == -1)
    {
        perror("read error!\n");
        close(fd);
        del_event(epollfd, fd, EPOLLIN);
    }
    else if (nread == 0)
    {
        //fprintf(stderr,"client close\n");
        printf("%d client close\n", fd);
        close(fd);
        del_event(epollfd, fd, EPOLLIN);
    }
    else
    {
        printf("%d read %dbytes msg:%s\n", fd, nread, buf);
        /*修改描述符对应的事件，由读改为写*/
        modify_event(epollfd, fd, EPOLLOUT);
    }
    printf(" 读处理结束！！！\n");
}
int set_nonblock(int fd)
{
    int old_flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, old_flags | O_NONBLOCK);
    return old_flags;
}
/*EPOLL_OUT事件存在的必要性：https://www.zhihu.com/question/22840801/answer/22814514
 *一般情况下，在处理EPOLL_IN时，在recv后，可以send发送data。
 *但是当文件较大时，为10G文件时，send返回值只能返回256K，写完缓冲区；接着调用send，就会返回EAGAIN，
 *表明缓冲区已满，无法继续send发送。
 *当采用监听EPOLLOUT事件时，只要socket缓冲区中的数据被对方接收之后，缓冲区就会有空闲空间可以继续接收数据，
 *此时epoll_wait就会返回这个socket的EPOLLOUT事件，获得这个事件时，你就可以继续往socket中写出数据.
 */