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
#include <poll.h>
/*
int poll(struct pollfd *fds,nfds_t nfds,int timeout)
监控并等待多个文件描述符的变化,
struct pollfd{
    int fd;
    short events;
    short revents;
}
每个结构体的 events 域是由用户来设置，告诉内核我们关注的是什么，
而 revents 域是返回时内核设置的，以说明对该描述符发生了什么事件。
*/
int main(int argc,char *argv[])
{
    struct pollfd fds[2];
    int pfd;
    if((mkfifo("test_fifo",0666)<0)&&(errno != EEXIST))
    {
        perror("mkfifo error!\n");
        return -1;
    }
    pfd = open("test_fifo",O_RDONLY);
    if(pfd<0)
    {
        perror("open test_fifo error\n");
        return -1;
    }
    fds[0].fd = 0;
    fds[1].fd = pfd;
    fds[0].events = POLLIN;
    fds[1].events = POLLIN;
    while(1) //循环判断
    {
        int ret;
        // 监视并等待多个文件（标准输入，有名管道）描述符的属性变化（是否可读）  
        // 没有属性变化，这个函数会阻塞，直到有变化才往下执行，这里没有设置超时  
        ret = poll(fds,2,-1);
        if(ret<0)
        {
            printf("poll error errno = %d(%s)\n",errno,strerror(errno));
            return -1;
        }
        else if(ret>0) //// 准备就绪的文件描述符 
        {
            char buf[20]={0};//如buf小与发送buf?也可以接收,
            if((fds[0].revents & POLLIN) == POLLIN)
            {
                read(fds[0].fd,buf,sizeof(buf));
                printf("stdio buf = %s\n",buf);
            }
            else if((fds[1].revents & POLLIN) == POLLIN)
            {
                read(fds[1].fd,buf,sizeof(buf));
                printf("fifo buf = %s\n",buf);
            }
            else if(ret == 0)
            {
                printf("Time out!\n");
                return -1;
            }
                
        }
    }
    return 0;
}