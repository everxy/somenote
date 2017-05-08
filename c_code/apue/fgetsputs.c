#include "apue.3e/include/apue.h"

int main(void)
{
	/* code */
	char buf[MAXLINE];
	while(fgets(buf,MAXLINE,stdin) != NULL)
		if(fputs(buf,stdout) == EOF)
			err_sys("output error");

	if(ferror(stdin))
		err_sys("input error");
	return 0;
}