/*************************************************************************
	> File Name: strclinonb.c
	> Author: 
	> Mail: 
	> Created Time: 二  5/16 15:48:21 2017
 ************************************************************************/

#include<stdio.h>
#include "./unpv13e/lib/unp.h"

void str_cli(FILE *fp,int  sockfd)
{
    int             maxfdp1,val,stdineof;
    ssize_t         n,nwritten;
    fd_set          rset,wset;
    char            to[MAXLINE],fr[MAXLINE];
    char            *toiptr,*tooptr,*friptr,*froptr;

    val = fcntl(sockfd,F_GETFL,0);
    fcntl(STDIN_FILENO,F_SETFL,val | O_NONBLOCK);

    val = fcntl(sockfd,F_GETFL,0);
    fcntl(sockfd,F_SETFL,val | O_NONBLOCK);

    val = fcntl(sockfd,F_GETFL,0);
    fcntl(STDOUT_FILENO,F_SETFL,val | O_NONBLOCK);

    toiptr = tooptr = to;
    friptr = froptr = fr;
    stdineof = 0;

    maxfdp1 = max(max(STDIN_FILENO,STDOUT_FILENO),sockfd) + 1;

    for(;;) {
        FD_ZERO(&rset);
        FD_ZERO(&wset);

        /* 从标准输入读入*/
        if(stdineof == 0 && toiptr <= &to[MAXLINE])
            FD_SET(STDIN_FILENO,&rset);
        /*read from socket*/
        if(friptr < &fr[MAXLINE])
            FD_SET(sockfd,&rset);
        //write to socket
        if(tooptr != toiptr)
            FD_SET(sockfd,&wset);
        //write to stdout
        if(froptr != friptr)
            FD_SET(STDOUT_FILENO,&wset);

        select(maxfdp1,&rset,&wset,NULL,NULL);

        if(FD_ISSET(STDIN_FILENO,&rset)) {
            if((n = read(STDIN_FILENO,toiptr,&to[MAXLINE] - toiptr)) < 0) {
                if(errno != EWOULDBLOCK)
                    err_sys("read error on stdin");
            } else if(n == 0) {
                fprintf(stderr,"%s:EOF on stdin \n",gf_time());
                stdineof = 1;
                if(tooptr == toiptr)
                    shutdown(sockfd,SHUT_WR);
            } else {
                fprintf(stderr,"%s：read %d bytes from stdin\n",gf_time(),n);
                toiptr += n;
                FD_SET(sockfd,&wset);
            }
        }

        if(FD_ISSET(sockfd,&rset)) {
            if((n = read(sockfd,friptr,&fr[MAXLINE] - friptr)) < 0) {
                if(errno != EWOULDBLOCK)
                    err_sys("read error on socket");
            } else if(n == 0) {
                fprintf(stderr,"%s:EOF in socket\n",gf_time());
                if(stdineof)
                    return;
                else 
                    err_quit("str_cli:server terminated");
            } else {
                fprintf(stderr,"%s：read %d bytes from stdin\n",gf_time(),n);
                friptr += n;
                FD_SET(STDOUT_FILENO,&wset);
            }
        }

        if(FD_ISSET(STDOUT_FILENO,&wset) && ((n = friptr - froptr) > 0)) {
            if((nwritten = write(STDOUT_FILENO,froptr,n)) < 0) {
                if(errno != EWOULDBLOCK)
                    err_sys("read error on socket");
            } else {
                fprintf(stderr,"%s：read %d bytes from stdin\n",gf_time(),nwritten);
                froptr += nwritten;
                if(froptr == friptr)
                    froptr = friptr = fr;
            }
        }

        if(FD_ISSET(sockfd,&wset) && ((n = toiptr - tooptr) > 0)) {
            if((nwritten = write(sockfd,tooptr,n)) < 0) {
                if(errno != EWOULDBLOCK)
                    err_sys("read error on socket");
            } else {
                fprintf(stderr,"%s：read %d bytes from stdin\n",gf_time(),nwritten);
                tooptr += nwritten;
                if(tooptr == toiptr)
                    tooptr = toiptr = to;
                if(stdineof)
                    shutdown(sockfd,SHUT_WR);
            }
        }
    }
}
