/*************************************************************************
	> File Name: add2.c
	> Author: 
	> Mail: 
	> Created Time: 六  4/22 15:51:43 2017
 ************************************************************************/

#include<stdio.h>
#include "apue.3e/include/apue.h"

int main(void)
{
    int         n,int1,int2;
    char        line[MAXLINE];

    while((n = read(STDIN_FILENO,line,MAXLINE)) > 0) {
        line[n] = 0;
        if(sscanf(line,"%d%d",&int1,&int2) == 2)
        {
            sprintf(line,"%d\n",int1+int2);
            n = strlen(line);
            if(write(STDOUT_FILENO,line,n) != n)
                err_sys("write error");
        } else {
            if(write(STDOUT_FILENO,"invalid args\n",13) != 13)
                err_sys("write error");
        }
    }
    exit(0);
        
}
