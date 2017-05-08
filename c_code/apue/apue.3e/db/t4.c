#include "apue_db.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP |S_IROTH)
#define IDXLEN_MAX 1024
int
main(void)
{
	DBHANDLE	db;

	if ((db = db_open("db5", O_RDWR | O_CREAT | O_TRUNC,
	  FILE_MODE)) == NULL)
		;

	if (db_store(db, "Alpha", "data1", DB_INSERT) != 0)
		;
	if (db_store(db, "beta", "Data for beta", DB_INSERT) != 0)
		;
	if (db_store(db, "gamma", "record3", DB_INSERT) != 0)
		;
	db_close(db);
	exit(0);
}
