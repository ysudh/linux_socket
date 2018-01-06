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
//int select(int nfds,fd_set *readfds,fd_set *writefds,
//           fd_set *exceptfds,struct timeval *timeout);
#define MAXBUFF 1024
#define IPADDR "127.0.0.1"
#define PORT 8787
#define LISTEN 5
#define SIZE 20
typedef struct server_set
{
    int cli_cnt; //存储描述符数量 小与SIZE
    int clifds[SIZE];//存储描述符
    fd_set allfds; //句柄集合
    int maxfd; //记录最大的fd 
}server_context_st;
static server_context_st *ser = NULL;
//static函数的作用域是本源文件，可以它想象为面向对象中的private函数就可以了
//建立监听的
static int create_server_proc(const char *ip,int port);
//链接一个client
static int accept_client_proc(int serfd);
//
static int handle_client_msg(int fd,char *buf);
static int recv_client_msg(fd_set *readfds);
//处理接入的client
static int handle_client_proc(int serfd);
static int server_init();
static void server_uninit();
int main(int argc,char **argv)
{   
    int fd;
    server_init();
    
    fd = create_server_proc(IPADDR,PORT);

    handle_client_proc(fd);

    server_uninit();

    return 0;
}
static int server_init()
{
    ser = (server_context_st *)malloc(sizeof(server_context_st));//分配内存
    if(ser == NULL)
    {
        return -1;
    } 
    memset(ser,0,sizeof(ser));//初始化
    int i=0;
    for(i =0;i<SIZE;i++)
        ser->clifds[i] = -1;
    return 0;
}
static void server_uninit()
{
    if(ser)
    {
        free(ser);
        ser = NULL;
    }
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
static int accept_client_proc(int serfd)
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
    //将该client加入clifds中
    int k = 0;
    for(k = 0;k<SIZE;k++)
    {
        if(ser->clifds[k]<0)
        {
            ser->clifds[k] = clifd;
            ser->cli_cnt ++;
            break;
        }
    }
    if(k>=SIZE)
    {
        printf("too much client\n");
        return -1;
    }
}
static int handle_client_proc(int serfd)
{
      fd_set *readfds = &(ser->allfds);
      struct timeval tv;
      int i = 0;
      int clifd;
      while(1)
      {
       //每次调用select前都要重新设置文件描述符和时间，因为事件发生后，
       //文件描述符和时间都被内核修改啦
          FD_ZERO(readfds);
          FD_SET(serfd,readfds);//加入句柄
          ser->maxfd = serfd;

          tv.tv_usec = 0;
          tv.tv_sec = 5;

          for(i = 0;i<ser->cli_cnt;i++)
          {
              clifd = ser->clifds[i];
              //去除无效的client句柄
              if(clifd != -1)
              {
                  FD_SET(clifd,readfds);//加入句柄
              }
              ser->maxfd = ser->maxfd > clifd ? ser->maxfd:clifd;
          }
          int ret ;
          //阻塞,等待readfds变化
          //返回时告诉我们哪些描述符已经准备好了，是读，写，还是异常
          //select(ser->maxfd + 1,readfds, NULL, NULL, &tv);
          //ser->maxfd + 1(最大监听数+1)
          select(ser->maxfd + 1,readfds, NULL, NULL, NULL);
          if(ret<0)
          {
              printf("select error errno = %d(%s)\n",errno,strerror(errno));
              return -1;
          }
          else if(ret == 0)
          {
              printf("Time out!\n");
              return -1;
          }
          else if(ret>0)
          {
              //至于FD_ISSET，则是检测哪一个描述符准备好了，描述符集中可能会有很
              //多描述符，而不只是一个,没准备好的,返回时置0

              /*if else 的逻辑*/

              if(FD_ISSET(serfd,readfds)) //监听到serfd
              {
                  //printf("accept_client_proc!\n");
                  //当客户端运行connect的时候,服务端将执行accept
                  accept_client_proc(serfd);//接收,new加入的
                  //recv_client_msg(readfds);//处理,
              }
              else
              {
                  //printf("recv_client_msg!\n");
                  //对应客户端发送数据时候,
                  recv_client_msg(readfds);//处理,
              }
          }

      }
}
static int handle_client_msg(int fd,char *buf)
{
    assert(buf);//假设有buf,若条件为假,终止
    printf("recv the fd = %d(%s)\n",fd,buf);
    write(fd,buf,sizeof(buf));//回调
}

static int recv_client_msg(fd_set *readfds)
{
    char buf[MAXBUFF];
    int clifd;
    int i;
    for(i =0;i<ser->cli_cnt;i++)//循环检查是哪个clifds
    {
        clifd = ser->clifds[i];
        if(clifd<0)
            continue;
        if(FD_ISSET(clifd,readfds))//检测是那个客户的消息
        {
            int n;
            n = read(clifd,buf,sizeof(buf));
            if(n<=0)//读取数据结束
            {
                FD_CLR(clifd,readfds);
                close(clifd);
                ser->clifds[i] = -1;
                continue;
            }
        }
        handle_client_msg(clifd,buf);
    }
}