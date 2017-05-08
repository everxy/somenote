/*************************************************************************
	> File Name: db_test.c
	> Author: 
	> Mail: 
	> Created Time: 三  5/ 3 15:33:31 2017
 ************************************************************************/
#include <stdio.h>
#include "apue_db.h"
#include <fcntl.h>
#include <stdlib.h>
#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP |S_IROTH)
#define IDXLEN_MAX 1024
int main(void)
{
    DBHANDLE    db;
    char *str;
    char key[IDXLEN_MAX];
    if((db = db_open("db4",O_RDWR|O_CREAT|O_TRUNC,FILE_MODE)) == NULL)
    {
        printf("db_open error");
        exit(-1);
    }
    if(db_store(db,"a","hello",DB_INSERT) != 0)
        ;
    db_store(db,"b","world",DB_INSERT);
    db_store(db,"c","alpha",DB_INSERT);
    db_store(db,"d","beta",DB_INSERT);
    db_store(db,"e","good",DB_INSERT);
    db_store(db,"f","yes",DB_INSERT);

    db_close(db);
    return 0;
}
