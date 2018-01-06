#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <net/if.h> 
#include <errno.h>

#define MAXBUFF 1024
int socket_cli(int fd);
/*
socket-->配置(sockaddr_in)-->connect()--->close()
*/
int main(int argc,char *argv[])
{
    int socket_fd;
    int port; 
    char ip[20]={0};
    struct sockaddr_in ser_addr;
    if(argc !=3)
    {
        printf("please enter a ip(such as ./client 127.0.0.1 8888) to connect\n");
        return -1;
    }
    strncpy(ip, argv[1], sizeof(argv[1]));
    port = atoi(argv[2]);

    if((socket_fd=socket(AF_INET, SOCK_STREAM, 0))<0)
    {
        printf("build socket error\n");
        return -1;
    }
    printf("socket_fd = %d\n",socket_fd);
    memset(&ser_addr, 0, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_port = htons(port);
    //ser_addr.sin_addr.s_addr = inet_addr(ip);
    if (inet_pton(AF_INET, argv[1], &ser_addr.sin_addr) < 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    if(connect(socket_fd, (struct sockaddr *)&ser_addr, sizeof(ser_addr))<0)//
    {
        printf("connect error!,errno = %d\n",errno);
        //printf("connect error!\n");
        return -1;
    }
    printf("connect success!\n");
    //while(1)
    int i;
    for(i=0;i<3;i++)
    {
       socket_cli(socket_fd); //pthread
       sleep(5);
    }
    //socket_cli(socket_fd); //pthread
    close(socket_fd);
    return 0;
}
int socket_cli(int fd)
{
   char sendBuf[MAXBUFF] = "hello";
   char recvBuf[MAXBUFF];
   printf("I am %d\n",fd);
   if(write(fd, sendBuf, sizeof(sendBuf))<0)
   {
       printf("write error!\n");
       return -1;
   }

   if(read(fd,recvBuf,sizeof(recvBuf))<0)
   {
        printf("read error!\n");
        return -1;
   }
   else
    {
            printf("read buf - %s\n",recvBuf);
    }
  
   return 0;
}