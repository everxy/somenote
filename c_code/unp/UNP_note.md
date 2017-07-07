## 基本

### UDP(用户数据包协议)

- 不可靠：不确定能送到目的地；不确定数据报先后顺序，不保证数据包的正确和完整；
- 有长度
- 无连接：c/s之间不需要存在长期的连接。

### tcp(传输控制协议)

- 连接的。
- 无无长度，无边界记录，是字节流的
- 可靠：数据的可靠传递或者故障通知
- TCP可以通过给每个字节关联序列号而进行对所发送数据的排序，所以s端可以根据这个来对接受到的数据排序和判断是否重复
- 含有估算c/s之间的往返时间的算法。
- 提供流量控制。
- 全双工。—UDP可以是全双工的。

### 套接字结构地址

```c
/*IPv4*/
struct in_addr {
	in_addr_t 	s_addr;
}

struct sockaddr_in {
  uint8_t 		sin_len; //结构体长度,16字节
  sa_family_t 	sin_family; //AF_INET
  in_port_t		sin_port; //16位的端口号，网络字节序
  struct in_addr sin_addr; //32位的ip地址，网络字节序
  char sin_zero[8] //占坑，未使用
}

//通用套接字结构，唯一地用处就是在想函数传递参数的时候，需要将套接字结构强制转换为该结构
struct sockaddr {
  uint8_t		sa_len;
  sa_familt_t 	sa_family; //AF_XXX
  char 			sa_data[14];
}
书上：
typedef SA 	(struct sockaddr)

//IPv6的通用套接字机构
struct sockaddr_storage {
  uint8_t		ss_len;
  sa_family_t 	ss_family;
}
足够大，其他字段是透明的，必须类型强制转换为或复制到合适以ss_family字段所给出的地址类型的套接字地址结构中，才能访问其他字段。但是一般使用的时候，在给函数传入参数的时候还是将其转换为SA。
```

### 地址转换函数



## TCP

### TCP的分组交换（包含三次握手和四次挥手）

![](https://ww2.sinaimg.cn/large/006tNbRwly1ffdzna9a0mj30i40nbgni.jpg)

MSS：发送SYN的一端用来告知最大分节大小

TIME_WAIT：有时间限制。存在理由

- 可靠的实现全双工连接的终止（发起close的一端可能由于最终的ACK-这里的n+1-丢失而重传）
- 允许老的重复分节都在网络中消失

#### 基本TCP套接字过程的函数详细图

![](https://ww1.sinaimg.cn/large/006tNbRwly1ffe9o3ny9rj30jq0id3zz.jpg)



---

### 值-结果参数

以套接字结构的长度传递方向来看：

进程—》内核：bind、connect、sendto。在传递长度的时候直接传值

```c
struct sockaddr_in servaddr;
connect(sockfd,(SA *)&servaddr,sizeof(serv));
```

内核—》进程：accept、recvfrom、getsockname、getpeername

```c
struct sockaddr_in cli;
socklen_t len;
len = sizeof(cli);
accept(sockfd,(SA *)&cli,&len);
```

这里传入accept的大小以指针形式传入，因为：

- 函数调用时：结构大小是一个值（value），告诉内核该结构大小。保证不越界

- 函数返回时：结构大小是一个结果（result），告诉进程，内核存储了多少信息在结构中。

  在固定的结构长度中，返回值是固定的。对于可变长度的套接字地址（比如unix域的sockaddr_un）返回值可能小于该结构的最大值。

这就是`值-结果`参数。

---

### WAIT和WAITPID

在使用进程并发版本的server端程序时，需要捕捉SIGCHLD信号来处理已完成的子进程避免其成为僵死进程。在处理信号的时候，使用signal函数进入捕捉处理函数（如自己定义的sig_chld()）；

`wait`函数：

​	当产生了多个SIGCHLD，捕捉到一个信号，就会进入signal函数处理，而其他的信号则可能无法处理，而产生僵死进程。如果使用循环多次调用wait，又因为其在子程序还未处理完成时会阻塞，而导致进程一直阻塞无法执行下去。

`waitpid`

​	使用`waitpid(-1,&stat,WNOHANG)`则可解决这个问题。

```c
while((pid = waitpid(-1,&stat,WNOHANG)) > 0)
  	;
```

WNOHANG:表示其在子进程为结束返回时不阻塞，直接返回0；

-1：接受任意子进程。

---

### getosockname、getpeername

这两个函数的最后一个参数都是值-结果参数。也就是这**两个函数都会装填所指的套接字地址结构**。

- 在没有bind的tcp客户机上，connect返回后，调用getsockname来**获取内核赋予的本地IP地址和端口号**
- 在以端口号为0（告知内核选择本地端口号），getsockname用于**返回内核赋予的本地端口号**。
- getsockname也可以获取某个套接字的地址族
- 在服务器调用过accept的某个进行再调用exec执行后，获取**客户**的IP地址和端口号等用户信息使用getpeername。

---

### 出错相关

服务器杀死连接着的子进程，会导致TCP收到FIN。但FIN并不会告知客户端服务器已经终止。当cli继续发送，会收到RST，如果cli因为在执行某个处理函数时直接返回了0，可能收不到RST，并且客户端答应错误信息，退出。

如果在此基础上继续发送多个信息，则可能导致SIGPIPE，该信号默认终止进程。

服务器主机崩溃：cli的TCP会持续重传数据分节，这种情况在不同系统可有不同的一直重传时间，应当对函数设置一个超时。如果想不主动发送数据也检测出服务器主机崩溃，可以使用SO_KEEPALIVE。

---

### I/O复用

select查看caspp下的echoclient.c

poll查看unp下的tcpsrvpoll.c



使用场景：

- 需要处理多个描述符
- TCP服务器既要处理监听套接字，又要处理已连接套接字
- 即处理TCP，又处理UDP
- 处理多个协议或者服务

#### select函数

如图，源码在csapp/echoclient.c。需要处理客户端的连接请求，又要处理服务的用户的键盘输入输出。所以使用select。

首先应该初始化描述符集合,描述符集合必须初始化，否则有不可预料的后果。先全部置为0，然后需要监听哪个描述符，就将其set为1，表示打开。

![](https://ww1.sinaimg.cn/large/006tNbRwly1fff7j69vvtj30b9031mxb.jpg)

其次在select中调用。

![](https://ww2.sinaimg.cn/large/006tNbRwly1fff77r8h7lj30nj04ggml.jpg)

使用`FD_ISSET()`来判断是哪个准备好并处理。

![](https://ww3.sinaimg.cn/large/006tNbRwly1fff7g7sidcj30hs03t3z6.jpg)

在初始化中，使用的是read_set用来进行记录，而使用select时，每次都在期之前将read_set赋值给ready_set，是因为FD_ISSET在调用后，会将描述符内任何未就绪描述都清为0，为此在使用select之前必须重新将关心的位置为1。所以使用了两个集合，一个初始化，另一个表示就绪位。

描述符集其实就是值-结果参数。

#### 批量输入

unp，select/strcliselect01.c

```c
void str_cli(FILE *p,int sockfd) {
  ...;
  for(;;) {
    ...;
    if(FD_ISSET(fileno(fp),&rset)) {
      if(Fgets(sendline,MAXLINE,fp) == NULL)
        return;  ----问题的出现
      writen(sockfd,sendline,strlen(sendline));
    }
  }
}
```

在标准输入中遇到EOF会返回mian函数。但是在网络中，请求都是一个接着一个的，虽然遇到了EOF，但是可能仍有请求在去往服务器或者仍有应答在返回客户端。

并且在select中，stdio是由缓冲区的，在这里fgets读取数据到缓存区，然后是返回第一行，其余输入仍在缓冲区。而当使用writen函数将输入写给服务器后，select会再次被调用以等待新工作，而不管stdio缓冲区还有额外内容。

​	select是不知道stdio有缓冲区的，它是从read系统调用的角度支出是否有数据可读。混合使用stdio和select是非常容易犯错的。

---

#### shutdown函数

​	shutdown函数可以使得tcp连接关闭一半。

close的限制：

1. close是将描述符引用-1，仅仅在计数变为0才关闭套接字。使用shutdown可以不管引用计数就激发TCP的正常连接终止序列。
2. close终止了读和写两个方向的数据发送。TCP是全双工的。使用shutdown就可以实现TCP的半关闭

----

#### 网络请求的使用情况

1. **多线程模型适用于处理短连接，且连接的打开关闭非常频繁的情形，但不适合处理长连接。这不同于多进程模型，线程间内存无法共享，因为所有线程处在同一个地址空间中。内存是多线程模型的软肋。**
2. *在UNIX平台下多进程模型擅长处理并发长连接，但却不适用于连接频繁产生和关闭的情形。**同样的连接需要的内存数量并不比多线程模型少，但是得益于操作系统虚拟内存的Copy on  Write机制，fork产生的进程和父进程共享了很大一部分物理内存。但是多进程模型在执行效率上太低，接受一个连接需要几百个时钟周期，产生一个进程 可能消耗几万个CPU时钟周期，两者的开销不成比例。而且由于每个进程的地址空间是独立的，如果需要进行进程间通信的话，只能使用IPC进行进程间通 信，而不能直接对内存进行访问。**在CPU能力不足的情况下同样容易遭受DDos，攻击者只需要连上服务器，然后立刻关闭连接，服务端则需要打开一个进程再关闭。
3. 同时需要保持很多的长连接，而且连接的开关很频繁，最高效的模型是非阻塞、异步IO模型。**而且不要用select/poll，这两个API的有着O(N)的时间复杂度。在Linux用epoll，BSD用kqueue，Windows用IOCP，或者用libevent封装的统一接口（对于不同平台libevent实现时采用各个平台特有的API），这些平台特有的API时间复杂度为O(1)。**然而在非阻塞，异步I/O模型下的编程是非常痛苦的。**由于I/O操作不再阻塞，报文的解析需要小心翼翼，并且需要亲自管理维护每个链接的状态。并且为了充分利用CPU，还应结合线程池，避免在轮询线程中处理业务逻辑。

---

### EPOLL

#### why

1. 从select那里只能知道是有I/O事件的发生，而并不知道是哪一个流。必须对其进行无差别的轮询，事件福再度O(n)；select返回的是整个句柄的数组，需要再一次遍历数组才能知道哪个事件发生。
2. select的最大监听描述符有限制，通常是1024，并且由于是轮询，所以在大量的描述符时，性能会很差
3. 内核 / 用户空间内存拷贝问题，select需要复制大量的句柄数据结构，产生巨大的开销；？
4. select的触发方式是水平触发，应用程序如果没有完成对一个已经就绪的文件描述符进行IO操作，那么之后每次select调用还是会将这些文件描述符通知进程。

相比如select，poll没有监视文件的约束，使用了链表保存文件描述符。（？unp上并不是这样）

#### 使用EPOLL，API

第一步：**epoll_create()**系统调用。此调用返回一个句柄，之后所有的使用都依靠这个句柄来标识。

第二步：**epoll_ctl()**系统调用。通过此调用向epoll对象中添加(EPOLL_CTL_ADD)、删除(.._DEL)、修改(._MOD)感兴趣的事件，返回0标识成功，返回-1表示失败。

第三部：**epoll_wait()**系统调用。通过此调用收集收集在epoll监控中已经发生的事件。

**结合实例代码unp/_epoll.c查看**

#### EPOLL的实现机制

设想一下如下场景：有100万个客户端同时与一个服务器进程保持着TCP连接。而每一时刻，通常只有几百上千个TCP连接是活跃的(事实上大部分场景都是这种情况)。如何实现这样的高并发？

​	在select/poll时代，服务器进程每次都把这100万个连接告诉操作系统(**从用户态复制句柄数据结构到内核态**)，让操作系统内核去查询这些套接字上是否有事件发生，**轮询完后，再将句柄数据复制到用户态**，让服务器应用程序轮询处理已发生的网络事件，这一过程资源消耗较大，因此，select/poll一般只能处理几千的并发连接。

​	epoll通过在Linux内核中申请一个简易的文件系统(**文件系统一般用什么数据结构实现？B+树**)。把原先的select/poll调用分成了3个部分：

1. 调用epoll_create()建立了一个epoll对象，在epoll文件系统中为这个句柄对象分配资源
2. 调用poll_ctl()函数向epoll对象中添加了100万个连接的套接字
3. 调用epoll_wait()收集发生的事件的连接

具体的实现机制：

当调用poll_create方法时，linux会在内核创建一个eventpoll结构体，

```c
struct eventpoll{
  ...;
  //红黑树的根节点，存储所有添加到epoll中需要监控的事件
  struct rb_root rbr;
  //双链表存放将要通过epoll_wait返回给用户的满足条件的事件
  struct list_head rdlist;
  ...
}
```

每一个epoll对象都有一个**独立的eventpoll结构体，用于存放通过epoll_ctl方法向epoll对象中添加进来的事件。这些事件都会挂载在红黑树中，如此，重复添加的事件就可以通过红黑树而高效的识别出来**(红黑树的插入时间效率是lgn，其中n为树的高度)。

而**所有添加到epoll中的事件都会与设备(网卡)驱动程序建立回调关系**，也就是说，当相应的事件发生时会调用这个回调方法。这个回调方法在内核中叫ep_poll_callback,**它会将发生的事件添加到rdlist双链表中**。在epoll中，对于每一个事件，都会建立一个epitem结构体，如下所示：

```c
struct epitem{
  struct rb_node rbn;  //红黑树节点
  struct list_head rdllink; //双向链表节点
  struct epoll_filed ffd; //事件句柄信息
  struct eventpoll *ep; //指向所属的eventpoll对象
  struct epoll_event event; //期待发生的事件类型
}
```

当调用**epoll_wait检查是否有事件发生时，只需要检查eventpoll对象中的rdlist双链表中是否有epitem元素即可**。如果rdlist不为空，则把发生的事件复制到用户态，同时将事件数量返回给用户。

#### 两种工作模式

##### **水平触发（LT）和边缘触发（ET）**

LT模式下，只要一个句柄上的事件一次没有处理完，会在以后调用epoll_wait时次次返回这个句柄，而ET模式仅在第一次返回。

ET：如果是事件没有处理完成，那么是无法再继续获取该事件的，同时，必须是使用非阻塞句柄才能使用。

##### **ET中的EPOLLIN和EPOLLOUT：**

EPOLLOUT事件：
EPOLLOUT事件只有在连接时触发一次，表示可写，其他时候想要触发，那你要先准备好下面条件：

1. 某次write，写满了发送缓冲区，返回错误码为EAGAIN。
2. 对端读取了一些数据，又重新可写了，此时会触发EPOLLOUT。

简单地说：EPOLLOUT事件只有在不可写到可写的转变时刻，才会触发一次，所以叫边缘触发，这叫法没错的！其实，如果你真的想强制触发一次，也是有办法的，直接调用epoll_ctl重新设置一下event就可以了，event跟原来的设置一模一样都行（但必须包含EPOLLOUT），关键是重新设置，就会马上触发一次EPOLLOUT事件。

EPOLLIN事件：
EPOLLIN事件则只有当对端有数据写入时才会触发，所以触发一次后需要不断读取所有数据直到读完EAGAIN为止。否则剩下的数据只有在下次对端有写入时才能一起取出来了。
现在明白为什么说epoll必须要求异步socket了吧？如果同步socket，而且要求读完所有数据，那么最终就会在堵死在阻塞里。

对照uno/_epoll.c理解：`AcceptConn()`函数中先通过eventset将recvdata事件初始化，然后通过eventadd添加注册EPOLLIN事件。当能读的时候就会激发EPOLLIN事件，同时在主函数中通过EPOLL_WAIT返回判断其为IN，就激活recvdata回调函数。

执行RecvData时，先删除该句柄的已绑定事件，然后读取，如果没有读完，设置sendata回调函数，添加注册EPOLLOUT事件，等待事件触发。同时在senddata中也设置了recvdata回调函数，同时注册EPOLLIN事件。直到读/写为0为止。

---

## UDP

### 基本udp编程函数图

![](https://ww2.sinaimg.cn/large/006tKfTcly1ffh6y03he2j30dp0btmxn.jpg)

​	客户不与服务器建立连接，而是只是用`sendto`函数将数据发送给服务器，其中必须指定目的地（即服务器）的地址作为参数。

​	服务器也不会接受来自客户的连接，只是调用`recvfrom()`函数，等待某个数据的到达，并且将接受的数据一道返回给客户的协议地址，因此服务器可以相应客户。

---

### udpserv.c

```c
int main(int argc,char *argv[])
{
  	...
      
    sockfd = Socket(AF_INET,SOCK_DGRAM,0);
  
	/*bzero,设置addr，port等，然后bind。。。*/
  
    dg_echo(sockfd,(SA *)&cliaddr,sizeof(cliaddr));
}

void dg_echo(int sockfd,SA *pcliaddr,socklen_t clilen)
{
  /*声明一些变量*/
  
  for(;;) {
        len = clilen;
        n = Recvfrom(sockfd,mesg,MAXLINE,0,pcliaddr,&len);
        Sendto(sockfd,mesg,n,0,pcliaddr,len);
    }
}

```

- 该函数永不终止，因为UDP是无连接，没有TCP中的EOF等。
- 提供迭代服务器，单个服务器程序需要处理所有用户、大多数TCP是并发的，而大多数UDP是迭代的。TCP可以较容易的并发的原因在于其每个客户连接是有一个独立的已连接套接字。而UDP只有一个套接字和所有的客户端通信。因此无法简单的绑定一个客户为其服务。
- UDP套接字中隐含有排队发生。并且每个UDP套接字都是有一个接收缓冲区。当进程调用recvfrom时，缓冲区中下一个数据包以FIFO方式返回给进程。

---

## 其他相关

### 重入和不可重入相关函数

gethostbyname、gethostbyaddr、getservbyname和getservbyport四个函数不可重入。函数名后为_r的是可重入版本。

inet_pton和inet_ntop是可重入的。

inet_ntoa是不可重入的。

---

### 守护进程

- 不与控制终端相连。所以也就无法使用fprintf将消息发送到stderr上.等级消息也就使用syslog函数。
- 守护进程肯定是后台进程，但不同的是，daemon必须自己是会话组长，脱离父进程。后台进程的文件描述符也可以是继承于父进程，例如shell，所以它也可以在当前终端下显示输出数据。但是daemon进程自己变成了进程组长，其文件描述符号和控制终端没有关联，是控制台无关的。

#### daemon_init

```c
extern int daeon_proc; //非0时，在unp中定义的err_xxx函数中就会使用syslog。
int daemon_init(const char *name,int facility) {
  ...;//some para
  /*first fork and parent exit*/
  if((pid = fork()) < 0) return (-1);
  else if(pid) _exit(0);
  
  /*child 1*/
  setsid(); //become session leader
 //必须忽略SIGHUP，因为当会话头进程结束(child 1 terminal)，其会话中所有的进程都会受到SIGHUP信号。而SIGHUP的默认动作是终止。
  signal(SIGHUP,sig_ign); 
  
  /*second fork and parent(child 1) exit*/
  if((pid = fork()) < 0) return (-1);
  else if(pid) _exit(0);
  
  /*child 2*/
  daemon_proc = 1;
  chdir("/"); //change working dir to /
  //close all 继承过来的文件描述符，这些描述符通常是从执行他的进程(通常是shell)中继承。
  for(i = 0;i < MAXFD;i++)
    close(i);
  
  //将是特定，stdout，stderr重定向到0，1，2
  //open返回的是最小可用的描述符，上面已经关闭了所有的描述符，所以第一个是0(stdin)，然后依次是1，2
  open("/dev/null",O_RDONLY);
  open("/dev/null",O_RDWR);
  open("/dev/null",O_RDWR);
  
  openlog(pname,LOG_PID,facility);
  return (0;)
}
```

----

## 高级I/O

### 设置超时

1. alarm，指定在超时期满时产生SIGALRM信号，该信号默认终止调用该信号的程序（errno=EINTR），设计信号处理，也可能干扰现有的alarm调用.多线程化程序中正确使用信号非常困难，所以比较适合单线程或未线程化的程序。

2. select中阻塞等待条件发生（或者select中间三个参数设置为NULL即可实现一个比alarm更精确的定时器）。

3. SO_RCVTIMEO和SO_SNDTIMEO套接字选项.

   ```c
   struct timeval tv;
   setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
   ```

### recv&send

```c
ssize_t recv(int socfd,void *buff,size_t nbytes,int flags);
ssize_t send(int socfd,const void *buff,size_t nbytes,int flags);
```

前三个参数等同于read和write，flags为0或者一些定义的常量值的逻辑与。

### recvmsg&sendmsg

最通用的两个函数，比如所有的read、readv、recv和recvfrom替换成recvmsg。

```c
ssize_t recvmsg(int sockfd,struct msghdr *msg,int flags);
ssize_t sendmsg(int sockfd,struct msghdr *msg,int flags);

struct msghdr {
  void 			*msg_name; 		
  socklen_t  	msg_namelen;
  struct iovec  *msg_iov;
  int 			msg_iovlen;
  void 			*msg_control
  socklen_t 	msg_controllen;
  int 			msg_flags;
};
```

---

## 非阻塞I/O

### 可能阻塞的套接字调用

1. 输入操作：read、readv、recv、recvfrom和recvmsg。
2. 输出操作：write、writev、send、sendto和sendmsg。
3. 接受外来连接：即accept函数。对阻塞的套接字调用accept，会阻塞程序；对非阻塞直接调用accept，会返回EWOULDBLOCK错误。
4. 发去外出连接：即connect函数。回忆在UDP中connect只是使得内核保存对端的端口和ip。如果对非阻塞TCP调用connect并且**连接不能立即建立**，连接的建立照样可以发起（比如三路握手的第一个分组），不过会返回EINPROGRESS错误。注意，当服务和客户在同一台主机的情况下连接可以立即建立。

### 非阻塞的读和写:参看strclinonb.c。

可以看到这个文件相较于之前的str_cli复杂了许多，因为要避免使用标准I/O并且需要多缓冲区进行更复杂的管理和处理。

当需要使用非阻塞式I/O时，更简单的方法是将程序划分到多个进程（使用fork）或者多个线程。

```c
//fork版本
void str_cli(FILE *fp,int sockfd) {
  pid_t 		pid;
  char 			sendline[MAXLINE],recvline[MAXLINE];
  if((pid = fork()) == 0) {
    while(readline(sockfd,recvline,MAXLINE) > 0)
      fputs(recvline,stdout);
    /* kill(pid_t,int sig)
    *  pid > 0,信号发往该pid
    *  pid = 0，发送至调用kill的相同组的进程
    *  pid = -1，发送信号给所以该进程可以发送信号的进程
    *  pid < -1，发送信号给-pid为组标识的进程。
    */
    kill(getppid(),SIGTERM);//子进程结束后显示的向父进程发送SIGTERM信号，防止其还在运行；另一种是子进程自己结束后，使得父进程（还在运行话）捕获一个SIGCHILD。
    exit(0);
  }
  
  while(fgets(sendline,MAXLINE,fp) != null)
    writen(sockfd,sendline,strlen(sendline));
  shutdown(sockfd,SHUT_WR);
  //父进程完成数据的复制后，进入睡眠，知道收到SIGTERM，该信号默认终止进程。
  pause();
  return;
}
```

TCP是全双工的，父子进程共享同一个套接字：父进程写，子进程读。

---

### 非阻塞connect

非阻塞套接字调用connect会返回一个EINPROGRESS错误，不过已经发起的三路握手继续进行。使用select检测这个连接成功或者失败的已建立条件。

- 三路握手可以叠加在其他处理上。
- 使用该技术同时建立多个连接，随着web浏览器变得流行起来
- 使用select等到连接的建立，可以给connect指定一个时间限制，缩短connect的超时。

处理细节：

- 连接的服务器如果在本机上，connect是立即建立连接的，必须处理这种情况
- 当连接成功建立，描述符变为可写；连接建立错误时，描述符变为即可写又可读

---

### IOCTL

一些不适合归入其他精细定义类别的特性的系统接口。在网络程序中经常在程序启动执行后使用ioctl获取所在主机全部网络接口的信息：接口地址，是否支持广播，是否支持多播等等。

```c
unistd.h
int ioctl(int fd,int requset,.../*void *arg*/);
```

第三个参数总是指针，依赖于request。request是定义为各种的宏，比如SIOCGIIFCONF表示获取所有接口的列表，arg为struct ifconf类型的指针用于存储返回的值。

ARP高速缓存操作也是使用ioctl操作。

---

## 广播和多播

### 广播（broadcasting）

实际上tcp只支持单播寻址：一个进程就与另一个进程通信。UDP是支持多播、广播等其他类型的。

用途1：在本地子网定位一个服务器主机。确定该主机在子网，但不知带其单播ip。成为资源发现

用途2：在有多个客户主机与单个服务器主机通信的局域网环境中尽量减少分组流通。比如：

- ARP。使用链路层进行广播而不是ip层
- DHCP。
- NTP（网络时间协议）
- 路由守护进程

需要显示的设置SO_BROADCAST套接字选项来高速内核发送广播数据报

---

多播在ipv4中的支持是可选的。同时一个主机发送一个多播分组，对他感兴趣的主机才接收该分组。

---

## 线程相关

![](https://ww3.sinaimg.cn/large/006tKfTcly1ffpsak4so4j30n10gp0vy.jpg)

源文件：caps/echo/echoclient.c

注意上图 红框中如果改为

```c
connfd = accept(listefd,cliaddr,&len);
pthread_create(&tid,null,thread,&connfdp);//不可行
pthread_create(&tid,null,thread,(void *)connfdp);//可行
```

- 在第一个create中不会起作用。不能直接将**地址**传给函数。在accept中返回一个描述符（比如5），然后pthread创建线程，准备执行thread函数；然后accept又收到了另一个连接就绪返回了新描述符（比如6）。这样虽然创建了两个线程，因为是传入的地址，所以两个线程操作同一个存放在connfd中的值。而这两个线程不是同步的访问这个共享变量的。
- 在第二个create中，传入的是connfdp的值，而不是指向该变量的一个指针。按照c向被调用函数传递整数值的方式（吧该值的一个副本推入被调用函数的栈中）。
- 更好的办法是图中，每次申请新空间分配内存用于存放各自的描述符。

### 互斥锁提供互斥机制，条件变量提供信号机制。



## 杂记

### socket传输结构体

由于Socket只能发送字符串，所以可以**使用发送字符串的方式发送文件、结构体、数字**等等.

**1.memcpy**

　　Copy block of memory。内存块拷贝函数，该函数是标准库函数，可以进行二进制拷贝数据。

　　函数原型： void * memcpy ( void * destination, const void * source, size_t num );

　　函数说明：从source指向的地址开始拷贝num个字节到以destination开始的地址。其中destination与source指向的数据类型无关。

**2.Socket传输**

　　**使用memcpy将文件、结构体、数字等，可以转换为char数组，之后进行传输，接收方在使用memcpy将char数组转换为相应的数据。**



服务端代码：

```c
serv:

#include<netinet/in.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

#define HELLO_WORLD_SERVER_PORT    6666
#define LENGTH_OF_LISTEN_QUEUE     20
#define BUFFER_SIZE                1024

typedef struct
{
    int ab;
    int num[1000000];
}Node;

int main(int argc, char **argv)
{
    // set socket's address information
    struct sockaddr_in   server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htons(INADDR_ANY);
    server_addr.sin_port = htons(HELLO_WORLD_SERVER_PORT);

    // create a stream socket
    int server_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        printf("Create Socket Failed!\n");
        exit(1);
    }

    //bind
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)))
    {
        printf("Server Bind Port: %d Failed!\n", HELLO_WORLD_SERVER_PORT);
        exit(1);
    }

    // listen
    if (listen(server_socket, LENGTH_OF_LISTEN_QUEUE))
    {
        printf("Server Listen Failed!\n");
        exit(1);
    }

    while(1)
    {
        struct sockaddr_in client_addr;
        socklen_t          length = sizeof(client_addr);

        int new_server_socket = accept(server_socket, (struct sockaddr*)&client_addr, &length);
        if (new_server_socket < 0)
        {
            printf("Server Accept Failed!\n");
            break;
        }

        Node *myNode=(Node*)malloc(sizeof(Node));

        int needRecv=sizeof(Node);
        char *buffer=(char*)malloc(needRecv);
        int pos=0;
        int len;
        while(pos < needRecv)
        {
            len = recv(new_server_socket, buffer+pos, BUFFER_SIZE, 0);
            if (len < 0)
            {
                printf("Server Recieve Data Failed!\n");
                break;
            }
            pos+=len;

        }
        close(new_server_socket);
        memcpy(myNode,buffer,needRecv);
        printf("recv over ab=%d num[0]=%d num[999999]=%d\n",myNode->ab,myNode->num[0],myNode->num[999999]);
        free(buffer);
        free(myNode);
    }
    close(server_socket);

    return 0;
}
```

客户端代码：

```c
cli:
#include <sys/types.h>
#include <sys/socket.h>                         // 包含套接字函数库
#include <stdio.h>
#include <netinet/in.h>                         // 包含AF_INET相关结构
#include <arpa/inet.h>                      // 包含AF_INET相关操作的函数
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <pthread.h>

#define MYPORT  6666
#define BUFFER_SIZE 1024

typedef struct
{
    int ab;
    int num[1000000];
}Node;

int main()
{
        ///sockfd
    int sock_cli = socket(AF_INET,SOCK_STREAM, 0);

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
       servaddr.sin_port = htons(MYPORT);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock_cli, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("connect");
        exit(1);
    }

    Node *myNode=(Node*)malloc(sizeof(Node));
    myNode->ab=123;
    myNode->num[0]=110;
    myNode->num[999999]=99;

    int needSend=sizeof(Node);
    char *buffer=(char*)malloc(needSend);
    memcpy(buffer,myNode,needSend);

    int pos=0;
    int len=0;
    while(pos < needSend)
    {
        len=send(sock_cli, buffer+pos, BUFFER_SIZE,0);
        if(len <= 0)
        {
            perror("ERRPR");
            break;
        }
        pos+=len;
    }
    free(buffer);
    free(myNode);
    close(sock_cli);
    printf("Send over!!!\n");
    return 0;
}
```

运行结果：

![](https://ws4.sinaimg.cn/large/006tNc79ly1fh7x8tzzuhj3096014mx2.jpg)

