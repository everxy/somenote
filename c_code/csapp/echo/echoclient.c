/*************************************************************************
	> File Name: echoclient.c
	> Author: 
	> Mail: 
	> Created Time: 五  5/ 5 20:21:28 2017
 ************************************************************************/

#include<stdio.h>
#include"../code/include/csapp.h"
int open_clientfd(char *hostname,int port);


/*client 的主程序*/
int main(int argc,char *argv[])
{
    int clientfd;
    char *host,*port,buf[MAXLINE];
    rio_t rio;

    if(argc != 3) {
        exit(0);
    }
    host = argv[1];
    port = argv[2];
    clientfd = open_clientfd(host,port);
    Rio_readinitb(&rio,clientfd);

    while(Fgets(buf,MAXLINE,stdin) != NULL) {
        Rio_writen(clientfd,buf,strlen(buf));
        Rio_readinitb(&rio,buf,MAXLINE);
        Fputs(buf,stdout);
    }

    Close(clientfd);
    exit(0);
}

/*server端主程序---迭代处理版本*/
int main(int argc ,char *argv[])
{
    int listenfd,connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    char client_hostname[MAXLINE],client_port[MAXLINE];

    if(argc != 2) {
        exit(0);
    }

    listenfd = open_listenfd(argv[1]);
    while(1) {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = accept(listenfd,(SA *)&clientaddr,&clientlen);
        getnameinfo((SA *)&clientaddr,clientlen,client_hostname,MAXLINE,client_port,MAXLINE,0);
        printf("connect to (%s %s)\n",client_hostname,client_port);
        echo(connfd);
        close(connfd);
    }
    exit(0);
}

/*server端主程序--基于进程并发版本*/
void sigchld_handler(int sig)
{
    /*waitpid(PID,statloc,options)
    * pid==-1.等待任一子进程。等效于wait
    * pid > 0 等待pid
    * pid  == 0 .等待调用组id的任一子进程
    * pid < -1  取绝对值的子进程。
    * WNOHANG：即是没有子进程退出，也会立即返回，不会一直像wait一直等待。返回0；
    *
    * 并且这里使用while来回收可能多个出现的SIGCHLD信号，因为信号阻塞且不排队
    * */
    while(waitpid(-1, 0,WNOHANG) > 0)
        ;
    return;
}

int main(int argc,char *argv[]) 
{
    int listenfd,connfd;
    socklen_t  clientlen;
    struct sockaddr_storage clientaddr;

    if(argc != 2) {
        exit(0);
    }
    /* 并发中fork多个子进程，所以需要处理SIGCHLD信号回收僵死子进程资源*/
    signal(SIGCHLD,sigchld_handler);
    listenfd = open_listenfd(argv[1]);
    while(1) {
        clientfd = sizeof(struct sockaddr_storage);
        connfd = accept(listenfd,(SA *)&clientaddr,&clientlen);
        if(fork() == 0) {
            Close(listenfd);
            //子进程回显
            echo(connfd);
            //子进程关闭连接
            close(connfd);
            exit(0);
        }
        //父进程也要关闭连接
        close(connfd);
    }
}

/*并发echo版本2：基于IO多路复用，服务器既能相应用户键盘输入，又能响应客户端连接请求
* 三种情况：
* {0，4}任意描述符准备好  读 时
* {1,2,4}任意描述符准备好 写 时
* 超时
* 这里对第一种讨论
* */

//pool结构维护活动客户端的集合
typedef struct {
    int maxfd;
    fd_set read_set;
    fd_set ready_set;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];
} pool;

int byte_cnt = 0;
int main(int argc,char *argv[])
{
    int listenfd,connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    struct pool pool;

    listenfd = open_listenfd(argv[1]);
    init_pool(listenfd,&pool);
    while(1) {
        //将read_set作为ready_set
        pool.ready_set = pool.read_set;
        /*select(int fd,fd_set readfds,fd_set writefds,fd_set execpfds,struct timeval tvptr)
         * 最后一个参数tvptr:NULL 表示永远等待直到一个信号中断
         * 中间三个分别是：可读，可写，异常的三个描述符集的指针
         * 第一个：最大文件描述符+1，提高效率，不必去扫描那些不需要的描述符
         * 这select就会阻塞直到中断，判断其是设置的listenfd还是STDIN_FILENO事件发生，然后返回其描述符数
         * */
        pool.nready = select(pool.maxfd+1,&pool.ready_set,NULL,NULL,NULL);
        
        //使用FD_ISSET()判断是哪一个准备好并且做相应处理
        if(FD_ISSET(STDIN_FILENO,&ready_set))
            command();
        if(FD_ISSET(listenfd,&pool.ready_set)) {
            clientlen = sizeof(struct sockaddr_storage);
            connfd = accept(listenfd,(SA *)&clientaddr,&clientlen);
            add_client(connfd,&pool);
        }

        check_clients(&pool);
    }
}
//初始化描述符集合
void init_pool(int listenfd,pool *p)
{
    int i;
    p->maxi = -1;
    for(i = 0;i < FD_SETSIZE; i++)
        p->clientfd[i] = -1;
    //最大文件描述符设置问监听描述符
    p->maxfd = listenfd;
    //将读集合设置为0
    FD_ZERO(&p->read_set);
    //将监听文件描述符加入到读集合
    FD_SET(listenfd,&p->read_set);
    FD_SET(STDIN_FILENO,&p->read_set);
}

void add_client(int connfd,pool *p)
{
    int i;
    p->nready--;
    for(i = 0;i < FD_SETSIZE;i++) 
    {
        if(p->clientfd[i] < 0) 
        {
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i],connfd);

            FD_SET(connfd,&p->read_set);

            if(connfd > p->maxfd)
                p->maxfd = connfd;
            if(i > p->maxi)
                p->maxi = i;
            break;
        }
    }
    if(i == FD_SETSIZE)
        app_error("add_client error");
}

void check_clients(pool *p) 
{
    int i,connfd,n;
    char buf[MAXLINE];
    rio_t rio;

    for(i = 0;(i <= p->maxi) && (p->nready > 0);i++) {
        connfd = p->clientfd[i];
        rio = p->clientrio[i];

        if((connfd > 0) && (FD_ISSET(connfd,&p->ready_set))) {
            p->nready--;
            if((n = Rio_readinitb(&rio,buf,MAXLINE)) != 0) {
                byte_cnt += n;
                printf("server recevied %d (%d total) bytes on fd %d\n",n,byte_cnt,connfd);
                Rio_writen(connfd,buf,n);
            } else {
                close(connfd);
                FD_CLR(connfd,&p->read_set);
                p->clientfd[i] = -1;
            }
        }
    }
}
/* IO多路复用的优点：在单一进城上下文中，每个逻辑流都能独立访问该进程的全部地址空间
 * 缺点：基于事件的设计使其无法从分利用多核处理器
 * 并发echo2 end*/


/*基于线程的并发编程*/

void *thread(void *vargp);
int main(int argc,char *argv[])
{
    int listenfd,*connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    listenfd = open_listenfd(atgv[1]);

    while(1) {
        //这里对于每个连接描述符分配内存，保证其在accept返回后的描述符在各自的内存中
        //避免了在accept和thread函数中的第一个赋值语句之间的潜在竞争
        clientlen = sizeof(struct sockaddr_storage);
        connfd = malloc(sizeof(int));
        *connfdp = accept(listenfd,(SA *)&clientaddr,&clientlen);
       
        /* pthread_create(pid,attr,start_fun,arg_of_start_func)
         * 该函数第一个参数用来存放生成的线程id
         * attr：指定线程属性，
         * start_fun：线程从该函数开始执行
         * arg：传入start_fun的参数结构体，这里需要将已连接的描述符传递给线程
         * 所以传入该指针，并在线程中间接引用(?why)并赋值给局部变量。
         * 是否是因为可能有多个线程，所以需要间接引用，避免一个线程将其关闭后，其他线程无法使用
         *
         * */
        pthread_create(&tid,NULL,thread,connfdp);
    }
}

void *thread(void *vargp)
{
    //间接引用这个指针
    int connfd = *((int *)vargp);
    //没有显式的回收进程，就分离每个线程，使得其资源最终能被收回
    pthread_detach(pthread_self());
    
    //这里释放了参数！，释放了主线程中分配的内存块
    free(vargp);
    echo(connfd);
    close(connfd);
    return NULL;
}



int open_listenfd(int port)
{
    int listenfd,optval = 1;
    struct sockaddr_in serveraddr;

    if((listenfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
        return -1;

    if(setsockopt(listenfd,SOL_SOCKET,SO_REUSERADDR,(const void *)&optval,sizeof(int)) < 0)
        return -1;

    bzero((char *)&serveraddr,sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);
    if(bind(listenfd,(SA *)&serveraddr,sizeof(serveraddr)) < 0)
        return -1;

    if(listen(listenfd,LISTENQ) < 0)
        return -1;
    return listenfd;
}
int open_clientfd(char *hostname,int port)
{
    int clientfd;
    struct hostent *hp;
    struct sockaddr_in serveraddr;

    if((clientfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
        return -1;
    
    /*
    * gethostbyname(const char *name):根据域名或者主机名返回hostent结构
    *
    * struct hostent {
    *   char *h_name;
    *   char **h_aliases;
    *   int  h_addrtype;
    *   int  h_length;
    *   char **h_addr_list;
    *   #define h_addr  h_addr_list[0]
    * }
    * */
    if((hp = gethostbyname(hostname)) == NULL)
        return -2;
    bzero((char *) &serveraddr,sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)hp->h_addr_list[0],(char *)&serveraddr.sin_addr.s_addr,hp->h_length);
    serveraddr.sin_port = htons(port);

    if(connect(clientfd,(SA *)&serveraddr,sizeof(serveraddr)) < 0)
        return -1;
    return clientfd;
}
