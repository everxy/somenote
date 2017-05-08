/*************************************************************************
	> File Name: timetcpcli.c
	> Author: 
	> Mail: 
	> Created Time: 日  5/ 7 14:35:54 2017
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "../unpv13e/lib/unp.h"

int main(int argc,char *argv[])
{
    int sockfd,n;
    char recvline[MAXLINE + 1];
    struct sockaddr_in servaddr;

    if(argc != 2) {
        printf("usage error");
        exit(0);
    }
    //socket的通常用法,0是使用默认协议
    if((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
        err_sys("socket error");
    bezro(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(13);
    if(inet_pton(AF_INET,argv[1],&servaddr.sin_addr) <= 0)
        err_quit("inet_pton error");
    if(connect(sockfd,(SA *)&servaddr,sizeof(servaddr)) < 0)
        err_sys("connect error");

    while((n = read(sockfd,recvline,MAXLINE)) > 0)
    {
        recvline[n] = 0;
        if(fputs(recvline,stdout) == EOF)
            err_sys("fputs error");
    }

    if(n < 0)
        err_sys("read error");
    exit(0);
}
