#include "../apue.3e/include/apue.h"
#include <errno.h>
#include <fcntl.h>

static void
lockabypt(const char *name,int fd,off_t offset)
{
	if(writew_lock(fd,offset,SEEK_SET,1) < 0)
		err_sys("%s:writew_lock error",name);
	printf("%s:got the lock,byte %lld\n", name,(long long)offset);
}

int
main(void)
{
	int 	fd;
	pid_t 	pid;


	fd = creat("templocl",FILE_MODE);

	write(fd,"ab",2);

	TELL_WAIT();

	if ((pid = fork()) < 0)
	{
		err_sys("fork error");
	} else if(pid == 0) {
		lockabypt("child",fd,0);
		TELL_PARENT(getppid());
		WAIT_PARENT();
		lockabypt("child",fd,1);
	} else {
		lockabypt("parent",fd,1);
		TELL_CHILD(pid);
		WAIT_CHILD();
		lockabypt("parent",fd,0);
	}

	exit(0);
}