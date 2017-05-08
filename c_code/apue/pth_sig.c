/*************************************************************************
	> File Name: pth_sig.c
	> Author: 
	> Mail: 
	> Created Time: 三  4/19 10:26:35 2017
 ************************************************************************/

#include<stdio.h>
#include "apue.3e/include/apue.h"
#include <pthread.h>


int quitflag;
sigset_t  mask;

pthread_mutex_t lock =  PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  waitloc = PTHREAD_COND_INITIALIZER;

void*
thr_fn(void *arg)
{
    int err,signo;

    for(;;) {
        err = sigwait(&mask,&signo);
        if(err != 0)
            err_exit(err,"sigwait error");
        switch (signo) {
            case SIGINT:
                printf("\ninterrupt\n");
                break;
            case SIGQUIT:
                pthread_mutex_lock(&lock);
                quitflag = 1;
                pthread_mutex_unlock(&lock);
                pthread_cond_signal(&waitloc);
                return(0);

            default:
                printf("unexited signal%d\n",signo);
                exit(1);
        }
    }
}

int 
main(void)
{
    int     err;
    sigset_t  oldmask;
    pthread_t   tid;

    sigemptyset(&mask);
    sigaddset(&mask,SIGINT);
    sigaddset(&mask,SIGQUIT);
    
    if((err = pthread_sigmask(SIG_BLOCK,&mask,&oldmask)) != 0)
        err_exit(err,"SIG_BLOCK error");

    pthread_create(&tid,NULL,thr_fn,0);

    pthread_mutex_lock(&lock);
    while(quitflag == 0)
        pthread_cond_wait(&waitloc,&lock);
    pthread_mutex_unlock(&lock);

    quitflag = 0;

    if(sigprocmask(SIG_SETMASK,&oldmask,NULL) < 0)
        err_sys("sig_setmask,error");
    
    exit(0);
}


