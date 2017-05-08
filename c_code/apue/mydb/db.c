/*************************************************************************
	> File Name: db.c
	> Author: 
	> Mail: 
	> Created Time: 二  4/25 16:59:36 2017
 ************************************************************************/

#include<stdio.h>
#include "../apue.3e/include/apue.h"
#include "apue_db.h"

//open 和db_open的flags
//m
#include <fcntl.h>
//可变参数函数
#include <stdarg.h>
#include <errno.h>

//iovec 结构体，writev和readv需要使用该结构体
#include <sys/uio.h>

//索引记录的长度
#define IDXLEN_SZ   4 /*索引占4个asc字符长*/
#define SEP         ':'
#define SPACE       ' '
#define NEWLINE     '\n'

/*
* hash表相关变量
*/
#define PTR_SZ      7
#define PTR_MAX     99999

//hash table size
//大小为137记录项
#define NHASH_DEF   137

//off--offsize
#define FREE_OFF    0
#define HASH_OFF    PTR_SZ


typedef unsigned long DBHASH;
typedef unsigned long COUNT;

/*
* DB 结构体记录打开数据库中的所有信息
* 因为在数据库中是以asc形式存放，所以将其转化为数字值
* */
typedef struct {
    int         idxfd; /*fd for index*/
    int         datfd; /*fd for data*/
    char       *idxbuf;/*索引记录的buffer*/
    char       *datbuf;
    char       *name;
    off_t       idxoff;/* 索引中的索引记录的偏移量*/
    size_t      idxlen;/*索引记录的长度，但是不包括idxlen_sz，包括换行符*/
    off_t       datoff;
    size_t      datlen;
    off_t       ptrval;/*索引记录中指针链的内容，也就是下一条记录的偏移量*/
    off_t       ptroff;/*该idx record的偏移量*/
    off_t       chainoff;/*这条散列链的偏移量*/
    off_t       hashoff;/*散列表的起始偏移量*/
    DBHASH      nhash;/*hash表的大小，这里是137*/
    
    /*操作的各种情况*/
    COUNT       cnt_delok;
    COUNT       cnt_delerr;
    COUNT       cnt_fetchok;
    COUNT       cnt_fetcherr;
    COUNT       cnt_nextrec;
    COUNT       cnt_stor1;/*DB_INSERT,no empty,appended*/
    COUNT       cnt_stor2;/*ins,found empty,reuserd*/
    COUNT       cnt_stor3;/*replace,diff len,appended*/
    COUNT       cnt_stor4;/* replace,same len,overwirte*/
    COUNT       cnt_storerr;

} DB;


/*内部方法*/
static DB       *_db_alloc(int);
static void     _db_dodelete(DB *);
static int      _db_find_and_lock(DB *,const char *,int);
static int      _db_findfree(DB *,int,int);
static void     _db_free(DB *);
static DBHASH   _db_hash(DB *,const char *);
static char     *_db_readdat(DB *);
static off_t    _db_readidx(DB *,off_t);
static off_t    _db_readptr(DB *,off_t);
static void     _db_writedat(DB *,const char *,off_t,int);
static void     _db_writeidx(DB *,const char *,off_t,int,off_t);
static void     _db_writeptr(DB *,off_t,off_t);

/*fcntl函数,当用来设置锁的时候，第三个参数为flock结构指针。
* cmd:F_SETLK:设置由flock结构设置的锁；非阻塞版本
*     F_SETLKW:阻塞版本；
*     F_GETLK:判断flock结构描述的锁是否被另一把所排斥。如果存在，则将现在锁信息重写进flock结构中
*               不存在则将l_type设置为F_UNLCK。
* */
int lock_reg(int fd,int cmd,int type,off_t offset,int whence,off_t len)
{
    struct  flock lock;
    /*type:F_RDLCK;F_WRLCK;F_UNLCK*/
    lock.l_type = type; 

    lock.l_start = offset;
   
    lock.l_whence = whence;
    lock.l_len = len;
    /*还有个l_pid属性这里并不需要*/

    return(fcntl(fd,cmd,&lock));
}
/*
apue.h中定义的读写锁函数。

#define read_lock(fd,offset,whence,len) lock_reg((fd),F_SETLK,F_RDLCK,(offset),(whence),(len))
#define readw_lock(fd,offset,whence,len) lock_reg((fd),F_SETLKW,F_RDLCK,(offset),(whence),(len))
#define write_lock(fd,offset,whence,len) lock_reg((fd),F_SETLK,F_WRLCK,(offset),(whence),(len))
#define writew_lock(fd,offset,whence,len) lock_reg((fd),F_SETLKW,F_WRLCK,(offset),(whence),(len))
#define un_lock(fd,offset,whence,len) lock_reg((fd),F_SETLK,F_UNLCK,(offset),(whence),(len))


*/


/*
* 打开或者建立一个数据库，会生成pathname.idx和pathname.dat文件
* db_open的参数与open()函数的 参数一致。
* oflag:O_RDONLY,O_WRONLY,O_RDWR,O_EXEC,O_SEARCH(只搜索，用于目录)，5个参数必须有且只有一个
* 其余常用参数：
* O_APPEND(追加),O_CREAT,O_NONBLOCK(如果是FIFO，块或字符特殊文件，对本次打开以及后续i/o操作设置为非阻塞)，等等
*/
DBHANDLE db_open(const char *pathname,int oflag,...)
{
    DB      *db;
    int     len,mode;
    size_t  i;
    char    asciiptr[PTR_SZ + 1],hash[(NHASH_DEF + 1) * PTR_SZ + 2];
    struct  stat  statbuff;

    /*为db结构体分配空间，并且初始化次结构*/
    len = strlen(pathname);
    if((db = _db_alloc(len)) == NULL)
    {
        printf("%d\n",__LINE__);
        exit(-1);
    }
    db->nhash = NHASH_DEF;
    db->hashoff = HASH_OFF;/*第一个空闲链表跳过，散列表起始的位置为7*/
    strcpy(db->name,pathname);
    strcat(db->name,".idx");
    
    if(oflag & O_CREAT) {/*新建文件*/
        /*因为是可变参数，所以使用va_list来解决，在头文件stdarg*/
        va_list     ap; /*实际上是一个指针*/
        va_start(ap,oflag);/*初始化指针ap，并且使其指向可变参数列表的第一个参数*/
        mode = va_arg(ap,int);/*获取可变参数，第二个参数是想要返回的该参数的类型，如果有多个，依次调用该方法获取*/
        va_end(ap);/*结束获取*/
    
        /*打开index文件或者data文件*/
        db->idxfd = open(db->name,oflag,mode);
        strcpy(db->name + len,".dat");
        db->datfd = open(db->name,oflag,mode);
    } else {/*打开已有数据库文件*/
        db->idxfd = open(db->name,oflag);
        strcpy(db->name + len,".dat");
        db->datfd = open(db->name,oflag);
    }
    
    /*打开任一文件失败的话*/
    if(db->idxfd < 0 || db->datfd < 0){
        _db_free(db);/*清理db结构*/
        return(NULL);
    }
    
    /*如果是创建文件，回忆open中的O_TRUNC表示如果文件存在，并且被只读或者读写成功打开，则将长度截断为0*/
    if((oflag & (O_CREAT | O_TRUNC)) == (O_CREAT | O_TRUNC)) {
        
        /*有可能有多个进程同时会进行创建同一个数据库，所以必须加锁*/
        if(writew_lock(db->idxfd,0,SEEK_SET,0) < 0)
            err_dump("db_open:writew_lock error");
        /*回忆stat族函数，返回文件的信息结构见p75
        * stat指向pathname的文件，
        * fstat打开fd指向的相对目录的文件，
        * lstat返回符号链接，不是所引用文件
        * */
        if(fstat(db->idxfd,&statbuff) < 0)
            err_dump("db_open:fstat error ");
        /*如果长度为0，刚被创建
        * sprintf:将格式化数据写入字符串
        * */
        if(statbuff.st_size == 0) {
            //%*d将数据库指针从整形转换为asc字符串，这里的指针是0，因为刚创建；
            //并且去PTR_SZ参数作为指针参数的最小字段宽度
            sprintf(asciiptr,"%*d",PTR_SZ,0);
            hash[0] = 0;
            for(i = 0; i < NHASH_DEF + 1; i++)
                strcat(hash,asciiptr);
            strcat(hash,"\n");
            i = strlen(hash);
            if(write(db->idxfd,hash,i) != i)
                err_dump("db_open:index file init write error");

        }
        if(un_lock(db->idxfd,0,SEEK_SET,0) < 0)
            err_dump("db_open:un_lock error");
    }
    db_rewind(db);
    return(db);
}

static DB *_db_alloc(int namelen)
{
    DB      *db;
    /*calloc :参数为元素个数和元素大小，可以将分配的空间初始化，保证其为0或者空指针等
    * malloc：参数为分配空间大小；
    * */
    if((db = calloc(1, sizeof(DB))) == NULL)
        err_dump("_db_alloc:calloc error");
    
    //calloc 的副作用，将文件描述符也设置为了0，所以要将其重新设置为-1，表示其至此还不是有效的。
    //db_open 函数中会打开两个文件描述符，如果任意失败，则其还是-1，
    db->idxfd = db->datfd = -1;
    
    /* 分配空间存放名字
    * +5是因为‘.idx’或‘.dat’ + null
    */
    if((db->name = malloc(namelen + 5)) == NULL)
        err_dump("_db_alloc:malloc error");
    
    /*
    * 为索引文件和数据文件的缓冲区分配空间。
    * +2 是为了在后面有\n和null
    * 记录下大小后，如果还需空间，通过realloc继续分配
    * */
    if((db->idxbuf = malloc(IDXLEN_MAX + 2)) == NULL)
        err_dump("_db_alloc:malloc error for index buffer %d,%d",db->idxbuf,IDXLEN_MAX);
    if((db->datbuf = malloc(DATLEN_MAX + 2)) == NULL)     
        err_dump("_db_alloc:malloc error for data buffer");
    return(db);   
}

void db_close(DBHANDLE h)
{
    _db_free((DB *)h);
}

static void _db_free(DB *db)
{
    if(db->idxfd >= 0)
        close(db->idxfd);
    if(db->datfd >= 0)
        close(db->datfd);
    if(db->idxbuf != NULL)
        free(db->idxbuf);
    if(db->datbuf != NULL)
        free(db->datbuf);
    if(db->name != NULL)
        free(db->name);
    free(db);
}


char *db_fetch(DBHANDLE h,const char *key)
{
    DB      *db = h;
    char    *ptr;
    
    //参数3：0-读，非0-写，根据这个决定锁
    if(_db_find_and_lock(db,key,0) < 0)
    {
        ptr = NULL;
        db->cnt_fetcherr++;
    }else {
        ptr = _db_readdat(db);
        db->cnt_fetchok++;
    }

    if(un_lock(db->idxfd,db->chainoff,SEEK_SET,1) < 0)
        err_dump("db_fetch:un_lock error");
    return(ptr);
}

/*
 * writelock:0-读锁，非0-写锁
 * 返回值：0：成功；-1：失败
*/
static int _db_find_and_lock(DB *db,const char *key,int writelock)
{
    off_t   offset,nextoffset;
    int j;
    /*计算key的hash值，确定起始位置，每一个链表指针的宽度为PTR_SZ,并且这里的hashoff在open中已经
    * 被赋值为PTR_SZ，跳过第一条空闲链表*/
    j = _db_hash(db,key);
    db->chainoff = (j * PTR_SZ) + db->hashoff;
    db->ptroff = db->chainoff;/*使得指针指向该链表*/
    
    /*
    * 加锁，注意这里只是将链的第一个字节锁住
    * 所以允许多个进程同时搜索不同的散列链，增加了并发性
    * */
    if(writelock)
    {
        if(writew_lock(db->idxfd,db->chainoff,SEEK_SET,1) < 0)
            err_dump("_db_find_and_lock:writew_lock error");
    } else {
        if(readw_lock(db->idxfd,db->chainoff,SEEK_SET,1) < 0)
            err_dump("_db_find_and_lock:readw_lock error");
    }
    
    //读散列链的第一个指针，如果返回0，则该散列链为空。
    offset = _db_readptr(db,db->ptroff);
    while(offset != 0) {
        //读取一条索引记录存入db->idxbuf,并且返回了下一条记录的偏移量
        nextoffset = _db_readidx(db,offset);
        if(strcmp(db->idxbuf,key) == 0)
            break;

        db->ptroff = offset;/*存放上一条索引记录的地址*/
        offset = nextoffset;
    }

    return(offset == 0 ? -1 : 0);
}

static DBHASH _db_hash(DB *db,const char *key)
{
    DBHASH hval = 0;
    char c;
    int  i;
    int  j;
    //hash计算，将key中的每个字符的asc码值*在字符中的位置，并加起来。
    for(i = 1; (c = *key++) != 0;i++)
        hval += c * i;
    //return(hval % db->nhash);/*db->nhash = 137 它是一个素数，素数通常能提供很好的分布特性*/
    j = hval % 137;	
    return j;
}

/*读取一个指针并转换为长整型，获取指针值都需要这个函数。包括以下三种指针
 * 索引文件开始出的空闲链表指针；散列表中指向散列链的第一条索引记录的指针；
 * 每条索引记录的开始、指向下一条记录的指针，
* */
static off_t _db_readptr(DB *db,off_t offset)
{
    char    asciiptr[PTR_SZ + 1];
    
    //设置打开文件的文件偏移量，
    //SEEK_SET:距离文件开始出offset个字节；
    //SEEK_CUR:距离当前值加offset，offset可负；
    //SEEK_END:文件长度加offset，同理offset可负
    if(lseek(db->idxfd,offset,SEEK_SET) == -1)
        err_dump("_db_readptr:lseek error");
    
    //一次读取7个字符
    if(read(db->idxfd,asciiptr,PTR_SZ) != PTR_SZ)
        err_dump("_db_readptr:read error");
    
    asciiptr[PTR_SZ] = 0;//追加空字符，
    return(atol(asciiptr));
}

/*根据索引记录的偏移量读取下一条索引记录，将其记录到db->idxbuf中，通过null字符分割
* */
static off_t _db_readidx(DB *db,off_t offset)
{
    ssize_t         i;
    char            *ptr1,*ptr2;
    char            asciiptr[PTR_SZ + 1],asciilen[IDXLEN_SZ + 1];
    struct iovec    iov[2];/*iovec结构用于readv和writev*/
    
    /*首先为读取设置偏移量，如果是0，则在当前偏移量位置读。*/
    if((db->idxoff = lseek(db->idxfd,offset,offset == 0 ? SEEK_CUR:SEEK_SET)) == -1)
        err_dump("_db_readidx:lseek error");
    iov[0].iov_base = asciiptr;
    iov[0].iov_len  = PTR_SZ;
    iov[1].iov_base = asciilen;
    iov[1].iov_len  = IDXLEN_SZ;
    /*readv,writev用于在一次函数调用中读、写多个非连续缓冲区
     * 比如这里readv从iov[0]开始读两个缓冲区的内容
     * 长度为缓冲区长度之和。这里缓冲区需要索引记录长度和一个指向下个记录的指针的长度
    */
    if((i = readv(db->idxfd,&iov[0],2)) != PTR_SZ + IDXLEN_SZ) {
        if(i == 0 && offset == 0)
            return(-1);
        err_dump("_db_readidx:readv error of index record");
    }
    
    //指针null结束符
    asciiptr[PTR_SZ] = 0;
    //索引链中下一条记录的偏移量。
    db->ptrval = atol(asciiptr);

    asciilen[IDXLEN_SZ] = 0;
    
    if((db->idxlen = atoi(asciilen)) < IDXLEN_MIN || db->idxlen > IDXLEN_MAX)
        err_dump("_db_readidx:invalid length");

    if((i = read(db->idxfd,db->idxbuf,db->idxlen)) != db->idxlen)
        err_dump("_db_readidx:readerror of index record");
    if(db->idxbuf[db->idxlen-1] != NEWLINE)
        err_dump("_db_readidx:missing newline");
    /*将换行符替换为空字符，为atol函数准备
    * atol函数遇到空字符结束
    * */
    db->idxbuf[db->idxlen-1] = 0;
    
    /*找出第一个分隔符
    * strchr(char *s,char c);找出c在s中第一次出现的位置，返回位置的指针或者NULL
    * */
    if((ptr1 = strchr(db->idxbuf,SEP)) == NULL)
        err_dump("_db_readidx:missing first separator");
    *ptr1++ = 0;/*分隔符替换为空字符，并指向下一位，即数据记录的偏移量*/
    
    if((ptr2 = strchr(ptr1,SEP)) == NULL)
        err_dump("_db_readidx:missing second separator");
    *ptr2++ = 0;/*ptr2指向数据记录的长度*/

    if(strchr(ptr2,SEP) != NULL)
        err_dump("_db_readidx:too many separator");
    
    /*保存数据所在的偏移量和长度，长度应当在2~1024字节之间*/
    if((db->datoff = atol(ptr1)) < 0)
        err_dump("_db_readidx:starting offset < 0");
    if((db->datlen = atol(ptr2)) <= 0 || db->datlen > DATLEN_MAX)
        err_dump("_db_readidx:invalid length");
    /*返回到下一条记录的偏移量*/
    return(db->ptrval);
}

/*
 * 读取实际的数据到datbuf中
*/
static char *_db_readdat(DB *db)
{
    if(lseek(db->datfd,db->datoff,SEEK_SET) == -1)
        err_dump("_db_readdat:lseek error");
    if(read(db->datfd,db->datbuf,db->datlen) != db->datlen)
        err_dump("_db_readdat:read error");

    /*索引记录为换行符结尾，并且在读取记录后将其替换为空字符结尾*/
    if(db->datbuf[db->datlen-1] != NEWLINE)    
        err_dump("_db_readdat:missing newline");
    db->datbuf[db->datlen-1] = 0;
    return(db->datbuf);
}

/*
* 根据键值删除一条记录，
* */
int db_delete(DBHANDLE h,const char *key)
{
    DB      *db = h;
    int     rc  = 0;/*假设记录会被找到*/
    
    /*为操作加一把写锁，因为可能会执行更改链表的操作*/
    if(_db_find_and_lock(db,key,1) == 0)
    {
        _db_dodelete(db);
        db->cnt_delok++;
    } else {
        //查找记录失败
        rc = -1;
        db->cnt_delerr++;
    }
    
    if(un_lock(db->idxfd,db->chainoff,SEEK_SET,1) < 0)
        err_dump("db_delete:un_lock error");
    return(rc);
}
/*
* 内部方法：删除记录
* 被delete和store功能调用，使用前需通过_db_find_and_lock方法加锁
* */
static void _db_dodelete(DB *db)
{
    int         i;
    char        *ptr;
    off_t       freeptr,saveptr;
    
    /*
    * 将数据缓冲和key都填充为空格
    * */
    for(ptr = db->datbuf,i = 0; i < db->datlen - 1;i++)
        *ptr++ = SPACE;
    *ptr = 0;
    ptr = db->idxbuf;
    while(*ptr)
        *ptr++ = SPACE;
    /*
    * db->datbuf = "    '\0'"
    * db->idxbuf = "   \0 datoff \0 datlen \0"
    * */
    
    /*对空闲链表进行加锁，FREE_OFF = 0
    * 因为删除后要将被删除的记录添加进空闲链表中，这样会改变空闲链表指针。
    * */
    if(writew_lock(db->idxfd,FREE_OFF,SEEK_SET,1) < 0)
        err_dump("_db_dodelete:writew_lock error");
    
    /*
    * 向数据记录中写入空白行，不需要加锁了，因为在调用函数中已经加锁，并且这里的
    * datoff已经在_db_readidx中已经设置好了。
    * */
    _db_writedat(db,db->datbuf,db->datoff,SEEK_SET);

    /*
    * 读取空闲链表指针
    * */
    freeptr = _db_readptr(db,FREE_OFF);
    //下一条索引记录
    saveptr = db->ptrval;

    /*
    * 修改删除索引记录，使得其链表指针中指向的下一条记录为freeptr
    * 而freeptr是记录空闲链表的第一条索引的偏移量。
    * 也就是相当于在空闲链表头部插入记录。
    * */
    _db_writeidx(db,db->idxbuf,db->idxoff,SEEK_SET,freeptr);
    /*将空闲链表指针指向删除条目，这样就完成了头部插入空闲链表*/
    _db_writeptr(db,FREE_OFF,db->idxoff);
    
    /*使得上一条ptroff记录的内容指向被删除条目的下一条，即从散列链中删除了该记录*/
    _db_writeptr(db,db->ptroff,saveptr);
    if(un_lock(db->idxfd,FREE_OFF,SEEK_SET,1) < 0)
        err_dump("_db_dodelete:un_lock error");
}

/*
* 写数据记录，会在dodelete和store中调用
* */
static void _db_writedat(DB *db,const char *data,off_t offset,int whence)
{
    struct iovec        iov[2];
    static char         newline = NEWLINE;

    //因为在追加，也就是store，此时需要加锁；如果是del操作，不需要，因为在上层函数调用中已经加锁
    if(whence == SEEK_END)
    {
        if(writew_lock(db->datfd,0,SEEK_SET,0) < 0)
            err_dump("_db_writedat:writew_lock error");
    }
    
    if((db->datoff = lseek(db->datfd,offset,whence)) == -1)
        err_dump("_db_writedat:lseek error");
    db->datlen = strlen(data) + 1;
    //聚集写，注意不能想当然的认为在调用者缓冲区尾端有空间追加换行，所以将其写入另一个缓冲区。
    iov[0].iov_base = (char *)data;
    iov[0].iov_len  = db->datlen - 1;
    iov[1].iov_base = &newline;
    iov[1].iov_len  = 1;
    if(writev(db->datfd,&iov[0],2) != db->datlen)
        err_dump("_db_writedat:writev error of data record");
    if(whence == SEEK_END)
        if(un_lock(db->datfd,0,SEEK_SET,0) < 0)
            err_dump("_db_writedat:un_lock error");
}

/*写一条索引记录*/
static void _db_writeidx(DB *db,const char *key,off_t offset,int whence,off_t ptrval)
{
    struct iovec    iov[2];
    char            asciiptrlen[PTR_SZ + IDXLEN_SZ + 1];
    int             len;
    //检测索引记录是否合法
    if((db->ptrval = ptrval) < 0 || ptrval > PTR_MAX)
        err_quit("_db_writeidx:invalid ptr:%d",ptrval);
    //索引记录的后半部分，使用强制转换是因为off_t和size_t因为平台不同而不同
    //datoff和datlen已经在_db_writedat函数中设置了。
    //同时，被删除的索引记录依然指向原来的数据，但是key部分已经变空了。
    sprintf(db->idxbuf,"%s%c%lld%c%ld\n",key,SEP,(long long)db->datoff,SEP,(long)db->datlen);
    len = strlen(db->idxbuf);
    if(len < IDXLEN_MIN || len > IDXLEN_MAX)
        err_dump("_db_writeidx:invalid length");
    //索引记录的前半部分，ptrval的宽度为PTR_SZ,len的宽度为IDXLEN_SZ。
    sprintf(asciiptrlen,"%*lld%*d",PTR_SZ,(long long)ptrval,IDXLEN_SZ,len);
    if(whence == SEEK_END)/*在追加则是加锁*/
    {   //跳过了空闲链表
        if(writew_lock(db->idxfd,((db->nhash + 1)*PTR_SZ) + 1,SEEK_SET,0) < 0)
            err_dump("_db_writeidx:writew_lock error");
    }
    if((db->idxoff = lseek(db->idxfd,offset,whence)) == -1)
        err_dump("_db_writeidx:lseek error");

    iov[0].iov_base = asciiptrlen;
    iov[0].iov_len  = PTR_SZ + IDXLEN_SZ;
    iov[1].iov_base = db->idxbuf;
    iov[1].iov_len  = len;
    if(writev(db->idxfd,&iov[0],2) != PTR_SZ + IDXLEN_SZ + len)
        err_dump("_db_writeidx:writev error");
    if(whence == SEEK_END)
        if(un_lock(db->idxfd,((db->nhash+1)*PTR_SZ)+1,SEEK_SET,0) < 0)
            err_dump("_db_writeidx:un_lock error");
}

static void _db_writeptr(DB *db,off_t offset,off_t ptrval)
{
    char        asciiptr[PTR_SZ + 1];
    if(ptrval < 0 || ptrval > PTR_MAX)
        err_quit("_db_writeptr:ptrval invalid %d",ptrval);
    sprintf(asciiptr,"%*lld",PTR_SZ,(long long)ptrval);

    if(lseek(db->idxfd,offset,SEEK_SET) == -1)
        err_dump("_db_writeptr:lseek error");
    if(write(db->idxfd,asciiptr,PTR_SZ) != PTR_SZ)
        err_dump("_db_writeptr:write error");
}
/*
 * 存储一条记录
 * 返回值：
 *  0：成功
 *  1：已存在
 * -1：错误
 * */
int db_store(DBHANDLE h,const char *key,const char *data,int flag)
{
    DB      *db = h;
    int     rc,keylen,datlen;
    off_t   ptrval;
    
    if(flag != DB_INSERT && flag != DB_REPLACE && flag != DB_STORE)
    {
        errno = EINVAL;
        return(-1);
    }

    keylen = strlen(key);
    datlen = strlen(data) + 1;

    if(datlen < DATLEN_MIN || datlen > DATLEN_MAX)
        err_dump("db_store:invalid length of data");
    
    if(_db_find_and_lock(db,key,1) < 0) {
        /*查找记录失败*/
        
        if(flag == DB_REPLACE) {
            rc = -1;
            db->cnt_storerr++;
            errno = ENOENT;
            goto  doreturn;
        }

        //读取指向散列链开始的偏移量，chainoff已经在find&lock函数中设置
        ptrval = _db_readptr(db,db->chainoff);

        /*
        * 空闲链表中查找一条可用记录，它的键长度和数据长度不小于所需长度。
        * 即是首次匹配原则
        * */
        if(_db_findfree(db,keylen,datlen) < 0) {
            //找不到足够大的记录，需要在散列链末尾添加记录
            //这里先写数据记录，会设置好datoff和datlen，然后写索引文件
            //分别在索引记录文件和数据记录文件的末尾新增
            _db_writedat(db,data,0,SEEK_END);

            //让其链表指针指向散列链开始，即头部插入
            _db_writeidx(db,key,0,SEEK_END,ptrval);
            
            //散列链开头指向新插入节点
            _db_writeptr(db,db->chainoff,db->idxoff);
            db->cnt_stor1++;
        } else {
            //找到了合适空记录
            //_db_findfree会将其从空闲链表中移除，并且设置好
            //db->datoff和db->idxoff
            _db_writedat(db,data,db->datoff,SEEK_SET);

            //任然是在头部插入记录
            _db_writeidx(db,key,db->idxoff,SEEK_SET,ptrval);
            _db_writeptr(db,db->chainoff,db->idxoff);
            db->cnt_stor2++;
        }
    } else {
        //查找记录成功
        if(flag == DB_INSERT) {
            rc = 1;
            db->cnt_storerr++;
            goto doreturn;
        }
        /*替换操作*/
        if(datlen != db->datlen) {
            /*长度不一致，先删除，然后重新添加新纪录*/
            _db_dodelete(db);
            ptrval = _db_readptr(db,db->chainoff);
            _db_writedat(db,data,0,SEEK_END);
            _db_writeidx(db,key,0,SEEK_END,ptrval);
            _db_writeptr(db,db->chainoff,db->idxoff);
            db->cnt_stor3++;
        } else {
            /*长度相同，直接替换*/
            _db_writedat(db,data,db->datoff,SEEK_SET);
            db->cnt_stor4++;
        }
    }
    
    rc = 0;
    doreturn:
        if(un_lock(db->idxfd,db->chainoff,SEEK_SET,1) < 0)
            err_dump("db_store:un_lock error");
        return(rc);
}

static int _db_findfree(DB *db,int keylen,int datlen)
{
    int         rc;
    off_t       offset,nextoffset,saveoffset;

    if(writew_lock(db->idxfd,FREE_OFF,SEEK_SET,1) < 0)
        err_dump("_db_findfree:writew_lock error");
    saveoffset = FREE_OFF;
    /*第一条空闲记录的偏移地址*/
    offset = _db_readptr(db,saveoffset);
    while(offset != 0)
    {
        /*读取索引记录*/
        nextoffset = _db_readidx(db,offset);
        if(strlen(db->idxbuf) == keylen && db->datlen == datlen)
            break;
        saveoffset = offset;
        offset = nextoffset;
    }

    if(offset == 0)
    {
        rc = -1;
    } else {
        /*
         * 找到合适空闲记录，db->ptrval会在readidx中设置，
         * ptrval是下一条记录的偏移量，然后让上一条记录指向ptrval
         * 这样就在空闲链表中移除了该条记录
        */
        _db_writeptr(db,saveoffset,db->ptrval);
        rc = 0;
    }
    
    if(un_lock(db->idxfd,FREE_OFF,SEEK_SET,1) < 0)
        err_dump("_db_findfree:un_lock error");
    return(rc);
}

/*将索引文件的偏移量定位到第一条索引记录的开头*/
void db_rewind(DBHANDLE h)
{
    DB      *db = h;
    off_t   offset;
    
    //+1是表示空闲链表
    offset = (db->nhash + 1) * PTR_SZ;
    
    //+1是跳过换行符
    if((db->idxoff = lseek(db->idxfd,offset + 1,SEEK_SET)) == -1)
        err_dump("db_rewind:lseek error");
}

/*&返回数据库的下一条记录，返回值是指向数据缓冲区的指针*/
char *db_nextrec(DBHANDLE h,char *key)
{
    DB      *db = h;
    char    c;
    char    *ptr;

    if(readw_lock(db->idxfd,FREE_OFF,SEEK_SET,1) < 0)
        err_dump("db_nextrec:readw_lock error");
    do {
        if(_db_readidx(db,0) < 0) {
            ptr = NULL;
            goto doreturn;
        }
        
        ptr = db->idxbuf;
        while((c = *ptr++) != 0 && c == SPACE)
            ;
    } while(c == 0);

    if(key != NULL)
        strcpy(key,db->idxbuf);
    ptr = _db_readdat(db);
    db->cnt_nextrec++;

    doreturn:
        if(un_lock(db->idxfd,FREE_OFF,SEEK_SET,1) < 0)
        err_dump("db_nextrec:un_lock error");
    return(ptr);
}

