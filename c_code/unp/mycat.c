/*************************************************************************
	> File Name: mycat.c
	> Author: 
	> Mail: 
	> Created Time: 一  5/15 20:24:28 2017
 ************************************************************************/

#include<stdio.h>
#include "./unpv13e/lib/unp.h"

int     my_open(const char *,int);

int main(int argc,char **argv)
{
    int         fd,n;
    char        buff[BUFFSIZE];
    if((fd = my_open(argv[1],O_RDONLY)) < 0)
        err_sys("cannot open %s",argv[1]);
    while((n = read(fd,buff,BUFFSIZE)) > 0)
        write(STDOUT_FILENO,buff,n);

    exit(0);
}

int my_open(const char *pathname,int mode)
{
    int     fd,sockfd[2],status;
    pid_t   childpid;
    char    c,argsockfd[10],argmode[10];

    socketpair(AF_LOCAL,SOCK_STREAM,0,sockfd);

    if((childpid = fork()) == 0) {
        close(sockfd[0]);
        snprintf(argsockfd,sizeof(argsockfd),"%d",sockfd[1]);
    }
}
