# linux内核学习笔记

## 进程的管理和调度

### 进程管理

#### 进程

进程应该包括可执行代码，挂起的信号，内核的内部数据，处理器状态以及内存地址空间，一个或者多个执行线程，全局的数据段等等。

#### 线程

每个线程拥有独立的计数器、进程栈和一组寄存器值（寄存器本身肯定是所有进程公有，才会有切换上下文）。

内核调度的是线程而不是进程。linux内核中对线程和进程并不特别区分。线程仅仅被视为一个与其他进程共享某些资源（比如用户态地址空间—页表，虚拟（全局）内存，堆，文件描述符—打开文件表项，信号处理等等）的进程。

内核线程与普通线程的区别在于内核线程没有自己独立的地址空间。只在内核空间运行。

线程之间可以共享**虚拟内存**，但是有自己独立的**虚拟处理器**。

#### 进程描述符及任务结构

进程的列表放在任务队列（task list），是一个双向循环列表，表项为`task_struct`（称为进程描述符）的结构。

![](https://ws2.sinaimg.cn/large/006tNc79ly1fh39vhju8nj30ez0f5ab1.jpg)



进程描述符完整地描述一个正在执行的程序：打开的文件，进程的地址空间，挂起的信号，进程的状态以及其他信息。

**参考链接：http://blog.csdn.net/unclerunning/article/details/51246749**。

分配时，通过slab分配器分配task_struct结构，只需要在栈低（或者顶）创建一个新的`struct thread_info`结构（使用该结构用于在一些寄存器较弱的结构中以及计算偏移量非常方便），在thread_info中有`task指针`指向实际的task_struct结构，它相对于thread_info的偏移量为0。在访问任务一般是需要访问`task_struct`的地址，所以需要`current`宏（本质上是`current_thread_info()->task`）查找当前运行进程的描述符。

esp寄存器是栈指针，存放栈底（栈增长方向）单元的地址，thread_info通常在栈区开始处，所以通过将esp的低地址位屏蔽，可以方便地获取thread_info的地址—>current——>获取到task_struct地址。

在内核中，通过一个唯一的进程标示值（pid）标识进程。通过current宏定义查找正在运行进程的描述符。

#### 进程状态

![](https://ws4.sinaimg.cn/large/006tKfTcly1fgqsxp2vibj30hu0azgmg.jpg)

- TASK_RUNNING(运行)：可执行，或正在执行，或在运行队列中等待执行
- TASK_INTERRUPTIBLE(可中断)：正在睡眠（被阻塞），条件达成或者收到信号即可唤醒
- TASK_UNINTERRUPTIBLE(不可中断)：就算接收到信号也不会被唤醒
- __TASK_TRACED:被其他进程追踪
- __TASK_STOPPED:停止状态。收到SIGSTOP,SIGTSTP,SIGTTIN,SIGTTOU等信号时发生。或者调试期间收到信号。

对于不同状态的进程通过链表进行组织。比如运行队列链表连接所有的可运行进程。

#### 进程创建（`fork()`）

fork使用写时拷贝页实现，实际开销为复制父进程的页表以及给子进程创建唯一的进程描述符。

linux中使用`clone()`系统调用实现fork。

`fork()`——>`clone()`——>`do_fork()`——>`copy_process()`—>`....others`。

clone隐藏了实际系统调用，需要参数为`fn(新进程执行函数)，arg(fn参数),flags(各种信息标志，如果通过某些标志来表明共享的资源就是创建线程)，child_stack(表示将用户态堆栈指针赋值给子进程的esp寄存器)等等`，然后调用dofork函数。

do_fork：需要`clone_flags,stack_start,regs（通用寄存器指针，从用户态到内核态时保存在内核态堆栈）等等`，然后调用copy_process来创建进程描述符和其他的一些数据结构。

copy_process创建了新进场的内核栈、thread_info和task_struct等结构。拷贝或共享打开的文件，文件系统信息，信号处理函数，进程地址空间和命名空间等等。子进程刚创建的时候状态为UNINTERRUPTIBLE，确保不会被运行。指导函数成功返回才使其唤醒。

---

### 进程调度

linux调度器以模块方式提供，允许不同类型的进程可以有针对地选择调度算法。调度中用到的数据结构是`runqueue`，其最重要的是有一个指向可运行进程的链表的相关字段。

```c
prio_array_t *active 	指向活动进程链表的指针
prio_array_t *expired   指向过期进程链表的指针
```



CFS：（完全公平调度）针对普通进程的调度算法。允许每个进程运行一段时间，循环轮转，选择时间最小的进程作为下一个运行进程，而不再是采用分配给每个进程时间片 ，nice值作为进程获得处理器运行比的权重。只有相对nice值才会影响处理器时间的分配。

#### linux调度的实现

- 时间记账：使用vruntime对运行时间记账，系统定时调用update_curr()更新进程的runtime，这样可以准确知道进程的运行时间，然后就知道了下一个被运行的进程
- 进程选择：挑选具有最小vruntime的进程。使用红黑树结构记录进程的vruntime，通常将最小值缓存在变量中。
- 调度器入口：函数`schedule`，调用`pick_next_task()`选择最高优先级的调度类。
- 睡眠和唤醒：进程阻塞，将自己标记为休眠状态，然后从可执行红黑树中移除，调用schedule选择和执行下一个其他进程。

#### 抢占和上下文切换

上下文切换，从一个可执行进程切换到另一个可执行进程。需要映射虚拟内存到新进程，然后切换处理器状态，保存、恢复栈信息（esp寄存器保存用户态堆栈指针）和寄存器信息等等。将所有的寄存器内容保存在内核态堆栈上（硬件上下文切换）。

进程的切换发生在精确定义的点：`schedule()`函数

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
//总之：该宏定义返回ptr对应链表节点的对象的基地址。ptr是member类型的指针

为了帮助理解，设有如下结构体定义： 
typedef struct xxx 
{ 
……（结构体中其他域） 
type1 member; 
……（结构体中其他域） 
}type; 
定义变量： 
type a; 
type * b; 
type1 * ptr; 
执行： 
ptr=&(a.member); 
b=list_entry(ptr,type,member); 
则可使b指向a，得到了a的地址。
```

这样，依靠list_entry()，内核提供了创建、操作以及其他链表管理的各种例程--并且不需要知道list_head所嵌入的父结构的数据结构。

最杰出的特性是：每一个节点都包含一个list_head指针，那么可以从任何一个节点开始遍历链表，知道我们看到所有的节点。*（另外：判断一个链表是否有环，使用快慢指针）。*

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

### HASH

在有些情况下，内核需要通过PID中迅速找出对应的进程描述符指针，而顺序扫描链表获取PID可能比较低效，故而引入了4个hash表

```c
enum pid_type
{
    PIDTYPE_PID,  //对应进程的PID
    PIDTYPE_TGID, //对应线程组领头进程的PID
    PIDTYPE_PGID,  //对应进程组领头进程的PID
    PIDTYPE_SID   //对应会话领头进程的PID
};

用pid_hashfn宏可以把pid转化为表索引： 
#define pid_hashfn(pid) hash_long((unsigned long)pid, pidhash_shift)
  
//../include/linux/list.h

//hash桶的头结点
struct hlist_head {
    struct hlist_node *first;//指向每一个hash桶的第一个结点的指针
};

//hash桶的普通结点
struct hlist_node {
    struct hlist_node *next;//指向下一个结点的指针
    struct hlist_node **pprev;//指向上一个结点的next指针的地址，下面会解释为什么要用一个二级指针。
};
这些点的宿主是进程的PCB（进程控制块），所以对这些点的操作等价于对PCB的操作，找到了这些点，也就找到了宿主。所以接下来的任务就是将这些点散列到对应的桶。
  
为什么要用一个二级指针pprev：

散列数组中存放的是hlist_head结构体的指针。hlist_head结构体只有一个域，即first。 first指针指向一个hlist_node的双向链表中的一个结点(因为是双向链表，所以不用考虑谁是第一的问题)。 
hlist_node结构体有两个域，next 和pprev。 next指针指向下个hlist_node结点，倘若该节点是链表的最后一个节点，next指向NULL。 
pprev是一个二级指针， 它指向前一个节点的next指针的地址。显然，如果hlist_node采用传统的next,prev指针(一级指针)，对于第一个节点和后面其他节点的处理会不一致。hlist_node巧妙地用pprev指向上一个节点的next指针的地址，由于hlist_head的first域和hlist_node的next域的类型都是hlist_node *，这样就解决了通用性问题！之于为什么不全部用hlist_node而非要定义一个专门的只含有一个hlist_node * 域的hlist_head结构体，答案当然是节省内存，而且还不失通用与优雅性。
```

![](https://ws3.sinaimg.cn/large/006tNc79ly1fh3bps5ysaj30km0hd3zv.jpg)

```c
//../include/linux/pid.h

struct pid
{
    /* Try to keep pid_chain in the same cacheline as nr for find_pid */
    int nr;//xid值，用来计算散列值，找到散列入口。

    struct hlist_node pid_chain;//对于同一类型的ID，如果其值不相同，而又有同样的散列值，这时就出现了散列冲突，需要维持一个开放的链表，又名开散列。

    /* list of pids with the same nr, only one of them is in the hash */
    struct list_head pid_list;//对于属于同一个进程组的进程，或属于同一个线程组的进程，它们的组ID是一样的，所以尽管PID不同，它们在对组ID进行计算时得到的散列值是一样的，这时需要为这些进程维持一个双向链表。注意，链表中只把`代表PCB`看做被散列的PCB，其他的PCB被当作未散列的PCB。
    //(这里说的散列是相对于pid_type的散列)。例如，在用(id,pid_type)查找id对应的PCB时，会返回`代表PCB`,一旦有了这个代表PCB，就可以通过它的pid.pid_list字段对同组进程遍历。

  //pid_chain和pid_list的差别在：
  //pid_chain是散列冲突链表。也就是说nr不同，散列结果相同。
  //pid_list是同组进程链表。它们的nr相同，散列结果自然相同，但这并不是散列冲突。
};
```



----



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
     return len;     
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
      return len;    
  }    
```

### 红黑树

首先对于平衡二叉树的旋转的算法实现

单旋转，k1为k2的左子树。旋转后，k2为k1的右子树。返回父节点

```c
static position singleRotateWithLeft(position k2)
{
  position k1;
  k1 = k2->left;
  k2->left = k1->right;
  k1->right = k2;
  k2->height = MAX(height(k2->left),height(k2->right)) + 1; //空树为-1
  k1->height = MAX(height(k1->left),height(k1->right)) + 1; //空树为-1
  return k1;
}
```

双旋转，就是执行两次单旋转：k1为k3的左子树，k2为k1的右子树；旋转后k1为k2的左子树，k3为k2的右子树

```c
static position doubleRotateWithLeft(position k3)
{
  //k1与k2之间的旋转
  k3->left = singleRotateWithRight(k3->left);
  //k3与k2之间的旋转
  return singleRotateWithLeft(k3);
}
```

红黑树的性质：

- 根是黑色的
- 如果一个节点是红色，那么它的子节点必须是黑色
- null节点是黑色的。
- 从一个节点到NULL的每个路径必须包含相同树木的黑色节点

红黑树的插入

在插入新节点，如果新节点为黑色，那么必定违反了第四条准则，且这样调整会非常难，所以新插入的项目我们必定将其涂成`红色`。

在数据结构与算法分析中介绍了：采用从根节点到插入位置的`自顶向下`调整的方法。

- 如果插入节点的父节点为黑色，直接插入
- 如果插入节点的父节点为红色，切父节点的兄弟节点为黑色（不可能的情况，违反了红黑树的原则4）
- 如果插入节点的父节点为红色，切父节点的兄弟节点为NULL，翻转父节点与祖父节点，使得祖父节点为父节点的右子树。父节点为黑色，祖父节点（翻转后的兄弟节点为）为红色即可。
- 如果插入节点的父节点为红色，切父节点的兄弟节点为红色。自顶向下调整。

自顶向下方法：从根节点的某一个子节点开始，如果我们看到了一个节点X有两个红儿子的时候，那么我们就让X成为红，它的两个儿子成为黑。然后进行适当的旋转。

首先是节点的单旋转方向的判断，根据插入节点的大小判断在左子树还是右子树进行哪种方向的旋转。

```c++
static Rotate(elementype elm,position parent)
{
  if(elm < parent->elm) {
    return parent->left = elm < parent->left->elm ? 
      					  singleRotateWithLeft(parent->left):
    					  singleRotateWithRight(parent->left);
  }
  else {
    return parent->right = elm < parent->right->elm ? 
      					  singleRotateWithLeft(parent->right):
    					  singleRotateWithRight(parent->right);
  }
}
```

然后就是插入过程：

```c
static position x,p,gp,ggp;
static void handleRotate(elementype elm,rbtree T) {
  x->color = red;
  x->left->color = x->right->color = black;
  //需要进行rotate翻转的情况
  if(p->color = red) {
    GP->color = red;
    if((elm < gp->elm) != (elm < p->elm)) //当插入节点位置后需要进行两次旋转的情况
      p = rotate(item,gp);
    x = rotate(item,ggp);
    //一定要将X的颜色涂成黑色！
    x->color = black;
  }
  T->right->color = black;
}

rbtree insert(elementype elm,rbtree T) {
  x = p = gp = ggp = t;
  nullNode->elm = elm;
  while(x->elm != elm) {
    ggp = gp;
    gp = p;
    p = x;
    if(elm < x->elm)
      x = x->left;
    else
      x = x->right;
    if(x->left->color == red && x->right->color == red)
      handleRotate(elm,T);
  }
  if(x != NUll)
    return nullNode;
  x = malloc(sizeof(struct rbNode));
  if(x == null)
    error("out of space");
  x->elm = elm;
  x->left = x->right = NUllNode;
  
  if(item < p->elm)
    p->left = x;
  else
    p->right = x;
  handleRotate(elm,T);
  
  return T;
}
```

#### Linux内核中的红黑树实现

1. 结构体相关定义和一些相关的操作

   ```c
   struct rb_node  
   {  
       unsigned long  rb_parent_color;  
   #define RB_RED      0  
   #define RB_BLACK    1  
       struct rb_node *rb_right;  
       struct rb_node *rb_left;  
   } __attribute__((aligned(sizeof(long))));  
   ```

   这里的巧妙之处是使用成员rb_parent_color同时存储两种数据，一是其双亲结点的地址，另一是此结点的着色。__attribute__((aligned(sizeof(long))))属性保证了红黑树中的每个结点的首地址都是32位对齐的（在32位机上），也就是说每个结点首地址的bit[1]和bit[0]都是0，因此就可以使用bit[0]来存储结点的颜色属性而不干扰到其双亲结点首地址的存储.

   `__attribute__((aligned(sizeof(type))))`aligned表示根据参数的长度进行对齐。GNU C扩展的__attribute__ 机制被用来设置函数、变量、类型的属性，其用得较多的是处理字节对齐的问题

   操作函数

   ```c
   //获得其双亲结点的首地址,3:11,~3:00,1:01,~1:10
   #define rb_parent(r)   ((struct rb_node *)((r)->rb_parent_color & ~3))  

   #define rb_color(r)   ((r)->rb_parent_color & 1) //获得颜色属性  
   #define rb_is_red(r)   (!rb_color(r))   //判断颜色属性是否为红  
   #define rb_is_black(r) rb_color(r) //判断颜色属性是否为黑  
   #define rb_set_red(r)  do { (r)->rb_parent_color &= ~1; } while (0)  //设置红色属性  
   #define rb_set_black(r)  do { (r)->rb_parent_color |= 1; } while (0) //设置黑色属性  
     
   static inline void rb_set_parent(struct rb_node *rb, struct rb_node *p)  //设置其双亲结点首地址的函数  
   {  
       rb->rb_parent_color = (rb->rb_parent_color & 3) | (unsigned long)p;  
   }  
   static inline void rb_set_color(struct rb_node *rb, int color) //设置结点颜色属性的函数  
   {  
       rb->rb_parent_color = (rb->rb_parent_color & ~1) | color;  
   }  
   ```

   初始化新节点

   ```c
   static inline void rb_link_node(struct rb_node * node, struct rb_node * parent,  
                   struct rb_node ** rb_link)  
   {  
       node->rb_parent_color = (unsigned long )parent;   //设置其双亲结点的首地址(根结点的双亲结点为NULL),且颜色属性设为黑色  
       node->rb_left = node->rb_right = NULL;   //初始化新结点的左右子树  
     
       *rb_link = node;  //指向新结点  
   }  
   ```

   指向红黑树根节点的指针

   ```c
   struct rb_root  
   {  
       struct rb_node *rb_node;  
   };  
   #define RB_ROOT (struct rb_root) { NULL, }  //初始化指向红黑树根结点的指针  
   #define rb_entry(ptr, type, member) container_of(ptr, type, member) //用来获得包含struct rb_node的结构体的首地址
   #define RB_EMPTY_ROOT(root) ((root)->rb_node == NULL) //判断树是否为空  
   #define RB_EMPTY_NODE(node) (rb_parent(node) == node)  //判断node的双亲结点是否为自身  
   #define RB_CLEAR_NODE(node) (rb_set_parent(node, node)) //设置双亲结点为自身  
   ```

2. 插入

   ```c
   //以父节点作为祖父节点的左节点和右节点分类判断，然后继续判断插入节点为父节点的左右节点进行分类判断
   void rb_insert_color(struct rb_node *node, struct rb_root *root)  
   {  
       struct rb_node *parent, *gparent;  
     
       while ((parent = rb_parent(node)) && rb_is_red(parent)) //双亲结点不为NULL，且颜色属性为红色  
       {  
           gparent = rb_parent(parent); //获得祖父结点  
     
           if (parent == gparent->rb_left) //双亲结点是祖父结点左子树的根  
           {  
               {  
                   register struct rb_node *uncle = gparent->rb_right; //获得叔父结点  
                   if (uncle && rb_is_red(uncle)) //叔父结点存在，且颜色属性为红色  
                   {  
                       rb_set_black(uncle); //设置叔父结点为黑色  
                       rb_set_black(parent); //设置双亲结点为黑色  
                       rb_set_red(gparent); //设置祖父结点为红色  
                       node = gparent;  //node指向祖父结点，然后继续上浮，判断是否有两个相连的节点都为红色。
                       continue; //继续下一个while循环,后续代码不会继续执行。如果是上升到了根节点，则执行最后一句，将根节点置为黑色即可。
                   }  
               }  
     
               if (parent->rb_right == node)  
                 //当node作为右子树插入，那么讲其左旋转，然后作为左节点插入的情况继续判断。
               {  
                   register struct rb_node *tmp;  
                   __rb_rotate_left(parent, root); //左旋  
                   tmp = parent;  //调整parent和node指针的指向  
                   parent = node;  
                   node = tmp;  
               }  
     			//注意：不可能有父节点为红且uncle节点存在且为黑色的情况，这样的
               rb_set_black(parent); //设置双亲结点为黑色  
               rb_set_red(gparent); //设置祖父结点为红色  
               __rb_rotate_right(gparent, root); //右旋，注意这里是讲祖父节点与父节点节点进行旋转交换位置。  
           } else { // !(parent == gparent->rb_left)  
               {  
                   register struct rb_node *uncle = gparent->rb_left;  
                   if (uncle && rb_is_red(uncle))  
                   {  
                       rb_set_black(uncle);  
                       rb_set_black(parent);  
                       rb_set_red(gparent);  
                       node = gparent;  
                       continue;  
                   }  
               }  
     
               if (parent->rb_left == node)  
               {  
                   register struct rb_node *tmp;  
                   __rb_rotate_right(parent, root);  
                   tmp = parent;  
                   parent = node;  
                   node = tmp;  
               }  
     
               rb_set_black(parent);  
               rb_set_red(gparent);  
               __rb_rotate_left(gparent, root);  
           } //end if (parent == gparent->rb_left)  
       } //end while ((parent = rb_parent(node)) && rb_is_red(parent))  
     
       rb_set_black(root->rb_node);  
   }
   ```

3. 删除节点的操作

   **第一步：将红黑树当作一颗二叉查找树，将节点删除。**
   ​       这和"删除常规二叉查找树中删除节点的方法是一样的"。分3种情况：
   ① 被删除节点没有儿子，即为叶节点。那么，直接将该节点删除就OK了。
   ② 被删除节点只有一个儿子。那么，直接删除该节点，并用该节点的唯一子节点顶替它的位置。
   ③ 被删除节点有两个儿子。那么，先找出它的后继节点；然后把“它的后继节点的内容”复制给“该节点的内容”；之后，删除“它的后继节点”。在这里，后继节点相当于替身，在将后继节点的内容复制给"被删除节点"之后，再将后继节点删除。这样就巧妙的将问题转换为"删除后继节点"的情况了，下面就考虑后继节点。 在"被删除节点"有两个非空子节点的情况下，它的后继节点不可能是双子非空。既然"的后继节点"不可能双子都非空，就意味着"该节点的后继节点"要么没有儿子，要么只有一个儿子。若没有儿子，则按"情况① "进行处理；若只有一个儿子，则按"情况② "进行处理。

   **第二步：通过"旋转和重新着色"等一系列来修正该树，使之重新成为一棵红黑树。**
   因为"第一步"中删除节点之后，可能会违背红黑树的特性。所以需要通过"旋转和重新着色"来修正该树，使之重新成为一棵红黑树。

   ```c
   void rb_erase(struct rb_node *node, struct rb_root *root)  
   {  
       struct rb_node *child, *parent;  
       int color;  
     
       if (!node->rb_left) //删除结点无左子树  
           child = node->rb_right;  
       else if (!node->rb_right) //删除结点无右子树  
           child = node->rb_left;  
       else //左右子树都有  
       {  
           struct rb_node *old = node, *left;  
     
           node = node->rb_right;  
           while ((left = node->rb_left) != NULL)  
               node = left;  
     
           if (rb_parent(old)) {  
               if (rb_parent(old)->rb_left == old)  
                   rb_parent(old)->rb_left = node;  
               else  
                   rb_parent(old)->rb_right = node;  
           } else  
               root->rb_node = node;  
     
           child = node->rb_right;  
           parent = rb_parent(node);  
           color = rb_color(node);  
     
           if (parent == old) {  
               parent = node;  
           } else {  
               if (child)  
                   rb_set_parent(child, parent);  
               parent->rb_left = child;  
     
               node->rb_right = old->rb_right;  
               rb_set_parent(old->rb_right, node);  
           }  
     
           node->rb_parent_color = old->rb_parent_color;  
           node->rb_left = old->rb_left;  
           rb_set_parent(old->rb_left, node);  
     
           goto color;  
       }  //end else  
     
       parent = rb_parent(node); //获得删除结点的双亲结点  
       color = rb_color(node); //获取删除结点的颜色属性  
     
       if (child)  
           rb_set_parent(child, parent);  
       if (parent)  
       {  
           if (parent->rb_left == node)  
               parent->rb_left = child;  
           else  
               parent->rb_right = child;  
       }  
       else  
           root->rb_node = child;  
     
    color:  
       if (color == RB_BLACK) //如果删除结点的颜色属性为黑色，则需调用__rb_erase_color函数来进行调整  
           __rb_erase_color(child, parent, root);  
   } 

   static void rbtree_delete_fixup(RBRoot *root, Node *node, Node *parent)
   {
       Node *other;

       while ((!node || rb_is_black(node)) && node != root->node)
       {
           if (parent->left == node)
           {
               other = parent->right;
               if (rb_is_red(other))
               {
                   // Case 1: x的兄弟w是红色的  
                   rb_set_black(other);
                   rb_set_red(parent);
                   rbtree_left_rotate(root, parent);
                   other = parent->right;
               }
               if ((!other->left || rb_is_black(other->left)) &&
                   (!other->right || rb_is_black(other->right)))
               {
                   // Case 2: x的兄弟w是黑色，且w的俩个孩子也都是黑色的  
                   rb_set_red(other);
                   node = parent;
                   parent = rb_parent(node);
               }
               else
               {
                   if (!other->right || rb_is_black(other->right))
                   {
                       // Case 3: x的兄弟w是黑色的，并且w的左孩子是红色，右孩子为黑色。  
                       rb_set_black(other->left);
                       rb_set_red(other);
                       rbtree_right_rotate(root, other);
                       other = parent->right;
                   }
                   // Case 4: x的兄弟w是黑色的；并且w的右孩子是红色的，左孩子任意颜色。
                   rb_set_color(other, rb_color(parent));
                   rb_set_black(parent);
                   rb_set_black(other->right);
                   rbtree_left_rotate(root, parent);
                   node = root->node;
                   break;
               }
           }
           else
           {
               other = parent->left;
               if (rb_is_red(other))
               {
                   // Case 1: x的兄弟w是红色的  
                   rb_set_black(other);
                   rb_set_red(parent);
                   rbtree_right_rotate(root, parent);
                   other = parent->left;
               }
               if ((!other->left || rb_is_black(other->left)) &&
                   (!other->right || rb_is_black(other->right)))
               {
                   // Case 2: x的兄弟w是黑色，且w的俩个孩子也都是黑色的  
                   rb_set_red(other);
                   node = parent;
                   parent = rb_parent(node);
               }
               else
               {
                   if (!other->left || rb_is_black(other->left))
                   {
                       // Case 3: x的兄弟w是黑色的，并且w的左孩子是红色，右孩子为黑色。  
                       rb_set_black(other->right);
                       rb_set_red(other);
                       rbtree_left_rotate(root, other);
                       other = parent->left;
                   }
                   // Case 4: x的兄弟w是黑色的；并且w的右孩子是红色的，左孩子任意颜色。
                   rb_set_color(other, rb_color(parent));
                   rb_set_black(parent);
                   rb_set_black(other->left);
                   rbtree_right_rotate(root, parent);
                   node = root->node;
                   break;
               }
           }
       }
       if (node)
           rb_set_black(node);
   }
   ```


---



## 内存管理

### 页

页是内存管理的基本单位。大多数32位体系支持4KB的页，64位体系支持8KB的页。系统通过`struct page{}`结构管理页。虚拟地址被划分为固定大小的组，就是页。连续的页内部地址映射到连续的物理地址中。通过`cr3`控制寄存器指向第一级页地址

32位系统使用二级页表(10+10+12)来完成映射就够了，可以减少每个进程页表所需的RAM数量。但是对于64位来说就不行了。为了兼容，linux从2.6.11开始一直使用了4级分页(9+9+9+9+12)模型。具体的与计算机系统的体系结构有关。每个进程有自己的页集，在进程切换时，cr3的值会被保存。

内存的部分被保留为硬件，以及内核代码，存放静态内核数据结构。其余就是动态内存部分。

- 页全局目录（PGD）
- 页上级目录(PUD)
- 页中间目录(PMD)
- 页表(PT)

页框：物理内存（RAM）被分成固定大小的物理页。一个页框包含一个页。

页：线性地址（虚拟地址）。通过页表映射到页框。

### 区

由于硬件限制，内核对页并不是一视同仁的，而是把页划分为不同的区(zone)。

- ZONE_DMA:直接内存访问操作的区
- ZONE_DMA32:32位上才能使用的dma，（2.6以后已经没有了）
- ZONE_NORMAL:能正常映射的页
- ZONE_HIGHEM:包含“高端内存”，其中的页并不能永久地映射到内核地址空间。（在64位上是空的，因为能够使用的虚拟地址总是远远大于能安装的内存大小。32位中的内存大于可使用的地址空间时就会产生高端内存）高端内存的永久映射（kmap）不在使用时应当释放，临时映射（kmap_atomic）。

### 获得页

内核提供了请求内存的底层机制，并提供了对它进行访问的几个接口。所有这些接口以页位单位分配内存。最核心的函数，从分区页框分配器中获得页框：

```c
struct page *alloc_pages(gfp_t gfp_mask,unsinged int order);
//分配2^order个连续的物理页
gfp_mask:分配器标志，在低级页分配函数和kmalloc函数中都会用到，用来标识不同的分配行为，区，以及相关的类型等等。
```

将给定的页转化为逻辑地址

```c
void *page_address(struct page *page)
```

如果页是给用户空间使用的，那么应该使用`unsigned long get_zeroed_page(unsigned int gfp_mask)`，将分配好的页都置为0，避免其中可能`随机`地包含一些敏感数据

### KMALLOC

对于一般获取内存的函数为`kmalloc`，以字节为单位分配，与用户空间的`malloc`类似，多了一个flag参数、

```c 
void * kmalloc(size_t size,gfp_t flags)
```

对应的释放内存的函数为`kfree`，只能释放由kmalloc申请的函数，否则会有严重的后果。

`kmalloc`确保分配的内存在物理上是连续的，自然的在虚拟地址上也是连续的

`vmalloc`函数的工作方式类似于`kmalloc`函数，不过v是分配连续的虚拟内存地址，并不一定保证物理内存地址也是连续的。同`malloc`。对应的有`vfree`.

但是`vmalloc`为了将非连续的物理地址转换为连续的虚拟地址，就必须建立专门的页表项，而且因为其不是连续的物理地址，通过其获得的页也必须一个一个的进行映射，会导致`TLB（一个用来缓存虚拟地址到物理地址的映射关系的硬缓冲区）`抖动。处于性能考虑，一般是用`kmalloc`。

大多数情况下，硬件设备才会需要连续的物理地址。

### 外碎片(Buddy memory allocation)

频繁的请求和释放不同大小的一组连续页框，会导致已分配页框中分散了许多小块的空闲页框，进而带来问题：即是有足够大小的空闲页框可以满足请求，但是也要分配一个大块的连续页框就无法满足了。

- 外部碎片:指的是还没有被分配出去（不属于任何进程），但由于太小了无法分配给申请内存空间的新进程的内存空闲区域。
- 内部碎片就是已经被分配出去（能明确指出属于哪个进程）却不能被利用的内存空间；

linux采用伙伴系统（`Buddy memory allocation`）解决问题：

将空闲页框分组为11个块链表：1，2，4，8，16，… ，1024。最大请求对应着4MB的连续RAM块。采用最佳适应思想。

比如：需要256个页框，那么就看256的链表是否有，没有就看512，将其分为两个256，将剩余的加入到256的链表，向上查找知道1024，没有就报错。

释放：有两个相同大小 b ，物理地址连续，并且第一块的第一个页框地址是`2 * b * 4K`的倍数,就将其合并。

优点：

​	较低的外部碎片，针对较大内存分配设计，

缺点：

​	内部碎片，如果所需内存不是2的幂，会有内部碎片。合并要求较为严格

伙伴算法以页框为单位，适合较大内存的分配，针对于一个page的内存分配，有slab和kmem_cache等方法。

### SLAB层

分配和释放数据结构是内核最普遍的操作之一，常常会用到空闲链表，其包含了可供使用的，已经分配好的数据结构块。当代码需要一个新的数据结构实例时，就可以从空闲链表中抓取一个，而不需要分配内存再将数据放进去。不需要这个实例的时候就将其放回，而不是释放。

空闲链表的问题是不能全局控制，比如当内存紧张的时候，内核无法通知每个空闲链表收缩缓存释放空间。实际上，内核也并不是知道有空闲链表的存在。这就是slab层出现的理由。

slab通常扮演数据结构缓存层的角色。

#### slab层的设计

设计原则：

1. 频繁使用的数据结构也会频繁的分配和释放,所以应该特么的缓存它们.
2. 频繁分配和释放势必会导致内存碎片,(难以找到大块的内存).为了避免,空闲链表的缓存会连续的存放.因为以释放的数据结构又会放回空闲链表,因此不会导致碎片,
3. 回收的对象可以立即投入到下一次的分配中,因此频繁分配和释放的数据结构使用空链表可以提高其性能.
4. 如果slab分配器知道对象数据结构,页大小还有缓冲区的大小的话,会做出更加明智的决策.
5. 如果让部分缓存专属于特定的CPU处理器,那么就特么的不用考虑SMP,更不用考虑加锁了.
6. 对存放的对象进行着色,以防止多个对象映射到相同的高速缓存行

slab层主要起到了两个方面的作用：

1. slab可以对小对象进行分配，这样就不用为每个小对象分配一个页框，节省了空间。
2. 内核中的一些小对象创建析构很频繁，slab对这些小对象做了缓存，可以重复利用一些相同的对象，减少内存分配次数。

不同的对象划分为高速缓存组，然后又被划分为slab（slab由一个或多个物理连续的页组成），每个slab包含一些对象成员，即是一些被缓存的数据结构。

![](https://ws3.sinaimg.cn/large/006tNc79ly1fgsnsv4gagj30f60cfabf.jpg)

主要就是高速缓存（kmem_cache结构）—>slab(三个状态的链表：full，partial，free).—>每个对象

#### slab着色

同一硬件高速缓存行可以映射RAM中很多不同的块。相同大小的对象倾向于存放在高速缓存内相同的偏移处。最终不同的slab内具有相同偏移量的对象很可能映射在同一高速缓存行中。高速缓存的硬件可能因此而花费内存周期在同一高速缓存行与RAM内存单元之间来往传送两个对象，而其他的高速缓存行并未充分使用。

slab分配器通过着色降低这种行为发生的可能，将叫做颜色的不同随机数分配给slab。

### 内存池

内存池允许一个内核成分（内存池的拥有者）仅在内存不足的紧急情况下分配一些动态内存使用。当动态内存西游以至于普通内存请求失败，将会调用内存池作为最后的手段。

一个内存池常常叠加在slab分配器上===也就是说，它被用来保存slab对象的存储。一般而言，它能备用来分配任何一种类型的动态内存，从整个页框到使用kmalloc分配的小内存区。



### 进程地址空间（虚拟内存）

内核除了管理自己本身的内存，还需要管理用户空间中进程的内存，称之为进程地址空间，linux中使用`虚拟内存`，也就是说，系统中的进程已虚拟方式共享内存。

内存描述符为`mm_struct`的结构。

创建进程地址空间也就是创建进程(`fork(),clone(),vfork()等系统调用`)，内核的实现函数是`copy_mm()`函数。

释放则是由`exit_mm`函数实现。

`进程地址空间`由进程可寻址的虚拟内存组成。每个进程都有32或者64位的平坦（flat）的地址空间（指独立连续的区间）。通常情况下，每个进程有唯一地这种平坦的地址空间，而如果一个进程的地址空间与另一个进程有相同的`内存地址`，实际上也互不相干。而这种进程我们称之为线程。

`内存地址`在地址空间范围内的一个值，尽管一个进程可以寻址整个大小的虚拟内存，但是并不代表它有权利访问。通常只有一段可以被合法访问的虚拟内存的地址区间，称之为`内存区域`.通过内核，进程可以给自己的地址空间动态添加或者减少。

如果一个进程访问了不在有效范围内的内存地址，或者不正确的访问了有效地址，就是`段错误`

进程创建加载的时候，自身感知获得到了一个连续的内存地址空间，而实际上内核只是分配了一个逻辑上的虚拟内存空间，并且对虚拟内存和磁盘通过`mmap`做映射关系，对虚拟内存和物理内存做映射关系；等到程序真正运行的时候，需要某些数据，并且不在物理内存中，才会触发缺页异常，进行数据拷贝；

Linux进程地址空间包含以下几个部分（包括了c程序的存储空间布局）：

1. 代码段：指令；
2. 数据段：初始化的全局变量；
3. BSS段：未初始化的全局变量；
4. 堆：malloc动态分配内存使用堆，从低地址向高地址增长；
5. 共享库和共享内存：加载共享库和使用mmap共享内存；
6. 栈：自动变量和函数使用栈，从高地址向低地址增长；

内存描述符由`mm_struct`结构表示。对应着进程地址空间中的唯一区间。

#### malloc，mmap，用户空间分配内存

在linux系统上，程序被载入内存时，内核为用户进程地址空间建立了代码段、数据段和堆栈段，在数据段与堆栈段之间的空闲区域用于动态内存分配。

内核数据结构mm_struct中的成员变量`start_code`和`end_code`是进程代码段的起始和终止地址，`start_data`和 `end_data`是进程数据段的起始和终止地址，`start_stack`是进程堆栈段起始地址，`start_brk`是进程动态内存分配起始地址（堆的起始地址），还有一个` brk`（堆的当前最后地址），就是动态内存分配当前的终止地址。

c的动态内存分配基本函数是malloc()，在Linux上的基本实现是通过内核的brk系统调用。brk()是一个非常简单的系统调用，只是简单地改变mm_struct结构的成员变量brk的值。

mmap系统调用实现了更有用的动态内存分配功能，可以将一个磁盘文件的全部或部分内容映射到用户空间中，进程读写文件的操作变成了读写内存的操作。在 linux/mm/mmap.c文件的do_mmap_pgoff()函数，是mmap系统调用实现的核心。do_mmap_pgoff()的代码，只是新建了一个`vm_area_struct`结构，并把file结构的参数赋值给其成员变量m_file，并没有把文件内容实际装入内存。

#### 虚拟内存区域（VMA）

内核区域有`vm_area_struct`结构体描述，它描述了指定地址空间内连续区间上的一个独立内存范围。进程的地址空间是由允许进程使用的全部线性地址组成。每个进程各不相同。内核通过所谓`线性区（也就是VMA）`来表示线性地址区间，它是由一些起始地址、长度和一些访问权限描述。

进程所拥有的线性区从不重叠，本身所拥有的线性区通过链表连接。但是如果面对数据库等这种的应用，可能会有大量的线性区使用，故而会使用红黑树对内存描述符进行管理。

获得新线性区：

- 执行一条shell命令
- exec装载新的完全不同的程序
- 对一个文件或者一部分进行内存映射
- 堆栈用完了，需要扩展
- IPC共享线性区来与其他进程共享数据

分配线性区：

do_mmap()函数为当前进程创建并初始化一个线性区。分配成功后，可以将这个与其他线性区合并（与已有的区间相邻并且具有相同的访问权限就会合并）。用户空间通过系统调用mmap()函数获取do_mmap()的功能。

对应的删除地址空间就是：munmap()和do_munmap()函数。

### 缺页处理程序

分为两个情况：

- 编程错误引起的异常
- 引用属于进程地址空间但还尚未分配物理页框的页所引起的异常

缺页异常由`do_page_fault()`函数处理，会用到线性区描述符

处理流程：

1. 读取线性地址，判断是否在内核空间（address >= TASK_SIZE），也就是判断是不是在第4个GB
2. 如果试图访问不存在的页框引起异常，转去执行`vmalloc_falut`，这是可能由于内核态访问非连续内存引起（比如使用了`vmalloc`分配内存）
3. 否则，跳转至`bad_area_nosemaphore`,处理地址空间以外的错误情况，如果发生在用户态，发送SIGSEGV并结束。
4. 地址在“虚存区间”中，但`虚存区间`的访问权限不够；例如“区间”是只读的，而你想写，段错误
5. 权限也够了，但是映射关系没建立；先建立映射关系再说。
6. 映射关系也建立了，但是页面不在内存中。肯定是换出到交换分区中了，换进来再说，也就是内核分配一个新的页框并适当初始化----请求调页
7. 页面也在内存中。**但页面的访问权限不够**。例如页面是只读的，而你想写。**这通常就是 “写时拷贝COW” 的情况**-------内核分配一个新的页框，并把旧页框的数据拷贝到新页框并初始化它的内容。

参考链接：http://www.linuxidc.com/Linux/2016-01/127538.htm

---

## 文件管理

### 虚拟文件系统（VFS）

VFS使得用户可以直接使用`read(),write()`等操作而不需考虑具体的文件系统。是一个通用的文件系统接口。

它在底层文件系统上建立了一层抽象层。提供了一个通用文件系统模型。定义了所有文件系统都支持的、基本、概念上的接口和数据结构。而实际文件系统本身在‘文件’，‘目录’等概念上也保持一致，并且隐藏了具体的实现细节，并提供了接口和相关数据结构。

内核通过抽象层能够方便地、简单地支持各种文件系统，而实际文件系统也通过提供VFS所希望的接口和数据结构。

除了提供统一接口外，VFS还会将最近最常使用的目录项放到所谓目录项高速缓存的磁盘高速缓存中。

### VFS对象和数据结构

VFS是一个非常经典的具有面向对象思想的模式例子。通过使用结构体来实现对象，使用函数指针来实现操作。

VFS有4个主要的对象类型：

- 超级块对象，代表一个具体的已安装文件系统，用于存储特定文件系统的信息，通常对应存放在磁盘特定扇区中的文件系统超级块或文件系统控制块。
- 索引节点对象，代表一个具体的文件，包含内核在操作文件或目录时需要的全部信息。
- 目录项对象，代表一个目录项，是路径的一个组成部分，在`/bin/vi`中bin和vi都是作为文件对待，bin是目录文件，vi是普通文件，但是方便查找，引入了目录项`/,bin,vi`都属于目录项对象。目录项对象的读入与创建比较耗时，而且可能经常用到，故而使用目录项高速缓存将去缓存在内存中。
- 文件对象，代表由进程`打开`的文件。

三个进程打开文件示意图：

![](https://ws4.sinaimg.cn/large/006tNc79ly1fh25yik1ddj30es0a7t9h.jpg)

站在用户角度看，文件对象首先进入我们的视野，因为可以同时打开和操作同一文件，所以一个实际的文件存在多个文件对象，文件对象其实仅仅表示已经打开文件，反过来指向目录项对象（反过来指向索引节点对象），这两个肯定是唯一地。

与文件系统相关的数据结构：

- file_sysyem_type：用来描述各种特定文件系统类型
- vfsmount：描述安装文件系统的实例。

与进程相关的数据结构：

- file_struct，由进程描述符的files指向，所有与单个进程相关的信息
- fs_struct，由进程描述符的fs域指向，包含文件系统和进程相关的信息。
- namespace，使得每一个进程在系统中都看到唯一地安装文件系统---唯一的文件系统层次结构。

每个对象中包含一个操作对象，指向操作其父对象的函数指针。并且也是通过函数指针定义了很多方法。

类似的有libevent中根据不同的I/O多路复用定义的eventop

```c
event.c:定义了对象，它使用结构体，定义了需要的变量，成员函数等，就类似于c++等当中的对象。
在使用的时候，现根据系统判断出op，比如base_evsel = eventops[i],然后具体每个事件event e->op = base_op，然后既可以通过
e->op->add，等形式使用方法了。
struct eventop {
	const char *name;
	void *(*init)(struct event_base *);
	int (*add)(void *, struct event *);
	int (*del)(void *, struct event *);
	int (*dispatch)(struct event_base *, void *, struct timeval *);
	void (*dealloc)(struct event_base *, void *);
	/* set if we need to reinitialize the event base */
	int need_reinit;
};
#ifdef HAVE_EVENT_PORTS
extern const struct eventop evportops;
#endif
...
#ifdef HAVE_POLL
extern const struct eventop pollops;
#endif
...
#ifdef HAVE_EPOLL
extern const struct eventop epollops;

#endif

/* In order of preference */
static const struct eventop *eventops[] = {
#ifdef HAVE_EVENT_PORTS
	&evportops,
#endif
#ifdef HAVE_WORKING_KQUEUE
	&kqops,
#endif
#ifdef HAVE_EPOLL
	&epollops,
#endif
#ifdef HAVE_DEVPOLL
	&devpollops,
#endif
#ifdef HAVE_POLL
	&pollops,
#endif
#ifdef HAVE_SELECT
	&selectops,
#endif
#ifdef WIN32
	&win32ops,
#endif
	NULL
};

epoll.c：根据对象定义了具体的方法
 const struct eventop epollops = {
	"epoll",
	epoll_init,
	epoll_add,
	epoll_del,
	epoll_dispatch,
	epoll_dealloc,
	1 /* need reinit */
};
static epoll_init(struct event_base *) { 具体实现}
```

---

## 设备

块设备：能被`随机`（不需要按顺序）访问固定大小数据片的硬件设备，比如硬盘

字符设备：按照字符流有序访问，比如键盘，串口等。

两者区别主要在于是否能随机（无序访问）。

### 块I/O

当一个块被调用内存，会存在一个缓冲区中，每个缓冲区对应一个块。缓冲区头有buffer_struct结构体表示，其作为内核中I/O单元，但是有缺点：

- 很大且不易控制的结构体，对于数据的操作不方便和清晰
- 仅能描述单个缓冲区，当所有I/O的容器使用，缓冲区头会将大的I/O操作分割为多行操作

故而引入了bio结构体，代表了正在现场的（活动的）以片段（segment）链表形式组织的I/O操作。一个片段是一小块连续的内存缓冲区。所以并不需要缓冲区一定连续。

缓冲区头要求连续的存储区，关联着单独页中的单独磁盘块，所以可能割裂I/O操作

bio不需要分割I/O操作。

### I/O调度程序

I/O调度器是管理块设备的请求队列的。通过：合并和排序减少磁盘寻址时间。

Linus电梯：

1. 如果已经存在一个对相邻扇区的请求，将新请求与其合并—提高了吞吐量
2. 如果存在驻留时间过长的请求，新请求将会被插入到队列尾部，防止其他饥饿请求发生
3. 如果存在以扇区方向为序存在合适的插入位置，新的请求插入该位置，保证以物理磁盘访问为序进行排序
4. 不存在合适位置，插入到队列尾部。

缺陷：饥饿问题，对于一个磁盘同一位置上的请求流会造成其他较远位置的请求得不到运行机会。特别的，在写-饥饿-读，写一般是给内核有空时执行，与应用程序异步，而读一般是与应用程序同步，如果读产生饥饿就会特别影响性能和体验。

解决：

- 最终期限I/O：对于请求设定超时时间，并且读比写小很多。按照磁盘物理位置维护请求队列，根据请求类型另外插入到相应的队列，
- 完全公平排队：根据进程划分请求，每个进程有自己的队列，队列按照扇区方式分类，以时间片轮转调度。

### 中断

中断使得硬件得以发出通知给处理器。本质上是一个特殊的电信号。硬件设备的中断随时都可以发生。内核随时可能被新到来的中断打断。

异常：与中断不同，它是由处理器自己产生的，比如在执行中的以外或者编程中的失误，应当需要考虑对处理器时钟的同步。又称为同步中断。比如软中断实现系统调用，也就是陷入内核，引起特殊的异常—系统调用处理程序。

中断方式与异常类似，不过是由硬件发生。硬件通过IRQ线与可编程中断控制器相连。可以有选择的禁止某条IRQ线，但是中断不会丢失，当其重新激活，中断信号会继续发送到CPU。

每个中断或者异常会引起一个内核控制路径，内核控制路径可以嵌套，一个中断可以被另一个中断处理程序中断（指令不会返回用户态，将一直在内核态执行）。中断处理程序必须永不阻塞，不能发生进程切换，因为内核控制路径的恢复所需信息都在内核态堆栈中，这个堆栈显然只能属于当前进程。

#### 异常处理

大部分异常由linux解释为出错条件。一个异常发生，内核就向引起异常的进程发送信号向他通知一个反常条件。另外情况：linux通过异常来管理硬件资源。1."device not available"；2.“page fault”异常，该异常推迟给进程分配新页框，直到不能推迟为止。

处理流程：

- 在内核堆栈中保存大部分寄存器的内容
- 通过C函数处理异常：通常会向进程发送信号，异常处理程序终止了，进程处理信号或者默认系统处理信号
- 通过ret_from_exception()退出。

#### 中断处理

中断处理依赖于中断类型：I/O中断、时钟中断、处理器间接中断。

I/O中断：一般而言，需要比较灵活的给多个设备提供服务。有可能几个设备共享同一个IRQ线。通过IRQ共享（每个中断服务例程都被执行，验证它的设备是否需要关注）或者IRQ动态分配（一个IRQ线只有在用户访问时才被分配，这样同一个IRQ向量可以由几个设备不同时刻使用）。

时钟中断一般当做I/O中断来处理

处理器间接中断：该中断不是通过IRQ传输，是作为信号直接放在连接所有CPU的本地总线上。

中断处理程序:

每个设备都有相应的中断处理程序。linux中，其就是c程序，不过声明类型不同`static irqreturn_t func(int irq,void *dev)`，并且存在于称之为中断上下文的特殊上下文中。—该上下文不可能被阻塞

中断处理被分为`上半部`和`下半部`。中断处理程序是上半部，接收到中断，立即执行，但是只做有严格时限的工作。允许稍后完成的工作会推迟到下半部去。

中断处理程序都是预先在内核中注册的回调函数。

在驱动程序中通过`requset_irq()`注册一个中断处理程序。

#### 中断上下文

复习进程上下文：内核所处的操作模式，此时内核代表进程执行--比如系统调用或运行内核进程。可以通过current宏关联当前进程。进程是以进程上下文连接到内核中，因此，进程上下文可以睡眠，也可以调用调度程序、

中断上下文不能睡眠，不能调用某些函数。有较为严格的时间限制，因为打断了其他代码。它有自己的栈。

#### 中断处理机制的实现

硬件产生中断---->中断控制器——>处理器——>处理器中断内核——>`do_IRQ()`——>判断是否有中断处理程序——(yes，no直接跳转到倒数第二步)>`handle_IRQ_event()`——>在该线上运行所有中断处理程序——>`ret_from_intr()`——>返回内核运行中断的代码。

### 下半部

下半部的运行机制有很多：2.6中提供了三个：软中断、tasklet和工作队列。

tasklet是在软中断之上实现的。中断上下文：表示内核正在执行一个中断处理程序或一个可延迟的函数。

软中断：

2.6中定义了有限个软中断（6种）。`HI_SORFTIRQ（0）、TIMER_..（1）、NET_TX_..、NET_RX_..、SCSI_..、TASKLET_..`所使用的数据结构是`softirq_vec`数组。它是在编译期间静态分配的，通常，首先分配索引（上面的0，1…，越低优先级越高），使用`open_softirq`注册软中断，然后中断处理程序会在退出前标记软中断，使其稍后被处执行，这被称为`除法软中断`。唤起后，会在`do_softirq`中执行，它会遍历每一个，调用它们的处理程序。

tasklet：建立在`HI..`和`TASKLET..`两个软中断上的。区别只在于HI先执行。所以tasklet本身也是软中断。

---

## 附录

### 内存对齐：

​	在现代计算机体系中，每次读写内存中数据，都是按字（word，4个字节，对于X86架构，系统是32位，数据总线和地址总线的宽度都是32位，所以最大的寻址空间为232 = 4GB（也 许有人会问，我的32位XP用不了4GB内存，关于这个不在本篇博文讨论范围），按A[31,30…2,1,0]这样排列，但是请注意为了CPU每次读写 4个字节寻址，A[0]和A[1]两位是不参与寻址计算的。）为一个块（chunks）来操作（而对于X64则是8个字节为一个快）。注意，这里说的 CPU每次读取的规则，并不是变量在内存中地址对齐规则。既然是这样的，如果变量在内存中存储的时候也按照这样的对齐规则，就可以加快CPU读写内存的速 度，当然也就提高了整个程序的性能，并且性能提升是客观，虽然当今的CPU的处理数据速度(是指逻辑运算等,不包括取址)远比内存访问的速度快，程序的执 行速度的瓶颈往往不是CPU的处理速度不够，而是内存访问的延迟，虽然当今CPU中加入了高速缓存用来掩盖内存访问的延迟，但是如果高密集的内存访问，一 种延迟是无可避免的，内存地址对齐会给程序带来了很大的性能提升。

对齐规则：

```c
a、一个char（占用1-byte）变量以1-byte对齐。
b、一个short（占用2-byte）变量以2-byte对齐。
c、一个int（占用4-byte）变量以4-byte对齐。
d、一个long（占用4-byte）变量以4-byte对齐。
e、一个float（占用4-byte）变量以4-byte对齐。
f、一个double（占用8-byte）变量以8-byte对齐。
g、一个long double（占用12-byte）变量以4-byte对齐。
h、任何pointer（占用4-byte）变量以4-byte对齐。

而在64位系统下，与上面规则对比有如下不同：

  a、一个long（占用8-byte）变量以8-byte对齐。
  b、一个double（占用8-byte）变量以8-byte对齐。
  c、一个long double（占用16-byte）变量以16-byte对齐。
  d、任何pointer（占用8-byte）变量以8-byte对齐

```



1. 数据成员对齐规则：结构(struct)(或联合(union))的数据成员，第一个数据成员放在offset为0的地方，以后每个数据成员存储的起始位置要从该成员大小或者成员的子成员大小（只要该成员有子成员，比如说是数组，结构体等）的整数倍开始(比如int在３２位机为４字节,则要从４的整数倍地址开始存储。
2. 结构体作为成员:如果一个结构里有某些结构体成员,则结构体成员要从其内部最大元素大小的整数倍地址开始存储.(struct a里存有struct b,b里有char,int ,double等元素,那b应该从8的整数倍开始存储.) 
3. 收尾工作:结构体的总大小,也就是sizeof的结果,.必须是其内部最大成员的整数倍.不足的要补齐.

eventfd

进程(线程)间的等待/通知(wait/notify) 机制。注意看看livevent中的通知机制，原本是通过pipe（管道只能用于父子进程之间?FIFO--命名管道可以用于任何进程间）进行写一个字节进行唤醒（？）

http://blog.chinaunix.net/uid-23146151-id-3069427.html