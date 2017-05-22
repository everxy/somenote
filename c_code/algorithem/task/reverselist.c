//反转单链表

#include <stdio.h>
#include <stdlib.h>

struct list {
    int val;
    struct list *next;
};

struct list *reavsrsel(struct list *l);
struct list *reavsrsel_2(struct list *l);
struct list *initlist();
int main(int argc, char const* argv[])
{
    struct list *l,*p;
    l = initlist(l);
    l = reavsrsel_2(l);
    p = l->next;
    while(p != NULL){
        printf("%d\t",p->val);
        p = p->next;
    }
    printf("\n");
    return 0;
}

struct list *initlist(struct list *l)
{
    struct list *p;
    struct list *temp;
    p = (struct list *)malloc(sizeof(struct list));
    l = p;
    for(int i = 1;i < 10 ;i++) {
        temp =(struct list *)malloc(sizeof(struct list));
        temp->val = i;
        p->next = temp;
        p = p->next;
    }
    p->next = NULL;
    return l;
}

/*
 *使用一个tenmp使得l头部插入
 * */
struct list *reavsrsel(struct list *l)
{
    struct list *ln,*p,*temp;
    temp = p = ln = l->next;
    while(p->next != NULL) {
        temp = p->next;
        l->next = temp;
        p->next = temp->next;
        temp->next = ln;
        ln = temp;
    }
    return l;
}


/*
 * 让链表的指向倒过来
 * */
struct list *reavsrsel_2(struct list *l)
{
    struct list *ln,*p,*temp;
    temp = NULL;
    ln = l;
    p = l->next;
    while(p != NULL ) {
        ln->next = temp;
        temp = ln;
        ln = p;
        p = p->next;
    }
    ln->next = temp;
    return ln;
}
