#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

char buf1[] = "asfasfasf";
char buf2[] = "ADSFADSFA";

int main(void){
	int fd;

	if((fd = creat("file.hole",FILESEC_MODE)) < 0)
		printf("creat error \n");
	if(write(fd,buf1,10) != 10)
		printf("buf write error\n");

	if(lseek(fd,16384,SEEK_SET) == -1)
		printf("lseek error\n");

	if(write(fd,buf2,10) != 10)
		printf("buf2 write error\n");

	// unistd.h中定义
	// _exit(0);

	// stdlib.h中定义
	exit(0);
}
