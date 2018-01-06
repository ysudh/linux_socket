#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<string.h>
#include<netdb.h>
#include<errno.h>
#include<arpa/inet.h>

#define MCAST_PORT 8888
#define MCAST_ADDR "224.0.0.100"
#define MCAST_DATA "TEXT DATA"
#define MCAST_INITERVAL 5

int main(int argc ,char *argv[])
{
	int s;
	struct sockaddr_in mcast_addr;//sockaddr_in
	s = socket(AF_INET,SOCK_DGRAM,0);
	if(s == -1)
	{
		perror("socket()");
		return -1;
	}
	memset(&mcast_addr,0,sizeof(mcask_addr));
	mcast_addr.sin_family = AF_INET; 
	mcast_addr.sin_addr.s_addr = inet_addr(MCAST_ADDR);
	mcast_addr.sin_port = htons(MCAST_ADDR); 

	while(1)
	{
		int n = sendto(s,
			MCAST_DATA,
			sizeof(MCAST_DATA),
			0,
			(struct sockadd*) &mcast_addr,
			sizeof(mcast_addr));
		if(n<0)
		{
			perror("sendto()");
			return -2;
		}
		sleep(MCAST_INTERVAL);
	}
	return 0;
}
