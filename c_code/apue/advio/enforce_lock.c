#include "../apue.3e/include/apue.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "../sigproc.c"
#include "../set_fl.c"


int main(int argc, char const *argv[])
{
	int  		fd;
	pid_t 		pid;
	char 		buf[5];
	struct stat statbuf;

	if(argc != 2) {
		fprintf(stderr, "usage:%s filename\n",argv[0]);
		exit(1);
	}

	if((fd = open(argv[1],O_RDWR | O_CREAT | O_TRUNC, FILE_MODE)) < 0)
		err_sys("open error");
	if(write(fd,"abde",4) != 4)
		err_sys("write error");


	if(fstat(fd,&statbuf) < 0)
		err_sys("fstat error");	

		//使得强制性锁机制起效	
	if(fchmod(fd,(statbuf.st_mode & ~S_IXGRP) | S_ISGID) < 0)
		err_sys("fchmod error");

	TELL_WAIT();

	if((pid = fork()) < 0){
		err_sys("fork error");
	} else if (pid > 0) {
		if(write_lock(fd,0,SEEK_SET,0) < 0)
			err_sys("write_lock error");
		
		TELL_CHILD(pid);

		if(waitpid(pid,NULL,0) < 0)
			err_sys("waitpid error");
	} else {
		WAIT_PARENT();

		set_fl(fd,O_NONBLOCK);

		if (read_lock(fd,0,SEEK_SET,0) != -1)
			err_sys("read_lock error");

		printf("read_lock of alreadylocked region return %d\n", errno);

		if(lseek(fd,0,SEEK_SET) == -1)
			err_sys("lseek error");
		if(read(fd,buf,2) < 0)
			err_ret("read failed");
		else
			printf("read ok ( no  locking ),buf = %2.2s\n", buf);
	}

	return 0;
}