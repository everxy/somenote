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

### 网络请求的使用情况

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

第二步：**epoll_ctl()**系统调用。通过此调用向epoll对象中添加、删除、修改感兴趣的事件，返回0标识成功，返回-1表示失败。

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

