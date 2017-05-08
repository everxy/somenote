/*************************************************************************
	> File Name: timetcpsrv.c
	> Author: 
	> Mail: 
	> Created Time: 一  5/ 8 11:01:37 2017
 ************************************************************************/

#include <stdio.h>
#include "../unpv13e/lib/unp.h"

int main(int argc,char *argv[])
{
    int     listenfd,connfd;
    struct  sockaddr_in servaddr;
    char    buf[MAXLINE];
    time_t  ticks;

    if((listenfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
        err_sys("socket error");

    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(13);

    if(bind(listenfd,(SA *)&servaddr,sizeof(servaddr)) < 0)
        err_sys("bind error");
    if(listen(listenfd,LISTENQ) < 0)
        err_sys("listen error");
    
    for(;;) {
        connfd = accept(listenfd,(SA *)NULL,NULL);
        
        //获取时间
        ticks = time(NULL);
        //格式化输出到buf,这里在时间后面加上了\r\n,对应在tcp总每个PDU的结束标志
        snprintf(buf,sizeof(buf),"%.24s\r\n",ctime(&ticks));
        //写入socket句柄
        write(connfd,buf,strlen(buf));

        close(connfd);
    }
}
