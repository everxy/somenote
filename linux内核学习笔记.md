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

   ​



