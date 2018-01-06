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

/*
创建epoll句柄,size用于告知内核该句柄监听的数目,该句柄占用一个fd值(/proc/进程id/fd)
所以在完成epoll后,要关闭close(),否则导致fd被耗尽.
int epoll_create(int size);
epoll的事件注册函数,不同与select()在监听事件时告知内核监听事件的类型(R/W/E),而是先注册要监听的事件类型,
int epoll_ctl(int epfd,int op,int fd ,struct epoll_event *event);
struct epoll_event {
  __uint32_t events;  /* Epoll events 
  epoll_data_t data;  /* User data variable
};
 typedef union epoll_data
 {
     void* ptr;              //指定与fd相关的用户数据 
     int fd;                 //指定事件所从属的目标文件描述符 
     uint32_t u32;
     uint64_t u64;
 } epoll_data_t;
等待事件的产生,类似select,events用来从内核得到事件的集合 返回需要处理的事件数目
int epoll_wait(int epfd,struct epoll_event *events,int maxevent,int timeout);
*/
#define MAXBUFF 1024
#define IPADDR "127.0.0.1"
#define PORT 8787
#define EPOLLEVENTS 100
#define LISTEN 5
#define FDSIZE 100
#define SIZE 20

/*
select，poll实现需要自己不断轮询所有fd集合，直到设备就绪，期间可能要睡眠和唤醒多次交替。
而epoll其实也需要调用epoll_wait不断轮询就绪链表，期间也可能多次睡眠和唤醒交替，但是它是设备就绪时，
调用回调函数，把就绪fd放入就绪链表中，并唤醒在epoll_wait中进入睡眠的进程。虽然都要睡眠和交替，
但是select和poll在“醒着”的时候要遍历整个fd集合，而epoll在“醒着”的时候只要判断一下就绪链表是否为空就行了，
这节省了大量的CPU时间。这就是回调机制带来的性能提升。
*/
//static函数的作用域是本源文件，可以它想象为面向对象中的private函数就可以了
//建立监听的
static int create_server_proc(const char *ip,int port);
//链接一个client
static int accept_client_proc(int epollfd, int serfd);
/*接收 发送 IO处理*/
static void write_client_msg(int epollfd, int fd,char *buf);
static void recv_client_msg(int epollfd, int fd,char *buf);
/*处理接入的client*/
static void handle_client_proc(int epollfd, struct epoll_event * events, int num, int listenfd);

static int do_epoll(int fd);

/*epoll的事件注册,*/
static void add_event(int epollfd,int fd,int state);
/*删除事件,*/
static void del_event(int epollfd,int fd,int state);
/*改变事件,*/
static void modify_event(int epollfd,int fd,int state);
int main(int argc,char **argv)
{   
    int srvfd;
    srvfd = create_server_proc(IPADDR,PORT);
    do_epoll(srvfd);
    return 0;
}

static int create_server_proc(const char *ip,int port)
{
    int fd;
    struct sockaddr_in seraddr;
    fd = socket(AF_INET,SOCK_STREAM,0);
    if(fd<0)
    {
        //perror("socket error!\n");
        printf("socket error errno = %d(%s)\n",errno,strerror(errno));
        return -1;
    }
    int reuse = 1;
    /*一个端口释放后会等待两分钟之后才能再被使用，SO_REUSEADDR是让端口释放后立即就可以被再次使用。*/
    if(setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse)) == -1)
    {
        return -1;
    }
    memset(&seraddr, 0, sizeof(seraddr));
    seraddr.sin_family = AF_INET;
   //seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    inet_pton(AF_INET,ip,&seraddr.sin_addr);
    seraddr.sin_port = htons(port);
    if(bind(fd,(struct sockaddr *) &seraddr,sizeof(seraddr))<0)
    {
        perror("bind error!\n");
        return -1;
    }
    listen(fd,LISTEN);
    //此处的port不为8787 ????
    printf("本地接收地址 = %s:%d\n", "127.0.0.1", port);//此处的port为
    return fd;
}
static int accept_client_proc(int epollfd, int serfd)
{
    struct sockaddr_in cliaddr;
    socklen_t cliaddrlen;
    cliaddrlen = sizeof(cliaddr);
    int clifd = -1;
   
    clifd = accept(serfd, (struct sockaddr *) &cliaddr,&cliaddrlen);
    if(clifd == -1)
    {
        printf("accept error errno = %d(%s)\n",errno,strerror(errno));
        return -1;
    }
    printf("accept a new client :%s:%d\n",inet_ntoa(cliaddr.sin_addr),cliaddr.sin_port);
  
    //添加一个客户描述符和事件
    add_event(epollfd,clifd,EPOLLIN);
    }
}
static void add_event(int epollfd,int fd,int state)
{
    struct epoll_event ev;
    ev.events = state; //EPOLLOUT or EPOLLIN or ...
    ev.data.fd = fd;
    /*注册监听事件*/
    /*
    fd:要操作的描述符
    EPOLL_CTL_ADD:往事件表上注册fd上的事件
    EPOLL_CTL_DEL:删除
    EPOLL_CTL_MOD:修改
    */
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,ev);
}
static void del_event(int epollfd,int fd,int state)
{
    struct epoll_event ev;
    ev.events = state;
    ev.data.fd = fd;
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,&ev);
}
static void modify_event(int epollfd,int fd,int state)
{
    struct epoll_event ev;
    ev.events = state;
    ev.data.fd = fd;
    epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&ev);
}

static int do_epoll(int fd)
{
    int epollfd;
    struct epoll_event events[EPOLLEVENTS];//事件表
    int ret;
    char buf[MAXBUFF] = {0};
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    /*创建一个epoll描述符*/
    epollfd = epoll_create(FDSIZE);//FDSIZE无意思
    /*添加监听描述符事件*/
    add_event(epollfd,fd,EPOLLIN);
    while(1)
    {
        /*最多监听多少个事件:EPOLLEVENTS 
        当timeout为-1是，epoll_wait调用将永远阻塞，直到某个时间发生
        events：检测到事件，将所有就绪的事件从内核事件表中复制到它的第二个参数events指向的数组中,
        events里面将储存所有的读写事件*/
        ret = epoll_wait(epollfd,events,EPOLLEVENTS,-1);
        if(ret<0)
        {
            printf("epoll_wait error errno = %d(%s)",errno,strerror(errno));
            return -1;
        }
        handle_client_proc(epollfd,events,ret,fd,buf);
    }
    close(epollfd);
}
static void handle_client_proc(int epollfd, struct epoll_event * events, int num, int listenfd,char *buf)
{
    int i = 0;
    int fd;
    /*检测事件表哪个事件的那种行为发生变化,*/
    for(i =0;i<num;i++)
    {
        fd = events[i].data.fd;
        /*有new connection ,并把新的连接加入监听队列*/
        if((fd==listenfd) && (events[i].events & EPOLLIN)) 
            accept_client_proc(epollfd,listenfd);  //对应client connection
        /*接收数据,读*/
        else if(event[i].events & EPOLLIN)
            recv_client_msg(epollfd,listenfd,buf);
        else if(event[i].events & EPOLLOUT)
            write_client_msg(epollfd,listenfd,buf);
            
    }
}
static int write_client_msg(int epollfd,int fd,char *buf)
{
   // assert(buf);//假设有buf,若条件为假,终止
    int nwrite;
    nwrite = write(fd,buf,sizeof(buf));//回调
    if(nwrite == -1)
    {
        perror("write error!\n");
        close(fd);
        del_event(epollfd,fd,EPOLLOUT);  
    }
    else 
        modify_event(epollfd,fd,EPOLLIN);
    memset(buf,0,MAXBUFF);
}

static int recv_client_msg(int epollfd,int fd,char *buf)
{
    int nread;
    nread = read(fd,buf,MAXBUFF);
    if(nread == -1)
    {
        perror("read error!\n");
        close(fd);
        del_event(epollfd,fd,EPOLLIN);
    }
    else if(nread == 0)
    {
        fprintf(stderr,"client close\n");
        close(fd);
        del_event(epollfd,fd,EPOLLIN);
    }
    else
    {
        printf("read msg:%s\n",buf);
        /*修改描述符对应的事件，由读改为写*/
        modify_event(epollfd,fd,EPOLLOUT);
    }
}
