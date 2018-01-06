#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h> 
#include <sys/epoll.h>
/*在 select/poll中，进程只有在调用一定的方法后，内核才对所有监视的文件描述符进行扫描，
而epoll事先通过epoll_ctl()来注册一个文件描述符，一旦基于某个文件描述符就绪时，
内核会采用类似callback的回调机制，迅速激活这个文件描述符，当进程调用epoll_wait() 时便得到通知。
(此处去掉了遍历文件描述符，而是通过监听回调的的机制。这正是epoll的魅力所在。)*/

/*
int epoll_create(int size);//该函数生成一个 epoll 专用的文件描述符（创建一个 epoll 的句柄）
//epoll 的事件注册函数，它不同于 select() 是在监听事件时告诉内核要监听什么类型的事件，而是在这里先注册要监听的事件类型
int epoll_ctl(int epfd,int op,int fd,struct epoll_event *event);
// 保存触发事件的某个文件描述符相关的数据（与具体使用方式有关）  
    typedef union epoll_data {  
        void *ptr;  
        int fd;  
        __uint32_t u32;  
        __uint64_t u64;  
    } epoll_data_t;  
 // 感兴趣的事件和被触发的事件  
    struct epoll_event {  
        __uint32_t events; /* Epoll events 
        epoll_data_t data; /* User data variable   
    }; 
等待事件的产生，收集在 epoll 监控的事件中已经发送的事件，类似于 select() 调用
int epoll_wait(int epfd,struct epoll_event * events,int maxevents,int timeout);
opoll对文件描述符的操作两种方式:LT（level trigger）默认模式和 ET（edge trigger）
*/
int main(int argc,char **argv)
{
    int fd;
    if((mkfifo("test_fifo",0666)<0)&&(errno != EEXIST))
    {
        perror("mkfifo error!\n");
        return -1;
    }
    fd = open("test_fifo",O_RDONLY);
    if(fd<0)
    {
        perror("open test_fifo error\n");
        return -1;
    }
    struct epoll_event event; //告知kernel监听的事件
    struct epoll_event wait_event; 
    //创建一个 epoll 的句柄，参数要大于 0，没有太大意义  
    int epfd = epoll_create(10);
    if(epfd == -1)
    {
        perror("epoll_create error!\n");
        return -1;
    }

    event.data.fd = 0; //标准输入
    event.events = EPOLLIN;//可读
    int ret = 0;
    //事件的注册函数,将标准的输入描述符加入监听事件
    //添加EPOLL_CTL_ADD，删除EPOLL_CTL_DEL，修改EPOLL_CTL_MOD
    ret = epoll_ctl(epfd,EPOLL_CTL_ADD,0,&event);
    if(ret == -1)
    {
        perror("epoll_ctl(0) error!\n");
        return -1;
    }

    event.data.fd = fd;
    event.events = EPOLLIN;
    int ret1 = 0;
    ret1 = epoll_ctl(epfd,EPOLL_CTL_ADD,fd,&event);
    if(ret1 == -1)
    {
        perror("epoll_ctl(fd) error!\n");
        return -1;
    }
    int ret2 = 0;
    while(1)
    {
        // 监视并等待多个文件（标准输入，有名管道）描述符的属性变化（是否可读）  
        // 没有属性变化，这个函数会阻塞，直到有变化才往下执行，这里没有设置超时 
        ret2 = epoll_wait(epfd,&wait_event,2,-1);
        if(ret2 == -1)
        {
            perror("epoll_wait error!\n");
            return -1;
        }
        else if(ret2>0) //
        {
            char buf[20] = {0};
            //标准输入就绪
            if((wait_event.data.fd == 0)&&(wait_event.events == EPOLLIN))
            {
                read(0,buf,sizeof(buf));
                printf("stdin buf = %s\n",buf);
            }
            else if((wait_event.data.fd == fd)&&(wait_event.events = EPOLLIN))
            {
                read(fd,buf,sizeof(buf));
                printf("fifo buf = %s\n",buf);
            }
        }
        else if(ret2 == 0)
        {
            printf("Time out!\n");
            return -1;
        }
    }
    return 0;
}
