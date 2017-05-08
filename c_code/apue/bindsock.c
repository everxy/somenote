/*************************************************************************
	> File Name: bindsock.c
	> Author: 
	> Mail: 
	> Created Time: 二  4/25 09:37:37 2017
 ************************************************************************/

#include<stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "apue.3e/include/apue.h"

int main(void)
{
    int     fd,size;
    struct  sockaddr_un un;
    un.sun_family = AF_UNIX;
    strcpy(un.sun_path,"foo.socket");
    if((fd = socket(AF_UNIX,SOCK_STREAM,0)) < 0)
        err_sys("socket failed");
    size = offsetof(struct sockaddr_un,sun_path) + strlen(un.sun_path);
    if(bind(fd,(struct sockaddr *)&un,size) < 0)
        err_sys("bind failed");
    printf("unix domain socket bind\n");
    exit(0);
}
