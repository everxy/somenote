/*************************************************************************
	> File Name: add2stdio.c
	> Author: 
	> Mail: 
	> Created Time: 六  4/22 16:07:49 2017
 ************************************************************************/

#include<stdio.h>
#include "apue.3e/include/apue.h"

static void sig_pipe(int);

int 
main(void)
{
    int         n,fd1[2],fd2[2];
    pid_t       pid;
    char        line[MAXLINE];

    if(signal(SIGPIPE,sig_pipe) == SIG_ERR)
        return 0;
    if(pipe(fd1) < 0 || pipe(fd2) < 0)
        return 0;
    if((pid = fork()) < 0) {
        return 0;
    } else if(pid > 0) {   
        //fd1 读端关闭
        close(fd1[0]);
        
        //fd2 写段关闭
        close(fd2[1]);
        
        //对每读取的一行操作
        while(fgets(line,MAXLINE,stdin) != NULL ){
            n = strlen(line);
            if(write(fd1[1],line,n) != n)
                return 0;
            if((n = read(fd2[0],line,MAXLINE)) < 0)
                return 0;

            if(n == 0) {
                printf("child closed pipe");
                break;
            }

            line[n] = 0;
            if(fputs(line, stdout) == EOF)
                err_sys("fputs error");
        }

        if(ferror(stdin))
            err_sys("fgets error on stdin ");
        exit(0);
    } else {
        close(fd1[1]);
        close(fd2[0]);

        if(fd1[0] != STDIN_FILENO) {
            if(dup2(fd1[0],STDIN_FILENO) != STDIN_FILENO);
                err_sys("dup2 error in stdin ");
            close(fd1[0]);
        }

        if(fd2[1] != STDOUT_FILENO) {
            if(dup2(fd2[1],STDOUT_FILENO) != STDOUT_FILENO)
                err_sys("dup2 error");
            close(fd2[1]);
        }

        if(execl("./add2","add2",(char *)0 ) < 0)
            return 0;
    }

    exit(0);
}


static void sig_pipe(int signo)
{
    printf("SIGPIPE caught\n");
    exit(1);
}
