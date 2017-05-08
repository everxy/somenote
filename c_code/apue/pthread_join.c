/*************************************************************************
	> File Name: pthread_join.c
	> Author: 
	> Mail: 
	> Created Time: 一  4/17 16:10:40 2017
 ************************************************************************/

#include<stdio.h>
#include "apue.3e/include/apue.h"

#include<pthread.h>

void *
thr_fn1(void *arg)
{
    printf("thread 1 returning \n");
    return ((void *)1);
}


void *
thr_fn2(void *arg)
{
    printf("thread 2 returning \n");
    pthread_exit((void *)2);
}

int 
main(void){
    int err;
    pthread_t tid1,tid2;
    void  *tret;

    err = pthread_create(&tid1,NULL,thr_fn1,NULL);

    err = pthread_create(&tid2,NULL,thr_fn2,NULL);
    
    err = pthread_join(tid1,&tret);

    printf("thread 1 exit code %ld\n",(long)tret);

    pthread_join(tid2,&tret);
    printf("thread 2 exit code %ld\n",(long)tret);
    exit(0);
}
