/*************************************************************************
	> File Name: tcpsrvpoll.c
	> Author: 
	> Mail: 
	> Created Time: 三  5/10 15:20:43 2017
 ************************************************************************/

#include "../unp/unpv13e/lib/unp.h"
#include <limits.h> /*for OPEN_MAX*/

//pollfd结构示例。
//poll.h已经定义了该结构
//使用_避免vim提示错误
struct _pollfd {
    int         fd;
    short       events;
    short       revents;
};


int main(int argc,char *argv[])
{
    int             i,maxi,listenfd,connfd,sockfd;
    int             nready;
    ssize_t         n;
    char            buf[MAXLINE];
    socklen_t       clilen;
    struct pollfd   client[OPEN_MAX];
    struct sockaddr_in  cliaddr,servaddr;

    listenfd = Socket(AF_INET,SOCK_STREAM,0);

    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    Bind(listenfd,(SA *)&servaddr,sizeof(servaddr));

    Listen(listenfd,LISTENQ);
    
    client[0].fd = listenfd;
    client[0].events = POLLRDNORM;

    for(i = 0;i < OPEN_MAX; i++)
        client[i].fd = -1;
    maxi = 0;

    for(;;) {

        /*
        * INFTIM :永远等待
        * */
        nready = Poll(client,maxi + 1,INFTIM);

        if(client[0].revents & POLLRDNORM) {/* 新的客户端连接过来*/
            clilen = sizeof(cliaddr);
            connfd = Accept(listenfd,(SA *)&cliaddr,&clilen);

            for(i = 1; i < OPEN_MAX; i++){
                if(client[i].fd < 0) {
                    client[i].fd = connfd;
                    break;
                }
            }
            if(i == OPEN_MAX)
                err_quit("too many clients");
            client[i].events = POLLRDNORM;
            if(i > maxi)
                maxi = i;
            if(--nready <= 0)
                continue;
        }

        for(i = 1;i <= maxi;i++) {
            if((sockfd = client[i].fd) < 0)
                continue;
            if(client[i].revents & (POLLRDNORM | POLLERR)) {
                if((n = read(sockfd,buf,MAXLINE)) < 0) {
                    if(errno == ECONNRESET) {
                        Close(sockfd);
                        client[i].fd = -1;
                    } else 
                        err_sys("read error");

                } else if(n == 0) {
                    Close(sockfd);
                    client[i].fd = -1;
                } else 
                    Writen(sockfd,buf,n);

                if(--nready <= 0)
                    break;
            }
        }
    }
}
