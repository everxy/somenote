#include <unistd.h>
#include <stdio.h>

int main(void){
	if(lseek(STDIN_FILENO,0,SEEK_CUR) == -1)
		printf("cannot lseek\n");
	else
		printf("seek ok\n");
	_exit(0);
}