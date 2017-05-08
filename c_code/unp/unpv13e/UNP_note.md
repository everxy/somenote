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

