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

#define MAXBUFF 1024
int socket_ser(int fd);
void sig_pid(int signo); 
/*
信号机制--signal():为指定的信号安装一个新的信号处理函数
#include <signal.h>
typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler);
signum:要处理的信号类型(many...SIGINT:由中断产生,)
handler:描述了与信号相关的操作(SIG_IGN,SIG_DFL)
为指定函数地址时(signal handler/signal-catching functing),当信号发生时调用该函数()
返回值:之前的信号处理函数指针,出现错误返回SIG_ERR(-1)
*/
//二级指针与指针数组(test)
//指针和二维数组
//sockt-->bind-->listen-->accept-->close
int main(int argc,char **argv)
{
    int socket_fd,confd;
    struct sockaddr_in local_addr,cli_addr;
    pid_t childpid;
    int port;
    const int listen_max = 64;//同时监听的client最大数
    if(argc != 2)
    {
        printf("please set port(such as ./server 8888)\n");
        return -1;
    }
    port = atoi(argv[1]);
    if((socket_fd=socket(AF_INET,SOCK_STREAM,0))<0)
    {
        perror("socket error !\n");
        exit(EXIT_FAILURE);
    }
    printf("socket_fd = %d\n",socket_fd);
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(port);
    /* Enable address reuse*/
    //SO_REUSEADDR参数用来解决当 前一次服务器断开连接产出套接字状态TIME_WAIT,该状态保持2-4分钟
    //当TIME_WAIT状态退出后,套接字被删除,地址才可以重新绑定.  实现避免TIME_WAIT状态.
    int on = 1;//允许地址重用
    int rs = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );
    
    int ret = bind(socket_fd,(struct sockaddr*)&local_addr,sizeof(local_addr));
    if(ret<0)
    {
        perror("bind error");
        exit(-1);
    }  
   //getsockname函数用于获取与某个套接字关联的本地协议地址 
    else
    {
        int len = sizeof(local_addr);
        if (getsockname(socket_fd, (struct sockaddr *) &local_addr, &len) < 0)
		{
			printf("ERROR: 分配端口失败\n");
		}
		else
		{
			port = ntohs(local_addr.sin_port);
		}
    }
    printf("本地接收地址 = %s:%d\n", "127.0.0.1", port);
    if(listen(socket_fd,listen_max)<0)
    {
        perror("listen error");
        exit(-1);
    }
    //进程Terminate或Stop的时候，SIGCHLD会发送给它的父进程.子进程结束
    signal(SIGCHLD, &sig_pid);
    while(1)
    {
        int len = sizeof(cli_addr);
        if((confd = accept(socket_fd, (struct sockaddr *)&local_addr,&len))<0)
        {
            perror("accept error!");
            continue;
        }
        printf("client_fd = %d\n",confd);
       // socket_ser(confd);
        if((childpid = fork())<0)//建立子进程 用于client 
        {
             perror("fork");
             exit(EXIT_FAILURE);
        }
        else if(childpid == 0)
        {
            //close(socket_fd);
            socket_ser(confd);   //子进程处理接收到的confd
            exit(0);
        }
        close(confd);
    }
    close(socket_fd);
    return 0;
}
void sig_pid(int signo)
{
    pid_t pid;
    int stat;
    /*pid_t waitpid(pid_t pid, int * status, int options); 中断(结束)进程函数(或是等待子进程中断)
      pid:<-1  =-1(等待任何子进程)  0  >0
    */
    while((pid = waitpid(-1, &stat, WNOHANG))<0)
        printf("waitpid error!\n");
}
int socket_ser(int fd)
{
    ssize_t n;//ssize_t unsigned (long/int)
    char buf[MAXBUFF];
    while(1)
    {
        if((n = read(fd, buf, sizeof(buf)))>0)
        {
            printf("read buf %s\n",buf);
            if(write(fd, buf, n)<0)
            {
                printf("write error!\n");
                return -1;
            }
            else
            {
                printf("write buf %s\n",buf);
            }
        }
    }   
    return 0;
}