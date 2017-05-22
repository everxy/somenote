/*************************************************************************
	> File Name: web.h
	> Author: 
	> Mail: 
	> Created Time: 三  5/17 11:03:35 2017
 ************************************************************************/

#ifndef _WEB_H
#define _WEB_H
#include "../unpv13e/lib/unp.h"
#define MAXFILES 20
#define SERV    "80"

struct file {
    char    *f_name;
    char    *f_host;
    int     f_fd;
    int     f_flags;
} file[MAXFILES];

#define F_CONNECTING    1
#define F_READING       2
#define F_DONE          4

#define GET_CMD         "GET %s HTTP/1.0\r\n\r\n"

/*
 * nlefttoconn:尚无TCP连接的文件数
 * nlefttoread:仍待读取的文件数（达到0时程序任务完成）
 * nconn：当前打开着的连接数
 * */
int  nconn,nfiles,nlefttoconn,nlefttoread,maxfd;
fd_set rset,wset;

void home_page(const char *,const char *);
void start_connect(struct file *);
void write_get_cmd(struct file *);

#endif
