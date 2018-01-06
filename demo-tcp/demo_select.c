#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h> 
#include <fcntl.h>
/*select，poll，epoll本质上都是同步I/O，因为他们都需要在读写事件就绪后自己负责进行读写，
也就是说这个读写过程是阻塞的，而异步I/O则无需自己负责进行读写，异步I/O的实现会负责把数据从内核拷贝到用户空间*/
/*
int select(int nfds,fd_set *readfds,fd_set *writefds,
           fd_set *exceptfds,struct timeval *timeout);
监听并等待多个文件描述符的属性变化(可读,可写或是错误异常),调用select后会阻塞,
直到描述符就位(出现可读,可写或是错误异常),或是超时(timeout),函数才返回,
当函数返回后,可以遍历fdset,就找到就绪的描述符.
nfds 要描述的文件描述符的范围,Linux(最大值1024)
fd_set 描述符集合,不需要的要NULL
void FD_CLR(int fd,fd_set *set);//从集合之中将删除一个给定的文件描述符
int  FD_ISSET(int fd,fd_set *set);//判断一个给定的文件描述符是否加入集合之
void FD_SET(int fd,fd_set *set);//将一个给定的文件描述符加入集合之中
void FD_ZERO(fd_set *set); //清空集合
*/
/*
   struct timeval {
               long    tv_sec;         /* seconds 
               long    tv_usec;        /* microseconds 
           };
       and
   struct timespec {
               long    tv_sec;         /* seconds 
               long    tv_nsec;        /* nanoseconds 
           };

*/
int main(int agrc,char **argv)
{
    struct timeval tv;
    fd_set rfds; //句柄集合

    int fd;
     //以阻塞型只读方式打开fifo  
    if((fd=open("test_fifo",O_RDONLY))<0);
    {
        printf("open test_fifo error(errno =  %d %s)\n",errno,strerror(errno));
       //return -1;
    }
    //IO多路
    while(1)
    {
        /*每次调用select前都要重新设置文件描述符和时间，因为事件发生后，文件描述符和时间都被内核修改啦*/
        //加入rdfs
        FD_ZERO(&rfds);
        FD_SET(0,&rfds);
        FD_SET(fd,&rfds);

        tv.tv_sec = 1;
        tv.tv_usec = 0;//ms

        int ret;
        // 监视并等待多个文件（标准输入，有名管道）描述符的属性变化（是否可读）  
        // 没有属性变化，这个函数会阻塞，直到有变化才往下执行，这里没有设置超时 
        ret = select(FD_SETSIZE,&rfds,NULL,NULL,NULL);
       // ret = select(FD_SETSIZE,&rfds,NULL,NULL,tv);
        if(ret == -1)
        {
            perror("select error\n"); 
            return -1;           
        }
        else if(ret == 0)
        {
            printf("time out!\n");
            return -1;
        }
        else if(ret>0)
        {
            char buf[32] = {0};
            //0 1 2 stdin stdout stderr
            if(FD_ISSET(0,&rfds)) //标准输入
            {
                read(0,buf,sizeof(buf));
                printf("stdin buf = %s\n",buf);
            }
            if(FD_ISSET(fd,&rfds)) //有名管道
            {
                read(fd,buf,sizeof(buf));
                printf("fifo buf = %s\n",buf);
            }
        }

    }
    return 0;
}