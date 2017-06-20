# linux内核学习笔记

## 进程的管理和调度

### 进程管理

#### 进程

进程应该包括可执行代码，挂起的信号，内核的内部数据，处理器状态以及内存地址空间，一个或者多个执行线程，全局的数据段等等。

#### 线程

每个线程拥有独立的计数器、进程栈和一组寄存器。

内核调度的是线程而不是进程。linux内核中对线程和进程并不特别区分。线程仅仅被视为一个与其他进程共享某些资源（比如地址空间，虚拟（全局）内存，堆，文件描述符等等）的进程。

内核线程与普通线程的区别在于内核线程没有自己独立的地址空间。只在内核空间运行。

线程之间可以共享**虚拟内存**，但是有自己独立的**虚拟处理器**。

#### 进程描述符及任务结构

进程的列表放在任务队列（task list），是一个双向循环列表，表项为task_struct（成为进程描述符）的结构。

进程描述符完整地描述一个正在执行的程序：打开的文件，进程的地址空间，挂起的信号，进程的状态以及其他信息。

分配时，通过slab分配器分配task_struct结构，只需要在栈低（或者顶）创建一个新的struct thread_info结构（使用该结构用于在一些寄存器较弱的结构中以及计算偏移量非常方便），在thread_info中有task指向实际的task_struct结构。

在内核中，通过一个唯一的进程标示值（pid）标识进程。通过current宏定义查找正在运行进程的描述符。

#### 进程状态

![](https://ws4.sinaimg.cn/large/006tKfTcly1fgqsxp2vibj30hu0azgmg.jpg)

- TASK_RUNNING(运行)：可执行，或正在执行，或在运行队列中等待执行
- TASK_INTERRUPTIBLE(可中断)：正在睡眠（被阻塞），条件达成或者收到信号即可唤醒
- TASK_UNINTERRUPTIBLE(不可中断)：就算接收到信号也不会被唤醒
- __TASK_TRACED:被其他进程追踪
- __TASK_STOPPED:停止状态。收到SIGSTOP,SIGTSTP,SIGTTIN,SIGTTOU等信号时发生。或者调试期间收到信号。

#### 进程创建（`fork()`）

fork使用写时拷贝页实现，实际开销为复制父进程的页表以及给子进程创建唯一的进程描述符。

linux中使用`clone()`系统调用实现fork。

`fork()`——>`clone()`——>`do_fork()`——>`copy_process()`—>`....others`。

copy_process创建了新进场的内核栈、thread_info和task_struct等结构。拷贝或共享打开的文件，文件系统信息，信号处理函数，进程地址空间和命名空间等等。子进程刚创建的时候状态为UNINTERRUPTIBLE，确保不会被运行。指导函数成功返回才使其唤醒。

---

### 进程调度

linux调度器以模块方式提供，允许不同类型的进程可以有针对地选择调度算法。

CFS：（完全公平调度）针对普通进程的调度算法。允许每个进程运行一段时间，循环轮转，选择时间最小的进程作为下一个运行进程，而不再是采用分配给每个进程时间片 ，nice值作为进程获得处理器运行比的权重。只有相对nice值才会影响处理器时间的分配。

#### linux调度的实现

- 时间记账：使用vruntime对运行时间记账，系统定时调用update_curr()更新进程的runtime，这样可以准确知道进程的运行时间，然后就知道了下一个被运行的进程
- 进程选择：挑选具有最小vruntime的进程。使用红黑树结构记录进程的vruntime，通常将最小值缓存在变量中。
- 调度器入口：函数`schedule`，调用`pick_next_task()`选择最高优先级的调度类。
- 睡眠和唤醒：进程阻塞，将自己标记为休眠状态，然后从可执行红黑树中移除，调用schedule选择和执行下一个其他进程。

#### 抢占和上下文切换

上下文切换，从一个可执行进程切换到另一个可执行进程。需要映射虚拟内存到新进程，然后切换处理器状态，保存、恢复栈信息和寄存器信息等等。

用户抢占

返回用户空间的时候，如果need_reshed被设置，会触发schedule函数，就会发生用户抢占，也就是另一个用户抢占了本该返回继续执行的进程。发生在：

- 系统调用返回用户空间
- 中断处理返回用户空间

内核抢占

只要没有持有锁，内核都可以抢占。发生在：

- 中断处理正在执行，且在返回内核空间之前
- 内核代码再一次具有抢占性
- 内核中的任务显式的调用schedule
- 任务阻塞（这也会导致调用schedule）

#### 实时调度策略

`SCHED_FIFO`:先入先出，不存在时间片概念

`SCHED_RR`：带有时间片的先入先出，时间陪用完就不会执行。

---

## 内核数据结构

### 链表

#### 链表结构

内核中的标准链表是环型双向链表

```c
//普通链表结构
struct fox {
  int a;
  int b;
  struct fox *next;
  struct fox *pre;
}

//内核链表
struct list_head{
  struct list_head *next;
  struct list_head *pre;
}
struct fox {
  int a;
  int b;
  struct list_head list;
}
```

将链表结构插入到数据结构中，而不是在数据结构中嵌入链表指针！

而如何获取父结构中的变量，则是使用`container_of()`宏定义，因为给定结构中的偏移量在编译时候就已经固定下来了：

```c
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)  
//将0x0地址强制转换为TYPE*类型，然后取TYPE中的成员MEMBER地址，因为起始地址为0，得到的MEMBER的地址就直接是该成员相对于TYPE对象的偏移地址了

#define container_of(ptr,type,member)({\
	const typeof( ((type *)0)->member) *__mptr = (ptr);\
	(type *)((char *)__mptr - offsetof(type,member);})
//const typeof(((type *)0)->member)*__mptr = (ptr); 首先将0x0转换为TYPE类型的指针变量，再引用member成员，typeof(x)返回的是x的数据类型，所以typeof(((type *)0)->member)返回的就是member成员的数据类型，该语句就是将__mptr强制转换为member成员的数据类型，然后再将ptr赋给它。ptr本来就是指向member的指针；
//说白了，就是定义一个member成员类型的指针变量，然后将ptr赋给它。

//下一句：先将__mptr强制转换为char*类型（因为char* 类型进行加减的话，加减量为sizeof(char)*offset，char占一个字节空间，这样指针加减的步长就是1个字节，实现加一减一。）实现地址的精确定位，如果不这样的话，假如原始数据类型是int，那么这减去offset，实际上就是减去sizeof(int *)*offset 的大小了。
//所以整体意思就是：得到指向type的指针，已知成员的地址，然后减去这个成员相对于整个结构对象的地址偏移量，不就得到这个数据对象的地址么

#define list_entry(ptr,type,member) \
	container_of(ptr,type,member)
//总之：该宏定义返回ptr对应链表节点的对象的地址。ptr是member类型的指针
```

这样，依靠list_entry()，内核提供了创建、操作以及其他链表管理的各种例程--并且不需要知道list_head所嵌入的父结构的数据结构。

最杰出的特性是：每一个节点都包含一个list_head指针，那么可以从任何一个节点开始遍历链表，知道我们看到所有的节点。

不过一般使用`static LIST_HEAD(fox_list)`来定义并初始化名为fox_list的链表例程。

#### 链表操作

遍历所有对象：

```c
#define list_for_each_entry(pos, head, member)              \  
for (pos = list_entry((head)->next, typeof(*pos), member);   \  
    prefetch(pos->member.next), &pos->member != (head);   \  
    pos = list_entry(pos->member.next, typeof(*pos), member))  
/* 
实际上就是一个for循环，循环内部由用户自己根据需要定义 
初始条件：pos = list_entry((head)->next, typeof(*pos), member); pos为第一个对象地址 
终止条件：&pos->member != (head);pos的链表成员不为头指针，环状指针的遍历终止判断 
递增状态：pos = list_entry(pos->member.next, typeof(*pos), member) pos为下一个对象地址 
所以pos就把挂载在链表上的所有对象都给遍历完了。至于访问对象内的数据成员，有多少个，用来干嘛用，则由用户根据自己需求来定义了。 pos就是每个链表成员的对象。
*/  
```

添加元素

```c
//head是头指针  
//头节点后添加元素，相当于添加头节点，可实现栈的添加元素  
//new添加到head之后，head->next之前  
static inline void list_add(struct list_head *new, struct list_head *head)  
{  
    __list_add(new, head, head->next);  
}  
//头节点前添加元素，因为是环状链表所以相当于添加尾节点，可实现队列的添加元素  
//new添加到head->prev之后，head之前  
static inline void list_add_tail(struct list_head *new, struct list_head *head)  
{  
    __list_add(new, head->prev, head);  
}  
static inline void __list_add(struct list_head *new,  
                              struct list_head *prev,  
                              struct list_head *next)  
{  
    next->prev = new;  
    new->next = next;  
    new->prev = prev;  
    prev->next = new;  
}  
```

删除元素

```c
//prev和next两个链表指针构成双向链表关系  
static inline void __list_del(struct list_head * prev, struct list_head * next)  
{  
    next->prev = prev;  
    prev->next = next;  
}  
//删除指定元素，这是一种安全删除方法，最后添加了初始化,这样做的目的，因为虽然不在需要entry项，但是还可以再次使用包含entry项的数据结构体
static inline void list_del_init(struct list_head *entry)  
{  
    //删除entry节点  
    __list_del(entry->prev, entry->next);  
    //entry节点初始化，是该节点形成一个只有entry元素的双向链表  
    INIT_LIST_HEAD(entry);  
}  
```

替换元素

```c
//调整指针，使new的前后指针指向old的前后，反过来old的前后指针也指向new  
//就是双向链表的指针调整  
static inline void list_replace(struct list_head *old,  
                                struct list_head *new)  
{  
    new->next = old->next;  
    new->next->prev = new;  
    new->prev = old->prev;  
    new->prev->next = new;  
}
//替换元素，最后将old初始化一下，不然old还是指向原来的两个前后节点（虽然人家不指向他）  
static inline void list_replace_init(struct list_head *old,  
                                     struct list_head *new)  
{  
    list_replace(old, new);  
    INIT_LIST_HEAD(old);  
}  
```

移动元素

```c
//移动指定元素list到链表的头部  
static inline void list_move(struct list_head *list, struct list_head *head)  
{  
    __list_del(list->prev, list->next);//删除list元素  
    list_add(list, head);//将list节点添加到head头节点  
}  
/** 
* list_move_tail - delete from one list and add as another's tail 
* @list: the entry to move 
* @head: the head that will follow our entry 
*/  
//移动指定元素list到链表的尾部  
static inline void list_move_tail(struct list_head *list,  
                                  struct list_head *head)  
{  
    __list_del(list->prev, list->next);  
    list_add_tail(list, head);  
}  
```

---

### 队列

linux中通用队列成为`kfifo`。

in表示入队一些数据后当前的队列所在偏移量，使用unsigned，那么在超出了buff的边界后会从0开始，即是从buff的开始边界继续存放。

```c
//kfifo结构定义
struct kfifo {
    unsigned char *buffer;     /* the buffer holding the data */
    unsigned int size;         /* the size of the allocated buffer */
  	unsigned int in;           /* data is added at offset (in % size) */
    unsigned int out;          /* data is extracted from off. (out % size) */
    spinlock_t *lock;          /* protects concurrent modifications */
};
```

主要的put和get操作实现：

在kfifo中，传入的size总是2的幂，因为这样取模运算就可以转化为与运算，`kfifo->in % kfifo->size` 可以转化为 `kfifo->in & (kfifo->size – 1)`

超出队头队尾之间长度的数据放到整个队列buffer的开始处.

```c
unsigned int__kfifo_put(struct kfifo *fifo,    
              unsigned char *buffer, unsigned int len)    
  {    
      unsignedint l;
  	//最多还能存放len长度的数据。
      len =min(len, fifo->size - fifo->in + fifo->out);         
      /*  
       * Ensurethat we sample the fifo->out index -before- we  
       * startputting bytes into the kfifo.  
       */    
     smp_mb();    
      /* firstput the data starting from fifo->in to buffer end */    
      l =min(len, fifo->size - (fifo->in & (fifo->size - 1)));    
     memcpy(fifo->buffer + (fifo->in & (fifo->size - 1)),buffer, l);     
      /* then putthe rest (if any) at the beginning of the buffer */    
     memcpy(fifo->buffer, buffer + l, len - l);       
      /*  
       * Ensurethat we add the bytes to the kfifo -before-  
       * weupdate the fifo->in index.  
       */        
     smp_wmb();         
      fifo->in+= len;   //每次累加，到达最大值后溢出，自动转为0     
     returnlen;     
  }   
    
  unsigned int__kfifo_get(struct kfifo *fifo,    
  
              unsigned char *buffer, unsigned int len)    
  
 {    
     unsignedint l;    
      len =min(len, fifo->in - fifo->out);    
      /*  
       * Ensurethat we sample the fifo->in index -before- we  
       * startremoving bytes from the kfifo.  
       */    
     smp_rmb();         
      /* firstget the data from fifo->out until the end of the buffer */    
      l =min(len, fifo->size - (fifo->out & (fifo->size - 1)));    
     memcpy(buffer, fifo->buffer + (fifo->out & (fifo->size -1)), l);        
      /* then getthe rest (if any) from the beginning of the buffer */    
     memcpy(buffer + l, fifo->buffer, len - l);    
     /*  
       * Ensurethat we remove the bytes from the kfifo -before-  
       * weupdate the fifo->out index.  
       */         
     smp_mb();         
    fifo->out += len;   //每次累加，到达最大值后溢出，自动转为0       
      returnlen;    
  }    
```

### 红黑树（待完成）



