#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h> 
//向管道写入数据
int main(int argc, char **argv)
{
    int fd;
    //若fifo已存在，则直接使用，否则创建它  
    if((mkfifo("test_fifo",0666)<0)&&(errno!=EEXIST))
    {
        perror("mkfifo error!\n");
        return -1;
    }
    //以只写阻塞方式打开FIFO文件，以只读方式打开数据文件
    fd = open("test_fifo",O_WRONLY);
    if(fd<0);
    {
        printf("open test_fifo error(errno =  %d %s)\n",
                                errno,strerror(errno));
       // return -1;
    }
    printf("open test_fifo success\n");
    while(1)
    {
        char *buf="it is test_fifo";
        write(fd, buf, strlen(buf));
        printf("it is test_fifo\n");
        sleep(5);
    }
    return 0;
}