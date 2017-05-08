/*************************************************************************
	> File Name: apue_db.h
	> Author: 
	> Mail: 
	> Created Time: 二  4/25 16:49:06 2017
 ************************************************************************/

#ifndef _APUE_DB_H
#define _APUE_DB_H

typedef void * DBHANDLE;

DBHANDLE        db_open(const char *,int ,...);
void            db_close(DBHANDLE);
char           *db_fetch(DBHANDLE,const char *);
int             db_store(DBHANDLE,const char *,const char *,int );
int             db_delete(DBHANDLE,const char *);
void            db_rewind(DBHANDLE);
char           *db_nextrec(DBHANDLE,char *);

/*
* flags for db_store() 
* 
*/

#define DB_INSERT   1
#define DB_REPLACE  2
#define DB_STORE    3

//最小的索引记录长度，表示1字节键，1字节分割符，1字节起始偏移量，另一个分隔符，数据记录长度，换行符

#define IDXLEN_MIN  6
#define IDXLEN_MAX  1024

//一条数据记录的最短长度，1字节记录，换行符
#define DATLEN_MIN  2
#define DATLEN_MAX  1024

#endif
