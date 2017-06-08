#include "../../unpv13e/lib/unp.h"
void        sig_chld(int),sig_int(int),web_child(int);

int main(int argc,char **argv)
{
    int         listenfd,connfd;
    pid_t       childpid;

    socklen_t   clilen,addrlen;
    struct sockaddr *cliaddr;

    if(argc == 2)
        listenfd = Tcp_connect(NULL,argv[1],&addrlen);
    else if(argc == 3)
        listenfd = Tcp_connect(argv[1],argv[2],&addrlen);
    else
        err_quit("usage");
    cliaddr = Malloc(addrlen);

    Signal(SIGCHLD,sig_chld);
    Signal(SIGINT,sig_int);

    for (; ;) {
        clilen = addrlen;
        if((connfd = accept(listenfd,cliaddr,&clilen)) < 0) {
            if(errno == EINTR)
                continue;
            else
                err_sys("accept error");
        }

        if((childpid = Fork()) == 0) {
            Close(listenfd);
            web_child(connfd);
            exit(0);
        }
        Close(connfd);
    }
}

void sig_int(int signo)
{
    void pr_cpu_time(void);
    pr_cpu_time();
    exit(0);
}

