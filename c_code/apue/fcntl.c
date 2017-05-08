#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
int atoi(char s[])
{
	int i,c;
	int n = 0;
	i = 0;
	while((c = s[i]) != '\0'){
		if(c >= '0' && c <= '9')
			n = n * 10 + (c - '0');
		i++;
	}
	return n;
}

int main(int argc, char *argv[])
{
	int val;

	if(argc != 2){
		printf("usage: a.out\n");
		_exit(0);
	}

	printf("%d\n",argc);
	printf("%s\n",argv[0]);
	printf("%s\n",argv[1]);
	printf("%s\n",argv[2]);

	if((val = fcntl(atoi(argv[1]),F_GETFL,0)) < 0)
		printf("fcntl error for fd %d\n", atoi(argv[1]));

	switch (val & O_ACCMODE){
		case O_RDONLY:
			printf("read only\n");
			break;
		case O_WRONLY:
			printf("write only\n");
			break;

		case O_RDWR:
			printf("read write\n");
			break;
		default:
			printf("unkonwn access mode\n");
	}

	if(val & O_APPEND)
		printf(",append\n");
	if(val & O_NONBLOCK)
		printf(",O_NONBLOCK\n");
	if(val & O_SYNC)
		printf(",SYNC  WRITE\n");

	#if !defined(_POSIX_C_SOURCE) && defined(O_FSYNC) && (O_FSYNC != O_SYNC)
		if(val & O_FSYNC)
			printf(", SYNC WRITE");
	#endif

		putchar('\n');
		_exit(0);


}