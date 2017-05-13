/*************************************************************************
	> File Name: udpsrv.c
	> Author: 
	> Mail: 
	> Created Time: 四  5/11 10:23:09 2017
 ************************************************************************/

#include<stdio.h>

#include "../unpv13e/lib/unp.h"

void dg_echo(int sockfd,SA *pcliaddr,socklen_t clilen);

int main(int argc,char *argv[])
{
    int         sockfd;
    struct sockaddr_in servaddr,cliaddr;

    sockfd = Socket(AF_INET,SOCK_DGRAM,0);

    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    Bind(sockfd,(SA *)&servaddr,sizeof(servaddr));
    
    dg_echo(sockfd,(SA *)&cliaddr,sizeof(cliaddr));
}

void dg_echo(int sockfd,SA *pcliaddr,socklen_t clilen)
{
    int         n;
    socklen_t   len;
    char        mesg[MAXLINE];

    for(;;) {
        len = clilen;
        n = Recvfrom(sockfd,mesg,MAXLINE,0,pcliaddr,&len);

        Sendto(sockfd,mesg,n,0,pcliaddr,len);
    }
}
