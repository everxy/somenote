/*************************************************************************
	> File Name: pipe1.c
	> Author: 
	> Mail: 
	> Created Time: 五  4/21 16:56:18 2017
 ************************************************************************/

#include<stdio.h>
#include "apue.3e/include/apue.h" 

int 
main(void)
{
    int         n;
    int         fd[2];
    pid_t       pid;
    char        line[100];

    if(pipe(fd) < 0)
        printf("pipe error");
    if((pid = fork()) < 0 )
        printf("fork error");
    else if (pid > 0) {
        close(fd[0]);
        write(fd[1],"hello world\n",12);
    }else {
        close(fd[1]);
        n = read(fd[0],line,100);
        write(STDOUT_FILENO,line,n);
    }
    exit(0);
}
