/*************************************************************************
	> File Name: atfok.c
	> Author: 
	> Mail: 
	> Created Time: 三  4/19 15:43:32 2017
 ************************************************************************/

#include<stdio.h>
#include "apue.3e/include/apue.h"
#include <pthread.h>

pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock2 = PTHREAD_MUTEX_INITIALIZER;


void
prepare(void)
{
	int err;
	printf("prepare locking\n");

	if((err = pthread_mutex_lock(&lock1)) != 0)
		err_cont(err,"lock1 prepare error");
	if((err = pthread_mutex_lock(&lock2)) != 0)
		err_cont(err,"lock2 prepare error");
}

void 
parent(void)
{
	int err;

	printf("parent unlocking \n");

	if((err = pthread_mutex_unlock(&lock1)) != 0)
		err_cont(err,"lock1 unlocking error in parent");
	if((err = pthread_mutex_unlock(&lock2)) != 0)
		err_cont(err,"lock2 unlock error in parent" );
}

void 
child(void)
{
	int err;

	printf("child unlocking \n");

	if((err = pthread_mutex_unlock(&lock1)) != 0)
		err_cont(err,"lock1 unlocking error in child");
	if((err = pthread_mutex_unlock(&lock2)) != 0)
		err_cont(err,"lock2 unlock error in child");

}

void *
thr_fn(void *arg)
{
	printf("thread started\n");
	pause();
	return(0);
}

int 
main(void)
{
	int 		err;
	pid_t 		pid;
	pthread_t 	tid;

	if((err = pthread_atfork(prepare,parent,child)) != 0)
		err_exit(err,"can't install fork handlers");

	if((err = pthread_create(&tid,NULL,thr_fn,0)) != 0)
		err_exit(err,"can't create thread");

	sleep(10);
	printf("parent about to fork\n");

	if((pid = fork()) < 0)
		err_quit("fork error");
	else if(pid == 0)
		printf("child return from fork\n");
	else
		printf("parent return from fork\n");
	exit(0);
}
