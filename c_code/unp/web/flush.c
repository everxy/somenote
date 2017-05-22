/*************************************************************************
	> File Name: flush.c
	> Author: 
	> Mail: 
	> Created Time: 四  5/18 16:32:18 2017
 ************************************************************************/

#include<stdio.h>
#include <stdlib.h>

int main()
{
    int a;
    char s;

    scanf("%d",&a);
    fflush(stdin);
    s = getchar();

    printf("a = %d,s = %c \n",a,s);

    return 0;
}
